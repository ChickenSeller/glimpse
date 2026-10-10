#include "ScreenRecorder.h"

#include "InputMonitor.h"
#ifdef GLIMPSE_HAVE_PIPEWIRE
#include "ScreenCastCapture.h"
#endif
#include "capture/CursorCapture.h"
#include "capture/Platform.h"

#include <QCursor>
#include <QMediaFormat>
#include <QMediaRecorder>
#include <QPainter>
#include <QPainterPath>
#include <QScreen>
#include <QScreenCapture>
#include <QTimer>
#include <QUrl>
#include <QVideoFrame>
#include <QVideoFrameInput>
#include <QVideoSink>

#include <algorithm>
#include <cmath>

namespace {

constexpr qint64 kRippleMs = 500;
constexpr qint64 kKeyShowMs = 2000;
constexpr int kMaxKeys = 4;

const QColor kHighlight(255, 214, 0, 80);
const QColor kLeftClick(229, 57, 53);
const QColor kRightClick(30, 136, 229);
const QColor kMiddleClick(67, 160, 71);

#ifndef Q_OS_WIN
// BT.709, limited range.
uchar lumaOf(int r, int g, int b)
{
    return uchar(16 + ((47 * r + 157 * g + 16 * b + 128) >> 8));
}

uchar cbOf(int r, int g, int b)
{
    return uchar(128 + ((-26 * r - 86 * g + 112 * b + 128) >> 8));
}

uchar crOf(int r, int g, int b)
{
    return uchar(128 + ((112 * r - 102 * g - 10 * b + 128) >> 8));
}

// From RGB frames FFmpeg encodes H.264 High 4:4:4 (12-bit for h264_nvenc,
// which NVENC then rejects on every frame); many players and most hardware
// decoders show 4:4:4 as black. 4:2:0 frames get plain High 4:2:0.
//
// 4:2:0 keeps one color per 2x2 pixels, which smears thin colored lines
// (charts, syntax highlighting). With `doubled`, the frame is twice the
// size: every pixel becomes 2x2 luma samples with a color sample of its own,
// as exact as 4:4:4 and still plain 4:2:0. The image has even dimensions.
QVideoFrame toYuv420(const QImage &image, bool doubled)
{
    const int scale = doubled ? 2 : 1;
    QVideoFrameFormat format(image.size() * scale, QVideoFrameFormat::Format_YUV420P);
    format.setColorSpace(QVideoFrameFormat::ColorSpace_BT709);
    format.setColorTransfer(QVideoFrameFormat::ColorTransfer_BT709);
    format.setColorRange(QVideoFrameFormat::ColorRange_Video);
    QVideoFrame frame(format);
    if (!frame.map(QVideoFrame::WriteOnly))
        return frame;
    uchar *const yPlane = frame.bits(0);
    uchar *const uPlane = frame.bits(1);
    uchar *const vPlane = frame.bits(2);
    const int yStride = frame.bytesPerLine(0);
    const int uStride = frame.bytesPerLine(1);
    const int vStride = frame.bytesPerLine(2);
    if (doubled) {
        for (int y = 0; y < image.height(); ++y) {
            const QRgb *row = reinterpret_cast<const QRgb *>(image.constScanLine(y));
            uchar *yRows[2] = {yPlane + 2 * y * yStride, yPlane + (2 * y + 1) * yStride};
            uchar *u = uPlane + y * uStride;
            uchar *v = vPlane + y * vStride;
            for (int x = 0; x < image.width(); ++x) {
                const int r = qRed(row[x]), g = qGreen(row[x]), b = qBlue(row[x]);
                const uchar luma = lumaOf(r, g, b);
                yRows[0][2 * x] = yRows[0][2 * x + 1] = yRows[1][2 * x] = yRows[1][2 * x + 1] = luma;
                u[x] = cbOf(r, g, b);
                v[x] = crOf(r, g, b);
            }
        }
    } else {
        for (int y = 0; y < image.height(); y += 2) {
            const QRgb *rows[2] = {reinterpret_cast<const QRgb *>(image.constScanLine(y)),
                                   reinterpret_cast<const QRgb *>(image.constScanLine(y + 1))};
            uchar *yRows[2] = {yPlane + y * yStride, yPlane + (y + 1) * yStride};
            uchar *u = uPlane + y / 2 * uStride;
            uchar *v = vPlane + y / 2 * vStride;
            for (int x = 0; x < image.width(); x += 2) {
                int r = 0, g = 0, b = 0; // averaged over the 2x2 block
                for (int dy = 0; dy < 2; ++dy) {
                    for (int dx = 0; dx < 2; ++dx) {
                        const QRgb p = rows[dy][x + dx];
                        yRows[dy][x + dx] = lumaOf(qRed(p), qGreen(p), qBlue(p));
                        r += qRed(p);
                        g += qGreen(p);
                        b += qBlue(p);
                    }
                }
                u[x / 2] = cbOf((r + 2) / 4, (g + 2) / 4, (b + 2) / 4);
                v[x / 2] = crOf((r + 2) / 4, (g + 2) / 4, (b + 2) / 4);
            }
        }
    }
    frame.unmap();
    return frame;
}
#endif

QColor clickColor(Qt::MouseButton button)
{
    switch (button) {
    case Qt::RightButton: return kRightClick;
    case Qt::MiddleButton: return kMiddleClick;
    default: return kLeftClick;
    }
}

} // namespace

