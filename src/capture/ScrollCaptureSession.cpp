#include "ScrollCaptureSession.h"

#include "Platform.h"
#include "ScrollInput.h"
#include "hotkey/GlobalHotkeys.h"

#include <QCursor>
#include <QDir>
#include <QGuiApplication>
#include <QHBoxLayout>
#include <QKeyEvent>
#include <QLabel>
#include <QLoggingCategory>
#include <QMouseEvent>
#include <QPainter>
#include <QPushButton>
#include <QScreen>
#include <QTimer>
#include <QToolTip>
#include <QWidget>

#include <functional>

#include <algorithm>
#include <cmath>

// Per-step diagnostics; enable with QT_LOGGING_RULES="glimpse.scroll.debug=true".
Q_LOGGING_CATEGORY(lcScroll, "glimpse.scroll", QtWarningMsg)

namespace {

// Time for the scrolled application to repaint (and finish smooth scrolling).
constexpr int kSettleMs = 350;
// First grab: also lets the selection overlay disappear.
constexpr int kStartDelayMs = 400;
// Manual scrolling: how often the area is grabbed.
constexpr int kManualIntervalMs = 100;
constexpr int kMaxHeight = 30000; // pixels
constexpr int kMaxSteps = 400;
constexpr int kMaxNotches = 15;
constexpr int kBorder = 3;
const QColor kFrameColor(0xe5, 0x39, 0x35);
constexpr int kUndoHotkey = 1000; // apart from the capture hotkeys' ids
constexpr int kStopHotkey = 1001;
// The preview beside the area: at most this size, and not shown in less room.
constexpr int kPreviewMaxWidth = 220;
constexpr int kPreviewMaxHeight = 560;
constexpr int kPreviewMinWidth = 100;
constexpr int kPreviewPad = 4;
const QColor kPreviewBorder(0xff, 0xd4, 0x00);

// The stitched image so far, shrunk to fit; the mouse goes through it.
class PreviewWindow : public QWidget
{
public:
    explicit PreviewWindow(qreal opacity)
        : QWidget(nullptr, Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint | Qt::Tool
                               | Qt::WindowTransparentForInput | Qt::WindowDoesNotAcceptFocus)
        , m_opacity(opacity)
    {
        setAttribute(Qt::WA_ShowWithoutActivating);
        setAttribute(Qt::WA_TranslucentBackground);
    }

