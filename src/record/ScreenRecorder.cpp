#include "ScreenRecorder.h"

#include "InputMonitor.h"
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
    m_capture = new QScreenCapture(this);
    m_capture->setScreen(screen);
    m_sink = new QVideoSink(this);
    m_source.setScreenCapture(m_capture);
    m_source.setVideoSink(m_sink);
    connect(m_sink, &QVideoSink::videoFrameChanged, this, &ScreenRecorder::onFrame);
    connect(m_capture, &QScreenCapture::errorOccurred, this,
            [this](QScreenCapture::Error, const QString &message) { fail(message); });

    m_frameTimer = new QTimer(this);
    m_frameTimer->setTimerType(Qt::PreciseTimer);
    m_frameTimer->setInterval(1000 / std::max(1, m_options.frameRate));
    connect(m_frameTimer, &QTimer::timeout, this, &ScreenRecorder::writeFrame);

    m_input = new QVideoFrameInput(this);
    m_recorder = new QMediaRecorder(this);
    m_output.setVideoFrameInput(m_input);
    m_output.setRecorder(m_recorder);
    QMediaFormat format(QMediaFormat::MPEG4);
    format.setVideoCodec(QMediaFormat::VideoCodec::H264);
    m_recorder->setMediaFormat(format);
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
    m_recorder->record();
    m_capture->setActive(true);
    m_frameTimer->start();
}

void ScreenRecorder::stop()
{
    if (m_stopping)
        return;
    m_stopping = true;
    m_frameTimer->stop();
    m_capture->setActive(false);
    if (m_inputMonitor)
        m_inputMonitor->stop();
    m_recorder->stop();
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
    m_capture->setActive(false);
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

void ScreenRecorder::onFrame(const QVideoFrame &frame)
{
    if (m_stopping || m_failed || !m_screen || !frame.isValid())
        return;
    QImage image = frame.toImage();
    if (image.isNull())
        return;
    const QRect screenRect = m_screen->geometry();
    m_scale = qreal(image.width()) / screenRect.width();

    // H.264 (4:2:0) needs even dimensions.
    QRect crop = QRectF(QPointF(m_rect.topLeft() - screenRect.topLeft()) * m_scale, QSizeF(m_rect.size()) * m_scale)
                     .toAlignedRect()
                     .intersected(image.rect());
    crop.setWidth(crop.width() & ~1);
    crop.setHeight(crop.height() & ~1);
    // ARGB32 is what the encoder path takes as is; RGB32 frames came out black.
    m_area = image.copy(crop).convertToFormat(QImage::Format_ARGB32);
    m_areaOrigin = crop.topLeft();
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

    QVideoFrame video(out);
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
