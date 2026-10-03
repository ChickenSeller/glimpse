#include "ScrollCaptureSession.h"

#include "Platform.h"
#include "ScrollInput.h"

#include <QCursor>
#include <QDir>
#include <QGuiApplication>
#include <QHBoxLayout>
#include <QLabel>
#include <QLoggingCategory>
#include <QPushButton>
#include <QScreen>
#include <QTimer>
#include <QWidget>

#include <algorithm>
#include <cmath>

// Per-step diagnostics; enable with QT_LOGGING_RULES="glimpse.scroll.debug=true".
Q_LOGGING_CATEGORY(lcScroll, "glimpse.scroll", QtWarningMsg)

namespace {

// Time for the scrolled application to repaint (and finish smooth scrolling).
constexpr int kSettleMs = 350;
// First grab: also lets the selection overlay disappear.
constexpr int kStartDelayMs = 400;
constexpr int kMaxHeight = 30000; // pixels
constexpr int kMaxSteps = 400;
constexpr int kMaxNotches = 15;
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

} // namespace

ScrollCaptureSession::ScrollCaptureSession(const QRect &logicalRect, QObject *parent)
    : QObject(parent)
{
    m_screen = QGuiApplication::screenAt(logicalRect.center());
    if (!m_screen)
        m_screen = QGuiApplication::primaryScreen();
    // One screen at a time: grabbing is per screen.
    m_rect = logicalRect.intersected(m_screen->geometry());
    // Near the right edge, where hover effects in the content are least likely.
    m_scrollPoint = QPoint(m_rect.right() - std::min(24, m_rect.width() / 4), m_rect.center().y());
}

ScrollCaptureSession::~ScrollCaptureSession() = default;

void ScrollCaptureSession::start()
{
    if (m_rect.width() < 16 || m_rect.height() < 48) {
        finish();
        return;
    }
    m_restoreCursor = QCursor::pos();
    ScrollInput::escapePressed(); // forget an Esc from before (e.g. a canceled selection)
    createChrome();
    QCursor::setPos(m_screen, m_scrollPoint);
    QTimer::singleShot(kStartDelayMs, this, &ScrollCaptureSession::step);
}

void ScrollCaptureSession::createChrome()
{
    // The frame sits entirely outside the captured area.
    const QRect outer = m_rect.adjusted(-kBorder, -kBorder, kBorder, kBorder);
    m_frame.emplace_back(makeStrip(QRect(outer.left(), outer.top(), outer.width(), kBorder)));
    m_frame.emplace_back(makeStrip(QRect(outer.left(), m_rect.bottom() + 1, outer.width(), kBorder)));
    m_frame.emplace_back(makeStrip(QRect(outer.left(), m_rect.top(), kBorder, m_rect.height())));
    m_frame.emplace_back(makeStrip(QRect(m_rect.right() + 1, m_rect.top(), kBorder, m_rect.height())));
    for (const auto &strip : m_frame)
        strip->show();

    m_panel = std::make_unique<QWidget>(nullptr, Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint | Qt::Tool
                                                     | Qt::WindowDoesNotAcceptFocus);
    m_panel->setAttribute(Qt::WA_ShowWithoutActivating);
    auto *layout = new QHBoxLayout(m_panel.get());
    layout->setContentsMargins(10, 6, 6, 6);
    m_status = new QLabel;
    auto *stop = new QPushButton(tr("Stop"));
    stop->setFocusPolicy(Qt::NoFocus);
    connect(stop, &QPushButton::clicked, this, [this] { m_stopRequested = true; });
    layout->addWidget(m_status);
    layout->addWidget(stop);
    updatePanel();
    m_panel->adjustSize();
    Platform::disableWindowAnimations(m_panel.get());

    // Below, above, then beside the area; inside it only as a last resort
    // (then it is hidden for every grab). Taskbar space counts as room.
    const QRect screen = m_screen->geometry();
    const QSize size = m_panel->sizeHint().expandedTo(QSize(260, 0));
    const int gap = kBorder + 6;
    const QList<QPoint> candidates = {
        {m_rect.right() - size.width() + 1, m_rect.bottom() + gap},
        {m_rect.right() - size.width() + 1, m_rect.top() - gap - size.height()},
        {m_rect.right() + gap, m_rect.bottom() - size.height() + 1},
        {m_rect.left() - gap - size.width(), m_rect.bottom() - size.height() + 1},
    };
    QRect placed;
    for (const QPoint &pos : candidates) {
        const QRect r(pos, size);
        if (screen.contains(r)) {
            placed = r;
            break;
        }
    }
    if (placed.isNull()) {
        placed = QRect(QPoint(screen.right() - size.width() - 12, screen.top() + 12), size);
        m_panelOverlaps = placed.intersects(m_rect);
    }
    m_panel->setGeometry(placed);
    if (!m_panelOverlaps)
        m_panel->show();
}

