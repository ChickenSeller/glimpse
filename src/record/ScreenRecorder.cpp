#include "ScreenRecorder.h"

#include "AudioTrack.h"
#include "InputMonitor.h"
#include "MicrophoneCapture.h"
#include "SystemAudioCapture.h"
#ifdef GLIMPSE_HAVE_PIPEWIRE
#include "ScreenCastCapture.h"
#endif
#include "capture/CursorCapture.h"
#include "capture/Platform.h"

#include <QAudioBuffer>
#include <QAudioBufferInput>
#include <QAudioFormat>
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
#include <limits>
#include <vector>

namespace {

constexpr qint64 kRippleMs = 500;
constexpr qint64 kKeyShowMs = 2000;
constexpr int kMaxKeys = 4;

const QColor kHighlight(255, 214, 0, 80);
const QColor kBand(32, 32, 32);
const QColor kLeftClick(229, 57, 53);
const QColor kRightClick(30, 136, 229);
const QColor kMiddleClick(67, 160, 71);

// The sound is mixed this far behind the clock: the devices hand it over in
// chunks, some time after it was heard.
constexpr qint64 kAudioDelayMs = 150;
constexpr int kAudioIntervalMs = 20;

QAudioFormat audioFormat()
{
    QAudioFormat format;
    format.setSampleRate(AudioTrack::kRate);
    format.setChannelCount(AudioTrack::kChannels);
    format.setChannelConfig(QAudioFormat::ChannelConfigStereo);
    format.setSampleFormat(QAudioFormat::Float);
    return format;
}

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