ScreenRecorder::ScreenRecorder(QScreen *screen, const QRect &logicalRect, const Options &options, QObject *parent)
    : QObject(parent)
    , m_screen(screen)
    , m_rect(logicalRect.intersected(screen->geometry()))
    , m_options(options)
{
#ifdef GLIMPSE_HAVE_PIPEWIRE
    // With the pointer in the frames, which QScreenCapture leaves out there.
    if (Platform::displayServer() == Platform::DisplayServer::Wayland) {
        m_screenCast = new ScreenCastCapture(this);
        connect(m_screenCast, &ScreenCastCapture::frameReady, this, &ScreenRecorder::onImage);
        connect(m_screenCast, &ScreenCastCapture::failed, this, &ScreenRecorder::fail);
    }
#endif
    if (!m_screenCast) {
        m_capture = new QScreenCapture(this);
        m_capture->setScreen(screen);
        m_sink = new QVideoSink(this);
        m_source.setScreenCapture(m_capture);
        m_source.setVideoSink(m_sink);
        connect(m_sink, &QVideoSink::videoFrameChanged, this, &ScreenRecorder::onFrame);
        connect(m_capture, &QScreenCapture::errorOccurred, this,
                [this](QScreenCapture::Error, const QString &message) { fail(message); });
    }

    m_frameTimer = new QTimer(this);
    m_frameTimer->setTimerType(Qt::PreciseTimer);
    m_frameTimer->setInterval(1000 / std::max(1, m_options.frameRate));
    connect(m_frameTimer, &QTimer::timeout, this, &ScreenRecorder::writeFrame);

    m_input = new QVideoFrameInput(this);
    m_recorder = new QMediaRecorder(this);
    m_output.setVideoFrameInput(m_input);
    m_output.setRecorder(m_recorder);
    m_recorder->setQuality(QMediaRecorder::HighQuality);
    m_recorder->setVideoFrameRate(m_options.frameRate);
    m_recorder->setOutputLocation(QUrl::fromLocalFile(m_options.filePath));
    connect(m_recorder, &QMediaRecorder::errorOccurred, this,
            [this](QMediaRecorder::Error, const QString &message) { fail(message); });
    connect(m_recorder, &QMediaRecorder::recorderStateChanged, this, [this](QMediaRecorder::RecorderState state) {
        if (state == QMediaRecorder::StoppedState && m_stopping && !m_failed)
            emit finished(m_recorder->actualLocation().toLocalFile());
    });

    if (m_options.showClicks || m_options.showKeys) {
        m_inputMonitor = InputMonitor::create(this);
        connect(m_inputMonitor, &InputMonitor::mouseButton, this, [this](Qt::MouseButton button, bool pressed) {
            if (!m_options.showClicks || !m_screen)
                return;
            m_buttonsDown.setFlag(button, pressed);
            if (pressed) {
                const qreal scale = m_screen->devicePixelRatio();
                m_clicks.append({cursorPos(scale), button, m_clock.elapsed()});
            }
        });
        connect(m_inputMonitor, &InputMonitor::keyState, this, [this](int virtualKey, bool pressed) {
            if (m_options.showKeys)
                m_keyboard.setKey(virtualKey, pressed, m_clock.elapsed());
        });
        connect(m_inputMonitor, &InputMonitor::keyPressed, this, [this](const QString &text) {
            if (!m_options.showKeys)
                return;
            // A repeated combination counts up instead of piling up.
            if (!m_keys.isEmpty() && m_keys.last().text == text) {
                m_keys.last().count++;
                m_keys.last().at = m_clock.elapsed();
            } else {
                m_keys.append({text, 1, m_clock.elapsed()});
                if (m_keys.size() > kMaxKeys)
                    m_keys.removeFirst();
            }
        });
    }
}

ScreenRecorder::~ScreenRecorder()
{
    if (m_inputMonitor)
        m_inputMonitor->stop();
}

