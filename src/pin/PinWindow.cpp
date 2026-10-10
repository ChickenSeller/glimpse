#include "PinWindow.h"

#include "editor/EditorWindow.h"
#include "capture/Platform.h"
#include "editor/ImageActions.h"

#include <QAction>
#include <QApplication>
#include <QContextMenuEvent>
#include <QCursor>
#include <QGuiApplication>
#include <QMenu>
#include <QPainter>
#include <QScreen>
#include <QWheelEvent>
#include <QWindow>

#include <algorithm>
#include <cmath>

namespace {

const QColor kAccent(0x2d, 0x9c, 0xff);
constexpr qreal kMinZoom = 0.1;
constexpr qreal kMaxZoom = 8.0;
constexpr qreal kZoomStep = 1.1;
constexpr qreal kMinOpacity = 0.2;
constexpr qreal kOpacityStep = 0.1;
constexpr int kHintMs = 1200;
constexpr int kHoverPollMs = 30;
// The close button shown in the top-right corner while the pin is pointed at.
constexpr int kCloseSize = 22;
constexpr int kCloseMargin = 6;

} // namespace

PinWindow::PinWindow(const QImage &image, const QRect &logicalRect)
    : QWidget(nullptr, Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint | Qt::Tool)
    , m_image(image)
{
    setAttribute(Qt::WA_DeleteOnClose);
    setAttribute(Qt::WA_OpaquePaintEvent);
    setFocusPolicy(Qt::StrongFocus);
    setCursor(Qt::SizeAllCursor);
    setMouseTracking(true); // for the close button's hover state

    m_hintTimer.setSingleShot(true);
    m_hintTimer.setInterval(kHintMs);
    connect(&m_hintTimer, &QTimer::timeout, this, [this] {
        m_hint.clear();
        update();
    });

    // Windows does not reliably report the pointer entering or moving over a
    // window that is not active (another pin or program has the focus), so
    // the pin also watches the pointer itself. Not on native Wayland, where
    // the pointer position is only known over the focused window anyway.
    if (Platform::windowsCanPlaceThemselves()) {
        m_hoverTimer.setInterval(kHoverPollMs);
        connect(&m_hoverTimer, &QTimer::timeout, this, &PinWindow::pollHover);
        m_hoverTimer.start();
    }

    // On the widget too, so the shortcuts work without opening the menu.
    m_copyAction = new QAction(this);
    m_copyAction->setShortcut(QKeySequence::Copy);
    connect(m_copyAction, &QAction::triggered, this, &PinWindow::copy);
    m_saveAction = new QAction(this);
    m_saveAction->setShortcut(QKeySequence::Save);
    connect(m_saveAction, &QAction::triggered, this, &PinWindow::saveAs);
    m_editAction = new QAction(this);
    m_editAction->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_E));
    connect(m_editAction, &QAction::triggered, this, &PinWindow::openInEditor);
    m_actualSizeAction = new QAction(this);
    m_actualSizeAction->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_0));
    connect(m_actualSizeAction, &QAction::triggered, this,
            [this] { zoomAt(1.0, QPointF(0, 0)); });
    m_closeAction = new QAction(this);
    m_closeAction->setShortcut(QKeySequence(Qt::Key_Escape));
    connect(m_closeAction, &QAction::triggered, this, &QWidget::close);
    addActions({m_copyAction, m_saveAction, m_editAction, m_actualSizeAction, m_closeAction});
    retranslate();

    resize(logicalSize(m_zoom));
    if (!logicalRect.isEmpty()) {
        move(logicalRect.topLeft());
    } else {
        const QScreen *screen = QGuiApplication::screenAt(QCursor::pos());
        if (!screen)
            screen = QGuiApplication::primaryScreen();
        move(screen->availableGeometry().center() - rect().center());
    }
}

// The window's size for `zoom`: the image's own logical size (its pixels
// over the scale of the screen it was captured on), rounded up.
QSize PinWindow::logicalSize(qreal zoom) const
{
    const QSizeF size = QSizeF(m_image.size()) * zoom / m_image.devicePixelRatio();
    return QSize(std::max(1, int(std::ceil(size.width() - 1e-6))), std::max(1, int(std::ceil(size.height() - 1e-6))));
}