    void setImage(const QImage &image)
    {
        m_image = image;
        update();
    }

protected:
    void paintEvent(QPaintEvent *) override
    {
        if (m_image.isNull())
            return;
        // Only as wide as the image, centered; it narrows as it grows longer.
        const QSizeF size = m_image.deviceIndependentSize();
        const QRectF image(QPointF((width() - size.width()) / 2, kPreviewPad), size);
        QPainter p(this);
        // See-through, so it hides less of what is beside the area.
        p.setOpacity(m_opacity);
        p.drawImage(image, m_image);
        // Right around the image, a border of exactly one screen pixel, also
        // at 150% and the like.
        const qreal dpr = devicePixelRatioF();
        p.setWorldTransform(QTransform::fromScale(1.0 / dpr, 1.0 / dpr));
        const QRect edge = QRectF(image.topLeft() * dpr, image.size() * dpr).toAlignedRect();
        p.setPen(QPen(kPreviewBorder, 1));
        p.setBrush(Qt::NoBrush);
        p.drawRect(QRectF(edge).adjusted(-0.5, -0.5, 0.5, 0.5));
    }

private:
    QImage m_image;
    qreal m_opacity;
};

// Over the area until the user clicks where to scroll: catches that click
// (so it opens no link below). Nearly transparent: a fully transparent window
// lets clicks through.
class ClickCatcher : public QWidget
{
public:
    ClickCatcher(const QRect &geometry, std::function<void(const QPoint &)> clicked, std::function<void()> canceled)
        : QWidget(nullptr, Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint | Qt::Tool)
        , m_clicked(std::move(clicked))
        , m_canceled(std::move(canceled))
    {
        setAttribute(Qt::WA_TranslucentBackground);
        setAttribute(Qt::WA_DeleteOnClose);
        setCursor(Qt::PointingHandCursor);
        setFocusPolicy(Qt::StrongFocus);
        setGeometry(geometry);
    }

protected:
    void paintEvent(QPaintEvent *) override { QPainter(this).fillRect(rect(), QColor(0, 0, 0, 1)); }
    void mousePressEvent(QMouseEvent *event) override
    {
        if (event->button() != Qt::LeftButton)
            return;
        const QPoint at = event->globalPosition().toPoint();
        hide();
        close();
        m_clicked(at);
    }
    void keyPressEvent(QKeyEvent *event) override
    {
        if (event->key() == Qt::Key_Escape) {
            hide();
            close();
            m_canceled();
        }
    }

private:
    std::function<void(const QPoint &)> m_clicked;
    std::function<void()> m_canceled;
};

// What to do before the click, in a tooltip's looks, at the area's edge.
QWidget *makeClickHint()
{
    auto *hint = new QLabel(ScrollCaptureSession::tr("Click where to scroll to begin\nEsc to cancel"), nullptr,
                            Qt::ToolTip | Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint
                                | Qt::WindowTransparentForInput | Qt::WindowDoesNotAcceptFocus);
    hint->setAttribute(Qt::WA_ShowWithoutActivating);
    hint->setPalette(QToolTip::palette());
    hint->setFont(QToolTip::font());
    hint->setFrameShape(QFrame::Box);
    hint->setMargin(4);
    hint->adjustSize();
    return hint;
}

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

ScrollCaptureSession::ScrollCaptureSession(const QRect &logicalRect, Scrolling scrolling, QObject *parent)
    : QObject(parent)
    , m_scrolling(scrolling)
    , m_stitcher(scrolling == Scrolling::Manual ? ScrollStitcher::Scrolling::Tracking : ScrollStitcher::Scrolling::Down)
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
    ScrollInput::escapePressed(); // forget an Esc from before (e.g. a canceled selection)
    createChrome();
    // Hotkeys, so the scrolled application does not undo something itself,
    // and Esc works also over a window running as administrator, whose keys
    // Glimpse cannot otherwise see.
    m_hotkeys = GlobalHotkeys::create(this);
    connect(m_hotkeys, &GlobalHotkeys::activated, this, [this](int id) {
        if (m_clickCatcher && id == kStopHotkey)
            abort(); // nothing captured yet
        else if (id == kUndoHotkey)
            undo();
        else if (id == kStopHotkey)
            stop();
    });
    holdKeys(true);
    if (m_scrolling == Scrolling::Manual) {
        QTimer::singleShot(kStartDelayMs, this, &ScrollCaptureSession::manualStep);
        return;
    }
    // Automatic scrolling begins with a click where to scroll.
    m_clickCatcher = new ClickCatcher(
        m_rect, [this](const QPoint &at) { beginScrolling(at); }, [this] { abort(); });
    Platform::disableWindowAnimations(m_clickCatcher);
    m_clickCatcher->show();
    m_clickCatcher->raise();
    m_clickCatcher->activateWindow();
    m_clickCatcher->setFocus();
    showClickHint();
    updatePanel();
}

// Above the area at its left, else below it, outside the frame and clear of
// the panel; inside its top left corner only where there is no room. Gone
// with the click, before the first grab.
void ScrollCaptureSession::showClickHint()
{
    m_clickHint.reset(makeClickHint());
    const QSize size = m_clickHint->size();
    const QRect screen = m_screen->geometry();
    const int gap = kBorder + 4;
    const QRect panel = m_panel && m_panel->isVisible() ? m_panel->geometry() : QRect();
    QRect placed(QPoint(m_rect.left() + gap, m_rect.top() + gap), size); // inside, the last resort
    for (const QPoint &at : {QPoint(m_rect.left(), m_rect.top() - gap - size.height()),
                             QPoint(m_rect.left(), m_rect.bottom() + 1 + gap)}) {
        const QRect r(at, size);
        if (screen.contains(r) && !r.intersects(panel)) {
            placed = r;
            break;
        }
    }
    m_clickHint->move(placed.topLeft());
    Platform::disableWindowAnimations(m_clickHint.get());
    m_clickHint->show();
}