    if (hasAudio()) {
        m_audioInput = new QAudioBufferInput(audioFormat(), this);
        m_output.setAudioBufferInput(m_audioInput);
        connect(m_audioInput, &QAudioBufferInput::readyToSendAudioBuffer, this, &ScreenRecorder::sendAudio);
        m_audioTimer = new QTimer(this);
        m_audioTimer->setInterval(kAudioIntervalMs);
        connect(m_audioTimer, &QTimer::timeout, this, [this] { mixAudio(false); });
    }

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
        connect(m_inputMonitor, &InputMonitor::keyState, this, [this](int key, bool pressed) {
            if (!m_options.showKeys)
                return;
            // The layout may be switched while recording, or differ per window.
            if (pressed)
                m_keyboard.setLayout(currentKeyboardLayout());
            m_keyboard.setKey(key, pressed, m_clock.elapsed());
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
    stopAudio();
}

void ScreenRecorder::start()
{
    // First: a microphone that cannot be opened stops the recording here.
    // The time it takes (seconds, for one that was idle) is not recorded.
    if (hasAudio() && !startAudio())
        return;
    m_clock.start();
    if (m_options.showKeys)
        m_keyboard.setLayout(currentKeyboardLayout());
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
    if (hasAudio()) {
        // The rest of the sound, up to now.
        stopAudio();
        mixAudio(true);
    }
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
    if (hasAudio())
        format.setAudioCodec(QMediaFormat::AudioCodec::AAC);
#ifndef Q_OS_WIN
    // Twice the size: H.264 encoders take at most 4096 pixels a side, H.265
    // ones 8192 (NVENC, VA-API).
    const QSize doubled = m_frameSize * 2;
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
        m_pauses.append({m_pausedSince, m_clock.elapsed()});
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
    stopAudio();
    emit failed(message);
}

bool ScreenRecorder::startAudio()
{
    QString error;
    if (m_options.microphone) {
        m_microphoneTrack = std::make_unique<AudioTrack>();
        m_microphone = new MicrophoneCapture(m_options.microphoneDevice, m_microphoneTrack.get(), this);
        if (!m_microphone->start(&error)) {
            fail(error);
            return false;
        }
    }
    if (m_options.systemAudio) {
        m_systemTrack = std::make_unique<AudioTrack>();
        m_systemAudio = std::make_unique<SystemAudioCapture>(m_systemTrack.get());
        if (!m_systemAudio->start(&error)) {
            fail(error);
            return false;
        }
    }
    return true;
}

void ScreenRecorder::stopAudio()
{
    if (m_audioTimer)
        m_audioTimer->stop();
    if (m_microphone)
        m_microphone->stop();
    if (m_systemAudio)
        m_systemAudio->stop();
}

void ScreenRecorder::mixAudio(bool all)
{
    if (m_audioMixed < 0)
        return;
    const qint64 now = AudioTrack::frameAt(m_clock);
    const qint64 to = all ? now : now - AudioTrack::kRate * kAudioDelayMs / 1000;
    const qint64 from = m_audioMixed;
    if (to <= from)
        return;
    std::vector<float> mix(size_t(to - from) * AudioTrack::kChannels, 0.0f);
    for (AudioTrack *track : {m_microphoneTrack.get(), m_systemTrack.get()}) {
        if (track)
            track->mixInto(mix.data(), from, to);
    }
    m_audioMixed = to;
    for (float &sample : mix)
        sample = std::clamp(sample, -1.0f, 1.0f);

    // Paused, the video stands still and its time does not run: the sound
    // of that time is left out.
    const auto frameOf = [](qint64 ms) { return ms * AudioTrack::kRate / 1000; };
    m_pauses.removeIf([&](const std::pair<qint64, qint64> &pause) { return frameOf(pause.second) <= from; });
    QList<std::pair<qint64, qint64>> pauses;
    for (const auto &[begin, end] : std::as_const(m_pauses))
        pauses.append({frameOf(begin), frameOf(end)});
    if (m_paused)
        pauses.append({frameOf(m_pausedSince), std::numeric_limits<qint64>::max()});
    qint64 at = from;
    const auto keepUntil = [&](qint64 until) {
        until = std::min(until, to);
        if (until > at) {
            m_audioPending.append(reinterpret_cast<const char *>(mix.data() + (at - from) * AudioTrack::kChannels),
                                  qsizetype((until - at) * AudioTrack::kChannels * sizeof(float)));
        }
    };
    for (const auto &[begin, end] : std::as_const(pauses)) {
        keepUntil(begin);
        at = std::max(at, end);
        if (at >= to)
            break;
    }
    keepUntil(to);
    sendAudio();
}

void ScreenRecorder::sendAudio()
{
    if (!m_audioInput || m_audioPending.isEmpty())
        return;
    const QAudioFormat format = audioFormat();
    const qint64 frames = m_audioPending.size() / format.bytesPerFrame();
    const QAudioBuffer buffer(m_audioPending, format, m_audioSent * 1000000 / AudioTrack::kRate);
    // Not taken while the encoder is busy: kept, and sent with what follows.
    if (m_audioInput->sendAudioBuffer(buffer)) {
        m_audioSent += frames;
        m_audioPending.clear();
    }
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
    // (On Windows the encoder starts with the recording, before any frame.)
    if (m_frameSize.isEmpty())
        layOutFrame();
    if (!m_recording)
        beginRecording();
}

void ScreenRecorder::writeFrame()
{
    if (m_stopping || m_failed || m_paused || m_area.isNull())
        return;
    // The video (and the sound) starts at 0 with the first frame, however
    // long the screen content took to come.
    if (m_videoOrigin < 0) {
        m_videoOrigin = durationMs();
        if (hasAudio()) {
            m_audioMixed = AudioTrack::frameAt(m_clock);
            for (AudioTrack *track : {m_microphoneTrack.get(), m_systemTrack.get()}) {
                if (track)
                    track->start(&m_clock, m_audioMixed);
            }
            m_audioTimer->start();
        }
    }
    const qint64 now = durationMs() - m_videoOrigin;
    const qint64 frameMs = 1000 / std::max(1, m_options.frameRate);

    QImage out = m_area.copy();
    // The capture shows no pointer; paint the real one in.
    drawCursor(out, m_screen, m_areaOrigin);
    paintPointer(out, m_areaOrigin, m_scale);
    if (m_options.showKeys) {
        if (m_band.isEmpty()) {
            paintKeys(out, out.rect(), m_scale);
        } else {
            QImage frame(m_frameSize, QImage::Format_ARGB32);
            frame.fill(kBand);
            QPainter p(&frame);
            p.drawImage(m_areaOffset, out);
            p.end();
            out = frame;
            paintKeys(out, m_band, m_scale);
        }
    }

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

bool ScreenRecorder::keysOutside() const
{
    return m_options.showKeys && m_options.keyPosition >= KeyPosition::OutsideBottom;
}

qreal ScreenRecorder::keyboardUnit(qreal scale) const
{
    const QSize area = m_area.size();
    switch (m_options.keyPosition) {
    case KeyPosition::OutsideBottom:
    case KeyPosition::OutsideTop:
        // The band makes room: only as wide as on the picture.
        return std::min(area.width() * 0.6 / KeyboardOverlay::kUnitsWide, 34.0 * scale);
    case KeyPosition::OutsideLeft:
    case KeyPosition::OutsideRight:
        // A keyboard is wide: smaller beside the picture, or the band
        // would be wider than the picture.
        return std::min({area.height() / 2.0 / KeyboardOverlay::kUnitsHigh, area.width() * 0.5 / KeyboardOverlay::kUnitsWide,
                         26.0 * scale});
    default:
        // At most 60% of the width and a third of the height, and no
        // bigger than a comfortable size on a large recording.
        return std::min({area.width() * 0.6 / KeyboardOverlay::kUnitsWide, area.height() / 3.0 / KeyboardOverlay::kUnitsHigh,
                         34.0 * scale});
    }
}

int ScreenRecorder::keyLabelHeight(qreal scale) const
{
    QFont font;
    font.setPixelSize(qRound(22 * scale));
    font.setBold(true);
    return QFontMetrics(font).height() + qRound(16 * scale);
}

void ScreenRecorder::layOutFrame()
{
    const QSize area = m_area.size();
    m_frameSize = area;
    m_areaOffset = {};
    m_band = {};
    if (!keysOutside())
        return;

    const qreal scale = m_scale;
    const int margin = qRound(16 * scale);
    const bool keyboard = m_options.keyStyle != KeyStyle::Labels;
    const bool labels = m_options.keyStyle != KeyStyle::Keyboard;
    const QSize board = keyboard ? KeyboardOverlay::sizeFor(keyboardUnit(scale)) : QSize();
    // The keys' block: the keyboard with room for the labels above it.
    const int blockHeight = board.height() + (labels ? keyLabelHeight(scale) + (keyboard ? qRound(10 * scale) : 0) : 0);
    const int blockWidth = keyboard ? board.width() : qRound(320 * scale);

    switch (m_options.keyPosition) {
    case KeyPosition::OutsideTop:
    case KeyPosition::OutsideBottom: {
        const int band = (blockHeight + 2 * margin + 1) & ~1;
        m_frameSize = {area.width(), area.height() + band};
        const bool top = m_options.keyPosition == KeyPosition::OutsideTop;
        m_areaOffset = {0, top ? band : 0};
        m_band = QRect(0, top ? 0 : area.height(), area.width(), band);
        break;
    }
    default: {
        const int band = (blockWidth + 2 * margin + 1) & ~1;
        m_frameSize = {area.width() + band, std::max(area.height(), (blockHeight + 2 * margin + 1) & ~1)};
        const bool left = m_options.keyPosition == KeyPosition::OutsideLeft;
        m_areaOffset = {left ? band : 0, (m_frameSize.height() - area.height()) / 2 & ~1};
        m_band = QRect(left ? 0 : area.width(), 0, band, m_frameSize.height());
        break;
    }
    }
}

void ScreenRecorder::paintPointer(QImage &image, const QPoint &cropOrigin, qreal scale)
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
}

void ScreenRecorder::paintKeys(QImage &image, const QRect &region, qreal scale)
{
    const qint64 now = m_clock.elapsed();
    const bool keyboard = m_options.keyStyle != KeyStyle::Labels;
    const bool labels = m_options.keyStyle != KeyStyle::Keyboard;
    const QRect inner = region.adjusted(qRound(16 * scale), qRound(16 * scale), -qRound(16 * scale), -qRound(16 * scale));
    if (inner.isEmpty())
        return;

    // Where in the region: on a band, its middle.
    Qt::Alignment align = Qt::AlignCenter;
    switch (m_options.keyPosition) {
    case KeyPosition::Bottom: align = Qt::AlignBottom | Qt::AlignHCenter; break;
    case KeyPosition::BottomLeft: align = Qt::AlignBottom | Qt::AlignLeft; break;
    case KeyPosition::BottomRight: align = Qt::AlignBottom | Qt::AlignRight; break;
    case KeyPosition::Top: align = Qt::AlignTop | Qt::AlignHCenter; break;
    case KeyPosition::TopLeft: align = Qt::AlignTop | Qt::AlignLeft; break;
    case KeyPosition::TopRight: align = Qt::AlignTop | Qt::AlignRight; break;
    case KeyPosition::Left: align = Qt::AlignVCenter | Qt::AlignLeft; break;
    case KeyPosition::Right: align = Qt::AlignVCenter | Qt::AlignRight; break;
    default: break;
    }
    // At the top, the labels go under the keyboard, so it stays put at the edge.
    const bool labelsBelow = align.testFlag(Qt::AlignTop);

    m_keys.removeIf([now](const Key &key) { return now - key.at > kKeyShowMs; });
    QString text;
    if (labels && !m_keys.isEmpty()) {
        QStringList parts;
        for (const Key &key : std::as_const(m_keys))
            parts << (key.count > 1 ? QStringLiteral("%1 ×%2").arg(key.text).arg(key.count) : key.text);
        text = parts.join(QStringLiteral("   "));
    }
    QFont font;
    font.setPixelSize(qRound(22 * scale));
    font.setBold(true);
    const int padding = qRound(32 * scale);
    if (!text.isEmpty()) {
        // Smaller where it would not fit (a narrow band beside the picture).
        const int width = QFontMetrics(font).horizontalAdvance(text) + padding;
        if (width > inner.width())
            font.setPixelSize(std::max(8, int(font.pixelSize() * qreal(inner.width() - padding) / (width - padding))));
    }
    const QFontMetrics fm(font);
    const int labelHeight = keyLabelHeight(scale);
    const QSize labelSize = text.isEmpty()
                                ? QSize()
                                : QSize(std::min(inner.width(), fm.horizontalAdvance(text) + padding), labelHeight);

    // The block: the keyboard, with room for the labels kept beside it so it
    // does not jump when they come and go.
    const QSize board = keyboard ? KeyboardOverlay::sizeFor(keyboardUnit(scale)) : QSize();
    const int gap = keyboard && labels ? qRound(10 * scale) : 0;
    const int blockWidth = keyboard ? board.width() : labelSize.width();
    const int blockHeight = board.height() + (labels ? gap + labelHeight : 0);
    const auto place = [&](int size, int from, int room, bool start, bool end) {
        return start ? from : end ? from + room - size : from + (room - size) / 2;
    };
    const QRect block(place(blockWidth, inner.left(), inner.width(), align.testFlag(Qt::AlignLeft), align.testFlag(Qt::AlignRight)),
                      place(blockHeight, inner.top(), inner.height(), align.testFlag(Qt::AlignTop), align.testFlag(Qt::AlignBottom)),
                      blockWidth, blockHeight);

    QPainter p(&image);
    p.setRenderHint(QPainter::Antialiasing);
    if (keyboard) {
        const QRect area(block.left(), labelsBelow ? block.top() : block.bottom() + 1 - board.height(), board.width(), board.height());
        m_keyboard.paint(p, area, now);
    }
    if (!text.isEmpty()) {
        const int x = place(labelSize.width(), block.left(), block.width(), align.testFlag(Qt::AlignLeft), align.testFlag(Qt::AlignRight));
        const int y = labelsBelow ? block.bottom() + 1 - labelHeight : block.top();
        const QRect box(QPoint(x, y), labelSize);
        // Fades out with the newest key.
        const qreal age = qreal(now - m_keys.last().at) / kKeyShowMs;
        const qreal opacity = age < 0.75 ? 1.0 : std::max(0.0, (1.0 - age) / 0.25);
        p.setOpacity(opacity);
        p.setPen(Qt::NoPen);
        p.setBrush(QColor(0, 0, 0, 190));
        p.drawRoundedRect(box, 10 * scale, 10 * scale);
        p.setFont(font);
        p.setPen(Qt::white);
        p.drawText(box, Qt::AlignCenter, text);
        p.setOpacity(1.0);
    }
}