void PinWindow::paintEvent(QPaintEvent *)
{
    QPainter p(this);
    p.fillRect(rect(), palette().window());

    // Drawn in native pixels: at 100% on a screen of the capture's scale every
    // image pixel lands on one screen pixel, also at fractional scaling (150%),
    // where logical coordinates fall between screen pixels.
    const qreal dpr = devicePixelRatioF();
    const qreal scale = m_zoom * dpr / m_image.devicePixelRatio();
    const QRectF target(0, 0, m_image.width() * scale, m_image.height() * scale);
    QImage source = m_image;
    source.setDevicePixelRatio(1.0);
    p.save();
    p.setWorldTransform(QTransform::fromScale(1.0 / dpr, 1.0 / dpr));
    // Exact multiples stay crisp (pixels as squares); anything else is smoothed.
    p.setRenderHint(QPainter::SmoothPixmapTransform, std::abs(scale - std::round(scale)) > 1e-6);
    p.drawImage(target, source);

    // While pointed at: one screen pixel over the image's outermost pixels,
    // showing where the pin ends against whatever is behind it.
    if (m_hovered) {
        p.setPen(QPen(kAccent, 1));
        p.setBrush(Qt::NoBrush);
        p.drawRect(QRectF(target.topLeft(), target.size()).adjusted(0.5, 0.5, -0.5, -0.5));
    }
    p.restore();

    if (m_hovered) {
        // Half see-through until pointed at, so it hides little of the image.
        const QRect button = closeButtonRect();
        p.setRenderHint(QPainter::Antialiasing);
        p.setPen(Qt::NoPen);
        p.setBrush(m_closeHovered ? QColor(0xe8, 0x11, 0x23, 230) : QColor(0, 0, 0, 120));
        p.drawEllipse(button);
        const qreal arm = button.width() * 0.2;
        const QPointF c = QRectF(button).center();
        p.setPen(QPen(QColor(255, 255, 255, m_closeHovered ? 255 : 200), 1.6, Qt::SolidLine, Qt::RoundCap));
        p.drawLine(c + QPointF(-arm, -arm), c + QPointF(arm, arm));
        p.drawLine(c + QPointF(-arm, arm), c + QPointF(arm, -arm));
        p.setRenderHint(QPainter::Antialiasing, false);
    }

    if (!m_hint.isEmpty()) {
        const QFontMetrics fm = p.fontMetrics();
        const QRect box(QPoint(6, 6), fm.size(Qt::TextSingleLine, m_hint) + QSize(12, 6));
        p.setPen(Qt::NoPen);
        p.setBrush(QColor(0, 0, 0, 190));
        p.drawRoundedRect(box, 3, 3);
        p.setPen(Qt::white);
        p.drawText(box, Qt::AlignCenter, m_hint);
    }
}

// Top-right corner, shrunk on pins too small for the full size.
QRect PinWindow::closeButtonRect() const
{
    const int size = std::clamp(std::min(width(), height()) / 3, 10, kCloseSize);
    const int margin = std::min(kCloseMargin, size / 3);
    return QRect(width() - margin - size, margin, size, size);
}

void PinWindow::setCloseHovered(bool hovered)
{
    if (hovered == m_closeHovered)
        return;
    m_closeHovered = hovered;
    setCursor(hovered ? Qt::ArrowCursor : Qt::SizeAllCursor);
    update(closeButtonRect());
}

void PinWindow::setHovered(bool hovered)
{
    if (hovered == m_hovered)
        return;
    m_hovered = hovered;
    if (!hovered)
        setCloseHovered(false);
    update();
}

// Whether the pointer is over this pin, and not over a window above it.
void PinWindow::pollHover()
{
    const QPoint global = QCursor::pos();
    const bool over = QApplication::topLevelAt(global) == this;
    setHovered(over);
    setCloseHovered(over && closeButtonRect().contains(mapFromGlobal(global)));
}