void ScrollCaptureSession::clickAt(const QPoint &point)
{
    if (!m_clickCatcher)
        return;
    m_clickCatcher->close();
    beginScrolling(point);
}

void ScrollCaptureSession::beginScrolling(const QPoint &scrollPoint)
{
    if (m_done)
        return;
    m_clickHint.reset();
    m_scrollPoint = QPoint(std::clamp(scrollPoint.x(), m_rect.left(), m_rect.right()),
                           std::clamp(scrollPoint.y(), m_rect.top(), m_rect.bottom()));
    qCDebug(lcScroll) << "scrolling at" << m_scrollPoint;
    updatePanel();
    m_restoreCursor = QCursor::pos();
    QCursor::setPos(m_screen, m_scrollPoint);
    ScrollInput::escapePressed(); // forget an Esc pressed while waiting
    ScrollInput::watchUser(true);
    // The catcher is gone by the first grab.
    QTimer::singleShot(kStartDelayMs, this, &ScrollCaptureSession::step);
}

void ScrollCaptureSession::holdKeys(bool hold)
{
    m_hotkeys->clear();
    if (!hold)
        return;
    m_hotkeys->add(kUndoHotkey, QKeySequence(Qt::CTRL | Qt::Key_Z), tr("Undo"));
    m_hotkeys->add(kStopHotkey, QKeySequence(Qt::Key_Escape), tr("Stop"));
}

void ScrollCaptureSession::focusPanel()
{
    if (!m_panel || m_panelOverlaps)
        return;
    qCDebug(lcScroll) << "the area's window is out of reach of keys: the panel takes the focus";
    m_panel->setWindowFlag(Qt::WindowDoesNotAcceptFocus, false); // hides it: shown again below
    m_panel->setAttribute(Qt::WA_ShowWithoutActivating, false);
    m_panel->setFocusPolicy(Qt::StrongFocus);
    m_panel->installEventFilter(this);
    Platform::disableWindowAnimations(m_panel.get());
    m_panel->show();
    m_panel->raise();
    m_panel->activateWindow();
    m_panel->setFocus();
}

bool ScrollCaptureSession::eventFilter(QObject *watched, QEvent *event)
{
    if (m_panel && watched == m_panel.get() && event->type() == QEvent::KeyPress) {
        const auto *key = static_cast<QKeyEvent *>(event);
        if (key->key() == Qt::Key_Escape) {
            stop();
            return true;
        }
        if (key->key() == Qt::Key_Z && key->modifiers().testFlag(Qt::ControlModifier)) {
            undo();
            return true;
        }
    }
    return QObject::eventFilter(watched, event);
}

void ScrollCaptureSession::continueManually()
{
    holdKeys(true);
    m_reachChecked = true;
    focusPanel(); // the area's window is out of reach: that is why we are here
    ScrollInput::escapePressed(); // not the Esc that may have answered the question
    // The question's window may still be fading out over the area.
    takeOver(kStartDelayMs);
}

void ScrollCaptureSession::takeOver(int firstGrabDelayMs)
{
    if (m_scrolling == Scrolling::Manual)
        return;
    qCDebug(lcScroll) << "the user took over the scrolling";
    m_scrolling = Scrolling::Manual;
    m_stitcher.setScrolling(ScrollStitcher::Scrolling::Tracking);
    ScrollInput::watchUser(false);
    m_restoreCursor = {}; // the pointer is the user's now; leave it where they put it
    if (m_panelOverlaps)
        m_panel->hide(); // manual scrolling grabs too often to show it in between
    updatePanel();
    QTimer::singleShot(firstGrabDelayMs, this, &ScrollCaptureSession::manualStep);
}