void ScreenRecorder::start()
{
    m_clock.start();
    if (m_inputMonitor)
        m_inputMonitor->start();
#ifdef Q_OS_WIN
    beginRecording();
#endif
    setCaptureActive(true);
    m_frameTimer->start();
}

void ScreenRecorder::stop()
{
    if (m_stopping)
        return;
    m_stopping = true;
    m_frameTimer->stop();
    setCaptureActive(false);
    if (m_inputMonitor)
        m_inputMonitor->stop();
    if (!m_recording) {
        // No screen content ever came (the portal was still asking).
        fail(tr("Nothing was recorded: the screen content did not arrive."));
        return;
    }
    m_recorder->stop();
}

void ScreenRecorder::beginRecording()
{
    m_recording = true;
    QMediaFormat format(QMediaFormat::MPEG4);
    format.setVideoCodec(QMediaFormat::VideoCodec::H264);
#ifndef Q_OS_WIN
    // Twice the size: H.264 encoders take at most 4096 pixels a side, H.265
    // ones 8192 (NVENC, VA-API).
    const QSize doubled = m_area.size() * 2;
    m_doubled = m_options.fullColor && doubled.width() <= 8192 && doubled.height() <= 8192;
    if (m_doubled && (doubled.width() > 4096 || doubled.height() > 4096))
        format.setVideoCodec(QMediaFormat::VideoCodec::H265);
#endif
    m_recorder->setMediaFormat(format);
    m_recorder->record();
}

void ScreenRecorder::setPaused(bool paused)
{
    if (paused == m_paused)
        return;
    m_paused = paused;
    if (paused) {
        m_pausedSince = m_clock.elapsed();
    } else {
        m_pausedTotal += m_clock.elapsed() - m_pausedSince;
        m_pausedSince = -1;
    }
}

qint64 ScreenRecorder::durationMs() const
{
    if (!m_clock.isValid())
        return 0;
    const qint64 now = m_paused ? m_pausedSince : m_clock.elapsed();
    return now - m_pausedTotal;
}

void ScreenRecorder::fail(const QString &message)
{
    if (m_failed)
        return;
    m_failed = true;
    m_frameTimer->stop();
    setCaptureActive(false);
    if (m_inputMonitor)
        m_inputMonitor->stop();
    emit failed(message);
}

QPoint ScreenRecorder::cursorPos(qreal scale) const
{
    // Exact physical position where the platform reports it.
    if (const std::optional<QPoint> native = Platform::nativeCursorPos()) {
        const QPoint origin(qRound(m_screen->geometry().x() * scale), qRound(m_screen->geometry().y() * scale));
        return *native - origin;
    }
    const QPointF local = QPointF(QCursor::pos(m_screen) - m_screen->geometry().topLeft()) * scale;
    return local.toPoint();
}

void ScreenRecorder::setCaptureActive(bool active)
{
    if (m_capture)
        m_capture->setActive(active);
#ifdef GLIMPSE_HAVE_PIPEWIRE
    if (m_screenCast) {
        if (active)
            m_screenCast->start();
        else
            m_screenCast->stop();
    }
#endif
}

void ScreenRecorder::onFrame(const QVideoFrame &frame)
{
    if (frame.isValid())
        onImage(frame.toImage());
}

void ScreenRecorder::onImage(const QImage &image)
{
    if (m_stopping || m_failed || !m_screen || image.isNull())
        return;
    const QRect screenRect = m_screen->geometry();
    m_scale = qreal(image.width()) / screenRect.width();

    // H.264 (4:2:0) needs even dimensions.
    const QRectF area(QPointF(m_rect.topLeft() - screenRect.topLeft()) * m_scale, QSizeF(m_rect.size()) * m_scale);
#ifdef Q_OS_WIN
    QRect crop = area.toAlignedRect().intersected(image.rect());
#else
    // Only pixels wholly inside the area: the red frame, which cannot be
    // kept out of the capture here, sits right outside it, and a scaled frame
    // window (XWayland at 150%) blends into the pixels it partly covers.
    QRect crop = QRect(QPoint(int(std::ceil(area.left())), int(std::ceil(area.top()))),
                       QPoint(int(std::floor(area.right())) - 1, int(std::floor(area.bottom())) - 1))
                     .intersected(image.rect());
#endif
    crop.setWidth(crop.width() & ~1);
    crop.setHeight(crop.height() & ~1);
    // ARGB32 is what the encoder path takes as is; RGB32 frames came out black.
    m_area = image.copy(crop).convertToFormat(QImage::Format_ARGB32);
    m_areaOrigin = crop.topLeft();
    // The area's size in physical pixels is only known now.
    if (!m_recording)
        beginRecording();
}