void PinWindow::mouseMoveEvent(QMouseEvent *event)
{
    setHovered(true);
    setCloseHovered(closeButtonRect().contains(event->position().toPoint()));
}

void PinWindow::mousePressEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton && closeButtonRect().contains(event->position().toPoint())) {
        close();
        return;
    }
    activateWindow();
    // The window system moves the window, which also works on Wayland.
    if (event->button() == Qt::LeftButton && windowHandle())
        windowHandle()->startSystemMove();
}

void PinWindow::mouseDoubleClickEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton)
        close();
}

void PinWindow::wheelEvent(QWheelEvent *event)
{
    const int steps = event->angleDelta().y() / 120;
    if (steps == 0)
        return;
    if (event->modifiers() & Qt::ControlModifier) {
        const qreal opacity = std::clamp(windowOpacity() + steps * kOpacityStep, kMinOpacity, 1.0);
        setWindowOpacity(opacity);
        showHint(tr("Opacity %1%").arg(qRound(opacity * 100)));
        return;
    }
    zoomAt(m_zoom * std::pow(kZoomStep, steps), event->position());
}

// Zooms keeping the image point under `anchor` (local coordinates) in place.
void PinWindow::zoomAt(qreal zoom, const QPointF &anchor)
{
    // Snaps to 100% when passing it, so the original size is easy to get back to.
    if ((m_zoom < 1.0 && zoom > 1.0) || (m_zoom > 1.0 && zoom < 1.0))
        zoom = 1.0;
    zoom = std::clamp(zoom, kMinZoom, kMaxZoom);
    if (std::abs(zoom - m_zoom) > 1e-9) {
        const QPointF global = mapToGlobal(anchor);
        const QPointF topLeft = global - anchor * (zoom / m_zoom);
        m_zoom = zoom;
        setGeometry(QRect(topLeft.toPoint(), logicalSize(m_zoom)));
    }
    showHint(QStringLiteral("%1%").arg(qRound(m_zoom * 100)));
}

void PinWindow::showHint(const QString &text)
{
    m_hint = text;
    m_hintTimer.start();
    update();
}

void PinWindow::contextMenuEvent(QContextMenuEvent *event)
{
    QMenu menu(this);
    menu.addAction(m_copyAction);
    menu.addAction(m_saveAction);
    menu.addAction(m_editAction);
    menu.addSeparator();
    menu.addAction(m_actualSizeAction);
    m_actualSizeAction->setEnabled(std::abs(m_zoom - 1.0) > 1e-9);
    menu.addSeparator();
    menu.addAction(m_closeAction);
    menu.addSeparator();
    QAction *help = menu.addAction(tr("Scroll: zoom  ·  Ctrl+scroll: opacity  ·  Double-click: close"));
    help->setEnabled(false);
    menu.exec(event->globalPos());
}

void PinWindow::enterEvent(QEnterEvent *event)
{
    setHovered(true);
    QWidget::enterEvent(event);
}

void PinWindow::leaveEvent(QEvent *event)
{
    setHovered(false);
    QWidget::leaveEvent(event);
}

void PinWindow::changeEvent(QEvent *event)
{
    if (event->type() == QEvent::LanguageChange)
        retranslate();
    else if (event->type() == QEvent::ActivationChange)
        update();
    QWidget::changeEvent(event);
}

void PinWindow::retranslate()
{
    setWindowTitle(tr("Pinned Capture"));
    m_copyAction->setText(tr("&Copy"));
    m_saveAction->setText(tr("&Save As..."));
    m_editAction->setText(tr("Open in &Editor"));
    m_actualSizeAction->setText(tr("&Actual Size"));
    m_closeAction->setText(tr("C&lose"));
}

void PinWindow::copy()
{
    ImageActions::copyToClipboard(m_image);
    showHint(tr("Copied"));
}

void PinWindow::saveAs()
{
    if (!ImageActions::saveAs(this, m_image).isEmpty())
        showHint(tr("Saved"));
}

void PinWindow::openInEditor()
{
    auto *editor = new EditorWindow(m_image);
    editor->show();
    editor->raise();
    editor->activateWindow();
}