void ScrollCaptureSession::undo()
{
    if (m_done || !m_stitcher.canUndo())
        return;
    // Taking rows back only lines up again when following the view.
    takeOver(kManualIntervalMs);
    m_stitcher.undoLast();
    m_lost = false;
    updatePanel();
    updatePreview();
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
    m_undo = new QPushButton(tr("Undo"));
    m_undo->setFocusPolicy(Qt::NoFocus);
    m_undo->setToolTip(tr("Take back the last part stitched on (Ctrl+Z)"));
    connect(m_undo, &QPushButton::clicked, this, &ScrollCaptureSession::undo);
    auto *stop = new QPushButton(tr("Stop"));
    stop->setFocusPolicy(Qt::NoFocus);
    connect(stop, &QPushButton::clicked, this, &ScrollCaptureSession::stop);
    layout->addWidget(m_status);
    layout->addWidget(m_undo);
    layout->addWidget(stop);
    // Room for the longest status, so the panel never cuts one off.
    int widest = 0;
    for (const QString &text : {tr("Scrolling capture: %1 px  ·  Turn the wheel to scroll yourself  ·  Esc to stop").arg(99999),
                                tr("Click in the area where to scroll  ·  Esc to cancel"),
                                tr("Not lined up: scroll back a little  ·  Esc to stop"),
                                tr("Scroll the area yourself: %1 px  ·  Esc to stop").arg(99999)})
        widest = std::max(widest, m_status->fontMetrics().horizontalAdvance(text));
    m_status->setMinimumWidth(widest);
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
    // Manual scrolling grabs too often to hide it each time: then it stays hidden.
    if (!m_panelOverlaps)
        m_panel->show();
    placePreview(m_panelOverlaps ? QRect() : placed);
}

// Beside the area, its bottom level with the area's, on the side with room
// and clear of the panel; not at all if there is no such room.
void ScrollCaptureSession::placePreview(const QRect &panel)
{
    if (m_previewOpacity <= 0)
        return;
    const QRect screen = m_screen->geometry();
    const int gap = kBorder + 6;
    const int height = std::min(kPreviewMaxHeight, std::max(m_rect.height(), 160));
    const int top = std::max(screen.top(), m_rect.bottom() - height + 1);
    const int rightRoom = screen.right() - (m_rect.right() + gap);
    const int leftRoom = (m_rect.left() - gap) - screen.left();
    QList<QRect> candidates;
    if (rightRoom >= kPreviewMinWidth)
        candidates << QRect(m_rect.right() + gap, top, std::min(kPreviewMaxWidth, rightRoom), height);
    if (leftRoom >= kPreviewMinWidth) {
        const int width = std::min(kPreviewMaxWidth, leftRoom);
        candidates << QRect(m_rect.left() - gap - width, top, width, height);
    }
    for (const QRect &r : std::as_const(candidates)) {
        if (panel.intersects(r))
            continue;
        m_preview = std::make_unique<PreviewWindow>(m_previewOpacity);
        m_preview->setGeometry(r);
        Platform::disableWindowAnimations(m_preview.get());
        m_preview->show();
        return;
    }
}

void ScrollCaptureSession::updatePreview()
{
    if (!m_preview)
        return;
    const qreal dpr = m_preview->devicePixelRatioF();
    const QSize room = m_preview->size() - QSize(2 * kPreviewPad, 2 * kPreviewPad);
    QImage image = m_stitcher.preview(room * dpr);
    image.setDevicePixelRatio(dpr);
    static_cast<PreviewWindow *>(m_preview.get())->setImage(image);
}

void ScrollCaptureSession::updatePanel()
{
    if (!m_status)
        return;
    m_undo->setEnabled(m_stitcher.canUndo());
    if (m_clickCatcher)
        m_status->setText(tr("Click in the area where to scroll  ·  Esc to cancel"));
    else if (m_scrolling == Scrolling::Automatic)
        m_status->setText(tr("Scrolling capture: %1 px  ·  Turn the wheel to scroll yourself  ·  Esc to stop")
                              .arg(m_stitcher.height()));
    else if (m_lost)
        m_status->setText(tr("Not lined up: scroll back a little  ·  Esc to stop"));
    else
        m_status->setText(tr("Scroll the area yourself: %1 px  ·  Esc to stop").arg(m_stitcher.height()));
}