void ScrollCaptureSession::updatePanel()
{
    if (!m_status)
        return;
    m_status->setText(tr("Scrolling capture: %1 px  ·  Esc to stop").arg(m_stitcher.height()));
}

void ScrollCaptureSession::step()
{
    if (m_done)
        return;
    if (m_stopRequested || ScrollInput::escapePressed()) {
        qCDebug(lcScroll) << (m_stopRequested ? "stopped by the Stop button" : "stopped by Esc");
        finish();
        return;
    }

    const QRect local = m_rect.translated(-m_screen->geometry().topLeft());
    const QImage frame = m_screen->grabWindow(0, local.x(), local.y(), local.width(), local.height()).toImage();
    if (m_panelOverlaps)
        m_panel->show(); // hidden since before the scroll, see below
    if (frame.isNull()) {
        finish();
        return;
    }

    const int notchesUsed = m_notches;
    if (lcScroll().isDebugEnabled()) {
        // With diagnostics on, keep every frame for inspection.
        static int frameNumber = 0;
        frame.save(QDir::temp().filePath(QStringLiteral("glimpse-scroll-%1.png").arg(frameNumber++, 3, 10, QLatin1Char('0'))));
    }
    const ScrollStitcher::Result result = m_stitcher.addFrame(frame);
    qCDebug(lcScroll) << "step" << m_steps << "result" << int(result) << "(0 added, 1 unchanged, 2 no overlap)"
                      << "shift" << m_stitcher.lastShift() << "notches" << notchesUsed << "height"
                      << m_stitcher.height() << "content" << m_stitcher.contentHeight();
    switch (result) {
    case ScrollStitcher::Result::Added:
        m_unchanged = 0;
        m_retrying = false;
        if (m_stitcher.lastShift() > 0) {
            // Aim for scrolling ~60% of the view per step: fast, yet with
            // enough overlap to line frames up.
            const double perNotch = double(m_stitcher.lastShift()) / notchesUsed;
            const double target = 0.6 * m_stitcher.contentHeight();
            m_notches = std::clamp(int(std::floor(target / perNotch)), 1, kMaxNotches);
        }
        break;
    case ScrollStitcher::Result::NoChange:
        if (m_retrying) {
            m_retrying = false; // back where the last good frame was
        } else if (++m_unchanged >= 2) {
            finish(); // the end of the content
            return;
        }
        break;
    case ScrollStitcher::Result::NoOverlap:
        if (m_notches <= 1) {
            finish(); // even one notch jumps too far; keep what we have
            return;
        }
        // Too far to line up: go back and take smaller steps.
        if (m_panelOverlaps)
            m_panel->hide();
        ScrollInput::wheel(m_notches);
        m_notches = std::max(1, m_notches / 2);
        m_retrying = true;
        QTimer::singleShot(kSettleMs, this, &ScrollCaptureSession::step);
        return;
    }

    updatePanel();
    if (m_stitcher.height() >= kMaxHeight || ++m_steps >= kMaxSteps) {
        finish();
        return;
    }
    // Keep the pointer on the area in case it was moved meanwhile.
    QCursor::setPos(m_screen, m_scrollPoint);
    // A panel inside the area is hidden now, so it is gone by the next grab.
    if (m_panelOverlaps)
        m_panel->hide();
    ScrollInput::wheel(-m_notches);
    QTimer::singleShot(kSettleMs, this, &ScrollCaptureSession::step);
}

void ScrollCaptureSession::finish()
{
    if (m_done)
        return;
    qCDebug(lcScroll) << "finished after" << m_steps << "steps, height" << m_stitcher.height();
    m_done = true;
    m_frame.clear();
    m_panel.reset();
    if (!m_restoreCursor.isNull())
        QCursor::setPos(m_restoreCursor);
    emit finished(m_stitcher.image());
}