void ScreenRecorder::writeFrame()
{
    if (m_stopping || m_failed || m_paused || m_area.isNull())
        return;
    const qint64 now = durationMs();
    const qint64 frameMs = 1000 / std::max(1, m_options.frameRate);

    QImage out = m_area.copy();
    // The capture shows no pointer; paint the real one in.
    drawCursor(out, m_screen, m_areaOrigin);
    paintOverlays(out, m_areaOrigin, m_scale);

#ifdef Q_OS_WIN
    QVideoFrame video(out);
#else
    QVideoFrame video = toYuv420(out, m_doubled);
#endif
    video.setStartTime(now * 1000);
    video.setEndTime((now + frameMs) * 1000);
    if (m_input->sendVideoFrame(video))
        ++m_framesWritten;
    else
        ++m_framesDropped; // the encoder is behind; skip rather than queue
}

void ScreenRecorder::paintOverlays(QImage &image, const QPoint &cropOrigin, qreal scale)
{
    const qint64 now = m_clock.elapsed();
    QPainter p(&image);
    p.setRenderHint(QPainter::Antialiasing);
    const QPoint cursor = cursorPos(scale) - cropOrigin;

    if (m_options.highlightCursor) {
        const qreal radius = 22 * scale;
        p.setPen(Qt::NoPen);
        p.setBrush(kHighlight);
        p.drawEllipse(QPointF(cursor), radius, radius);
    }

    if (m_options.showClicks) {
        // A ring grows and fades from each press; a held button keeps a ring.
        m_clicks.removeIf([now](const Click &click) { return now - click.at > kRippleMs; });
        for (const Click &click : std::as_const(m_clicks)) {
            const qreal t = qreal(now - click.at) / kRippleMs;
            QColor color = clickColor(click.button);
            color.setAlphaF(0.9 * (1.0 - t));
            p.setPen(QPen(color, 3 * scale));
            p.setBrush(Qt::NoBrush);
            const qreal radius = (10 + 26 * t) * scale;
            p.drawEllipse(QPointF(click.pos - cropOrigin), radius, radius);
        }
        for (Qt::MouseButton button : {Qt::LeftButton, Qt::RightButton, Qt::MiddleButton}) {
            if (!m_buttonsDown.testFlag(button))
                continue;
            QColor color = clickColor(button);
            color.setAlphaF(0.85);
            p.setPen(QPen(color, 3 * scale));
            p.setBrush(Qt::NoBrush);
            p.drawEllipse(QPointF(cursor), 12 * scale, 12 * scale);
        }
    }

    if (m_options.showKeys) {
        const bool keyboard = m_options.keyStyle != KeyStyle::Labels;
        const bool labels = m_options.keyStyle != KeyStyle::Keyboard;
        int bottom = image.height() - qRound(16 * scale);
        if (keyboard) {
            // At most 60% of the width and a third of the height, and no
            // bigger than a comfortable size on a large recording.
            const qreal unit = std::min({image.width() * 0.6 / 19.1, image.height() / 3.0 / 7.1, 34.0 * scale});
            const QSize size = KeyboardOverlay::sizeFor(unit);
            const QRectF area((image.width() - size.width()) / 2.0, bottom - size.height(), size.width(), size.height());
            m_keyboard.paint(p, area, now);
            bottom = int(area.top()) - qRound(10 * scale);
        }
        m_keys.removeIf([now](const Key &key) { return now - key.at > kKeyShowMs; });
        if (labels && !m_keys.isEmpty()) {
            QStringList labels;
            for (const Key &key : std::as_const(m_keys))
                labels << (key.count > 1 ? QStringLiteral("%1 ×%2").arg(key.text).arg(key.count) : key.text);
            const QString text = labels.join(QStringLiteral("   "));
            QFont font = p.font();
            font.setPixelSize(qRound(22 * scale));
            font.setBold(true);
            p.setFont(font);
            const QFontMetrics fm(font);
            const QSize size(fm.horizontalAdvance(text) + qRound(32 * scale), fm.height() + qRound(16 * scale));
            QRect box(QPoint((image.width() - size.width()) / 2, bottom - size.height()), size);
            // Fades out with the newest key.
            const qreal age = qreal(now - m_keys.last().at) / kKeyShowMs;
            const qreal opacity = age < 0.75 ? 1.0 : std::max(0.0, (1.0 - age) / 0.25);
            p.setOpacity(opacity);
            p.setPen(Qt::NoPen);
            p.setBrush(QColor(0, 0, 0, 190));
            p.drawRoundedRect(box, 10 * scale, 10 * scale);
            p.setPen(Qt::white);
            p.drawText(box, Qt::AlignCenter, text);
            p.setOpacity(1.0);
        }
    }
}