void ScrollCaptureSession::manualStep()
{
    if (m_done)
        return;
    // Once, with the selection gone and the pointer on the area.
    if (!m_reachChecked) {
        m_reachChecked = true;
        if (ScrollInput::targetOutOfReach())
            focusPanel();
    }
    if (m_stopRequested || ScrollInput::escapePressed()) {
        finish();
        return;
    }

    const QRect local = m_rect.translated(-m_screen->geometry().topLeft());
    const QImage frame = m_screen->grabWindow(0, local.x(), local.y(), local.width(), local.height()).toImage();
    if (frame.isNull()) {
        finish();
        return;
    }
    // Unchanged frames are simply skipped. One that does not line up was
    // scrolled too far since the last grab: scrolling back to where that one
    // was picks up from there.
    const ScrollStitcher::Result result = m_stitcher.addFrame(frame);
    if (result != ScrollStitcher::Result::NoChange)
        qCDebug(lcScroll) << "manual result" << int(result) << "(0 added, 1 moved, 2 unchanged, 3 no overlap, 4 gap)" << "shift"
                          << m_stitcher.lastShift() << "height" << m_stitcher.height();
    if (result == ScrollStitcher::Result::Added || result == ScrollStitcher::Result::Moved)
        m_lost = false;
    else if (result == ScrollStitcher::Result::NoOverlap || result == ScrollStitcher::Result::Gap)
        m_lost = true;
    updatePanel();
    if (result == ScrollStitcher::Result::Added)
        updatePreview();
    if (m_stitcher.height() >= kMaxHeight) {
        finish();
        return;
    }
    QTimer::singleShot(kManualIntervalMs, this, &ScrollCaptureSession::manualStep);
}

void ScrollCaptureSession::step()
{
    // Also a step still on its way after the user took over.
    if (m_done || m_scrolling == Scrolling::Manual)
        return;
    if (m_stopRequested || ScrollInput::escapePressed()) {
        qCDebug(lcScroll) << (m_stopRequested ? "stopped by the Stop button" : "stopped by Esc");
        finish();
        return;
    }
    if (ScrollInput::userScrolled()) {
        takeOver(kManualIntervalMs);
        return;
    }
    // Before the first scroll, with the pointer on the area: Windows would
    // drop the wheel input to a window running as administrator.
    if (m_steps == 0 && m_stitcher.isEmpty() && ScrollInput::targetOutOfReach()) {
        qCDebug(lcScroll) << "the area's window runs with higher rights";
        holdKeys(false); // the question needs Esc
        emit elevationNeeded();
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
    qCDebug(lcScroll) << "step" << m_steps << "result" << int(result) << "(0 added, 2 unchanged, 3 no overlap)"
                      << "shift" << m_stitcher.lastShift() << "notches" << notchesUsed << "height"
                      << m_stitcher.height() << "content" << m_stitcher.contentHeight();
    if (result == ScrollStitcher::Result::Added)
        updatePreview();
    switch (result) {
    case ScrollStitcher::Result::Added:
    case ScrollStitcher::Result::Moved: // only when Tracking
    case ScrollStitcher::Result::Gap:   // only when Tracking
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

void ScrollCaptureSession::abort()
{
    m_discard = true;
    finish();
}

void ScrollCaptureSession::finish()
{
    if (m_done)
        return;
    qCDebug(lcScroll) << "finished after" << m_steps << "steps, height" << m_stitcher.height();
    m_done = true;
    if (m_clickCatcher)
        m_clickCatcher->close();
    m_clickHint.reset();
    ScrollInput::watchUser(false);
    if (m_hotkeys)
        m_hotkeys->clear();
    m_frame.clear();
    m_panel.reset();
    m_preview.reset();
    if (!m_restoreCursor.isNull())
        QCursor::setPos(m_restoreCursor);
    const QImage image = m_discard ? QImage() : m_stitcher.image();
    if (lcScroll().isDebugEnabled())
        image.save(QDir::temp().filePath(QStringLiteral("glimpse-scroll-result.png")));
    emit finished(image);
}
