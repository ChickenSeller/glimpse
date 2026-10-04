#include "RecordingSession.h"
#include "ui_RecordingPanel.h"

#include "app/TintedIcon.h"
#include "capture/Platform.h"

#include <QGuiApplication>
#include <QScreen>
#include <QTimer>
#include <QWidget>

namespace {

constexpr int kBorder = 3;
const QColor kFrameColor(0xe5, 0x39, 0x35);

QWidget *makeStrip(const QRect &geometry)
{
    auto *strip = new QWidget(nullptr, Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint | Qt::Tool
                                           | Qt::WindowTransparentForInput | Qt::WindowDoesNotAcceptFocus);
    strip->setAttribute(Qt::WA_ShowWithoutActivating);
    strip->setAutoFillBackground(true);
    QPalette palette = strip->palette();
    palette.setColor(QPalette::Window, kFrameColor);
    strip->setPalette(palette);
    strip->setGeometry(geometry);
    return strip;
}

QString formatTime(qint64 ms)
{
    const qint64 s = ms / 1000;
    return s >= 3600 ? QStringLiteral("%1:%2:%3").arg(s / 3600).arg(s / 60 % 60, 2, 10, QLatin1Char('0')).arg(s % 60, 2, 10, QLatin1Char('0'))
                     : QStringLiteral("%1:%2").arg(s / 60, 2, 10, QLatin1Char('0')).arg(s % 60, 2, 10, QLatin1Char('0'));
}

} // namespace

RecordingSession::RecordingSession(const QRect &logicalRect, const ScreenRecorder::Options &options, QObject *parent)
    : QObject(parent)
{
    m_screen = QGuiApplication::screenAt(logicalRect.center());
    if (!m_screen)
        m_screen = QGuiApplication::primaryScreen();
    // One screen at a time: the capture is per screen.
    m_rect = logicalRect.intersected(m_screen->geometry());
    m_recorder = std::make_unique<ScreenRecorder>(m_screen, m_rect, options);
    connect(m_recorder.get(), &ScreenRecorder::finished, this, [this](const QString &path) {
        m_frame.clear();
        m_panel.reset();
        emit finished(path, m_recorder->durationMs());
    });
    connect(m_recorder.get(), &ScreenRecorder::failed, this, [this](const QString &message) {
        m_frame.clear();
        m_panel.reset();
        emit failed(message);
    });
}

RecordingSession::~RecordingSession() = default;

void RecordingSession::start()
{
    createChrome();
    m_clockTimer = new QTimer(this);
    connect(m_clockTimer, &QTimer::timeout, this, &RecordingSession::updatePanel);
    m_clockTimer->start(250);
    m_recorder->start();
}

void RecordingSession::stop()
{
    if (m_stopping)
        return;
    m_stopping = true;
    m_clockTimer->stop();
    m_panelUi->pauseButton->setEnabled(false);
    m_panelUi->stopButton->setEnabled(false);
    m_recorder->stop();
}

void RecordingSession::togglePause()
{
    m_recorder->setPaused(!m_recorder->isPaused());
    const bool paused = m_recorder->isPaused();
    m_panelUi->pauseButton->setIcon(tintedIcon(QIcon(paused ? QStringLiteral(":/icons/play_arrow.svg")
                                                            : QStringLiteral(":/icons/pause.svg")),
                                               m_panel->palette().color(QPalette::ButtonText)));
    m_panelUi->pauseButton->setToolTip(paused ? tr("Resume") : tr("Pause"));
    updatePanel();
}

void RecordingSession::updatePanel()
{
    const QString time = formatTime(m_recorder->durationMs());
    m_panelUi->timeLabel->setText(m_recorder->isPaused() ? tr("%1 (paused)").arg(time) : time);
}

void RecordingSession::createChrome()
{
    // The frame sits entirely outside the recorded area; it is also kept out
    // of the capture in case the area touches a screen edge.
    const QRect outer = m_rect.adjusted(-kBorder, -kBorder, kBorder, kBorder);
    m_frame.emplace_back(makeStrip(QRect(outer.left(), outer.top(), outer.width(), kBorder)));
    m_frame.emplace_back(makeStrip(QRect(outer.left(), m_rect.bottom() + 1, outer.width(), kBorder)));
    m_frame.emplace_back(makeStrip(QRect(outer.left(), m_rect.top(), kBorder, m_rect.height())));
    m_frame.emplace_back(makeStrip(QRect(m_rect.right() + 1, m_rect.top(), kBorder, m_rect.height())));
    for (const auto &strip : m_frame) {
        strip->show();
        Platform::excludeFromCapture(strip.get());
    }

    m_panel = std::make_unique<QWidget>(nullptr, Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint | Qt::Tool
                                                     | Qt::WindowDoesNotAcceptFocus);
    m_panel->setAttribute(Qt::WA_ShowWithoutActivating);
    // Sized for the screen it will be on (fonts and icons follow its scale).
    m_panel->setScreen(m_screen);
    m_panelUi = std::make_unique<Ui::RecordingPanel>();
    m_panelUi->setupUi(m_panel.get());
    // Room for the longest time text up front, so the panel never grows
    // (towards the screen edge or over the area) while recording.
    const QFontMetrics metrics(m_panelUi->timeLabel->font());
    m_panelUi->timeLabel->setMinimumWidth(metrics.horizontalAdvance(tr("%1 (paused)").arg(QStringLiteral("00:00:00"))));
    const QColor iconColor = m_panel->palette().color(QPalette::ButtonText);
    for (QToolButton *button : {m_panelUi->pauseButton, m_panelUi->stopButton})
        button->setIcon(tintedIcon(button->icon(), iconColor));
    connect(m_panelUi->pauseButton, &QToolButton::clicked, this, &RecordingSession::togglePause);
    connect(m_panelUi->stopButton, &QToolButton::clicked, this, &RecordingSession::stop);
    updatePanel();
    m_panel->adjustSize();
    placePanel();
    m_panel->show();
    Platform::excludeFromCapture(m_panel.get());
    // Shown, the window may end up a little different (frame metrics, scale);
    // place it again with its real size.
    QTimer::singleShot(0, this, [this] {
        if (m_panel)
            placePanel();
    });
}

void RecordingSession::placePanel()
{
    // Below, above, then beside the area; inside it (a full-screen recording)
    // only as a last resort, where it relies on being kept out of the capture.
    // Never under the taskbar: it is always on top as well and may cover it.
    const QRect screen = m_screen->availableGeometry();
    const QSize size = m_panel->size().expandedTo(m_panel->sizeHint());
    const int gap = kBorder + 6;
    const QList<QPoint> candidates = {
        {m_rect.right() - size.width() + 1, m_rect.bottom() + gap},
        {m_rect.right() - size.width() + 1, m_rect.top() - gap - size.height()},
        {m_rect.right() + gap, m_rect.bottom() - size.height() + 1},
        {m_rect.left() - gap - size.width(), m_rect.bottom() - size.height() + 1},
    };
    for (const QPoint &pos : candidates) {
        const QRect r(pos, size);
        if (screen.contains(r)) {
            m_panel->setGeometry(r);
            return;
        }
    }
    m_panel->setGeometry(QRect(QPoint(screen.right() - size.width() - 12, screen.bottom() - size.height() - 12), size));
}
