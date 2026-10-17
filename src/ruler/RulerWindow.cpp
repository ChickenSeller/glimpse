#include "RulerWindow.h"

#include "capture/Platform.h"

#include <QAction>
#include <QActionGroup>
#include <QApplication>
#include <QCloseEvent>
#include <QContextMenuEvent>
#include <QCursor>
#include <QGuiApplication>
#include <QKeyEvent>
#include <QMenu>
#include <QPainter>
#include <QPainterPath>
#include <QScreen>
#include <QSettings>
#include <QWheelEvent>
#include <QWindow>

#include <algorithm>
#include <cmath>
#include <numbers>
#include <tuple>

namespace {

const QString kSizeKey = QStringLiteral("ruler/size");
const QString kCrossKey = QStringLiteral("ruler/cross");
const QString kRadiansKey = QStringLiteral("ruler/radians");

// Dark marks with a light halo, to show on light and dark backgrounds alike.
const QColor kInk(0x20, 0x20, 0x20);
const QColor kHalo(255, 255, 255, 200);
// Not quite clear: wholly transparent pixels would let the mouse through.
const QColor kBody(0, 0, 0, 1);
const QColor kPointer(0xe8, 0x11, 0x23);
constexpr int kThickness = 56;      // logical px, across an arm
const QSize kDefaultSize(800, 600); // logical px: the arms' lengths
constexpr int kGrip = 8;            // at an arm's end: drags the end
constexpr int kPointerPollMs = 30;
constexpr int kHintMs = 1200;
constexpr qreal kMinOpacity = 0.2;
constexpr qreal kOpacityStep = 0.1;

// Tick lengths (logical px) by the screen pixel they mark.
qreal tickLength(int pixel)
{
    if (pixel % 100 == 0)
        return 18;
    if (pixel % 50 == 0)
        return 13;
    if (pixel % 10 == 0)
        return 9;
    return 4;
}

// Text in kInk with a kHalo outline.
void drawHaloText(QPainter &p, const QRectF &box, const QString &text)
{
    p.setPen(kHalo);
    for (const QPointF &offset : {QPointF(-1, 0), QPointF(1, 0), QPointF(0, -1), QPointF(0, 1)})
        p.drawText(box.translated(offset), Qt::AlignCenter, text);
    p.setPen(kInk);
    p.drawText(box, Qt::AlignCenter, text);
}

// White on red, beside the pointer's line.
void drawReadout(QPainter &p, const QRectF &box, const QString &text)
{
    p.setRenderHint(QPainter::Antialiasing);
    p.setPen(Qt::NoPen);
    p.setBrush(kPointer);
    p.drawRoundedRect(box, 3, 3);
    p.setPen(Qt::white);
    p.drawText(box, Qt::AlignCenter, text);
    p.setRenderHint(QPainter::Antialiasing, false);
}

} // namespace

// While Ctrl is held: the line from the ruler's cross to the pointer, arcs
// for its angles to the axes, and the figures beside the pointer. Over the
// whole screen, see-through, and never in the mouse's way.
class RulerAngleOverlay : public QWidget
{
public:
    explicit RulerAngleOverlay(QWidget *ruler)
        : QWidget(ruler, Qt::Tool | Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint
                             | Qt::WindowTransparentForInput | Qt::WindowDoesNotAcceptFocus)
    {
        setAttribute(Qt::WA_TranslucentBackground);
        setAttribute(Qt::WA_ShowWithoutActivating);
    }

    // Local logical positions, and the text beside the pointer.
    void set(const QPointF &cross, const QPointF &pointer, const QStringList &lines)
    {
        if (cross == m_cross && pointer == m_pointer && lines == m_lines)
            return;
        m_cross = cross;
        m_pointer = pointer;
        m_lines = lines;
        update();
    }

protected:
    void paintEvent(QPaintEvent *) override
    {
        QPainter p(this);
        p.setRenderHint(QPainter::Antialiasing);
        const QLineF line(m_cross, m_pointer);
        if (line.length() >= 1) {
            for (const auto &[color, width] : {std::pair{kHalo, 3.5}, std::pair{kPointer, 1.5}}) {
                p.setPen(QPen(color, width, Qt::SolidLine, Qt::RoundCap));
                p.drawLine(line);
            }
            // The angle to the x-axis, and the one to the y-axis further out,
            // each from the nearer half of its axis.
            const QPointF d = m_pointer - m_cross;
            const qreal angle = std::atan2(-d.y(), d.x()) * 180 / std::numbers::pi; // counterclockwise, as drawArc
            const qreal fromX = d.x() >= 0 ? 0 : 180;
            const qreal fromY = d.y() <= 0 ? 90 : -90;
            const auto span = [angle](qreal from) { return std::remainder(angle - from, 360.0); };
            const qreal reach = std::min(line.length(), 24.0);
            for (const auto &[from, radius, style] : {std::tuple{fromX, reach * 0.65, Qt::SolidLine},
                                                      std::tuple{fromY, reach, Qt::DashLine}}) {
                const QRectF box(m_cross - QPointF(radius, radius), QSizeF(2 * radius, 2 * radius));
                p.setPen(QPen(kHalo, 3));
                p.drawArc(box, qRound(from * 16), qRound(span(from) * 16));
                p.setPen(QPen(kPointer, 1.2, style));
                p.drawArc(box, qRound(from * 16), qRound(span(from) * 16));
            }
        }

        // Beside the pointer, kept on the screen.
        const QFontMetrics fm = p.fontMetrics();
        int width = 0;
        for (const QString &text : std::as_const(m_lines))
            width = std::max(width, fm.horizontalAdvance(text));
        const QSizeF size(width + 16, m_lines.size() * fm.height() + 10);
        QPointF topLeft = m_pointer + QPointF(18, 18);
        if (topLeft.x() + size.width() > this->width())
            topLeft.setX(m_pointer.x() - 18 - size.width());
        if (topLeft.y() + size.height() > height())
            topLeft.setY(m_pointer.y() - 18 - size.height());
        const QRectF box(topLeft, size);
        p.setPen(Qt::NoPen);
        p.setBrush(QColor(0, 0, 0, 200));
        p.drawRoundedRect(box, 4, 4);
        p.setPen(Qt::white);
        for (qsizetype i = 0; i < m_lines.size(); ++i)
            p.drawText(QPointF(box.left() + 8, box.top() + 5 + fm.ascent() + i * fm.height()), m_lines.at(i));
    }

private:
    QPointF m_cross;
    QPointF m_pointer;
    QStringList m_lines;
};

RulerWindow::RulerWindow()
    : QWidget(nullptr, Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint | Qt::Tool)
{
    setAttribute(Qt::WA_DeleteOnClose);
    setAttribute(Qt::WA_TranslucentBackground); // only the marks show
    setFocusPolicy(Qt::StrongFocus);
    setCursor(Qt::SizeAllCursor);
    setMouseTracking(true);
    setMinimumSize(kThickness, kThickness);

    m_hintTimer.setSingleShot(true);
    m_hintTimer.setInterval(kHintMs);
    connect(&m_hintTimer, &QTimer::timeout, this, [this] {
        m_hint.clear();
        update();
    });

    // The pointer lines follow the pointer outside the ruler too, so the
    // ruler can be laid next to what it measures. Not on native Wayland,
    // where the pointer is only known over our own window.
    if (Platform::windowsCanPlaceThemselves()) {
        m_pointerTimer.setInterval(kPointerPollMs);
        connect(&m_pointerTimer, &QTimer::timeout, this, &RulerWindow::pollPointer);
        m_pointerTimer.start();
    }

    m_closeAction = new QAction(this);
    m_closeAction->setShortcut(QKeySequence(Qt::Key_Escape));
    connect(m_closeAction, &QAction::triggered, this, &QWidget::close);
    addAction(m_closeAction);
    retranslate();

    // Shaped like the last one (symmetric the first time), its cross in the
    // middle of the screen under the pointer.
    const QScreen *screen = QGuiApplication::screenAt(QCursor::pos());
    if (!screen)
        screen = QGuiApplication::primaryScreen();
    const QSettings settings;
    const QSize size = settings.value(kSizeKey, kDefaultSize).toSize().expandedTo(QSize(2, 2) * kThickness);
    const QPoint middle(size.width() / 2, size.height() / 2);
    const QPoint cross = settings.value(kCrossKey, middle).toPoint();
    m_radians = settings.value(kRadiansKey, false).toBool();
    setLayout(QRect(screen->geometry().center() - cross, size), cross);
    keepOnScreen();
}

QRect RulerWindow::horizontalArm() const
{
    return QRect(0, m_cross.y(), width(), kThickness);
}

QRect RulerWindow::verticalArm() const
{
    return QRect(m_cross.x(), 0, kThickness, height());
}

void RulerWindow::setLayout(const QRect &geometry, const QPoint &cross)
{
    m_cross = QPoint(std::clamp(cross.x(), 0, std::max(0, geometry.width() - kThickness)),
                     std::clamp(cross.y(), 0, std::max(0, geometry.height() - kThickness)));
    setGeometry(geometry);
    // Only the arms take the mouse; the window's corners between them let it through.
    setMask(QRegion(horizontalArm()) + QRegion(verticalArm()));
    update();
}

void RulerWindow::resizeEvent(QResizeEvent *event)
{
    setMask(QRegion(horizontalArm()) + QRegion(verticalArm()));
    QWidget::resizeEvent(event);
}

// Exact where the platform tells both the pointer and the window in physical
// pixels; otherwise from Qt's logical positions, which are rounded at
// fractional scaling (150%) and can be a pixel off.
RulerWindow::Measure RulerWindow::measure() const
{
    const std::optional<QPoint> cursor = Platform::nativeCursorPos();
    const std::optional<QRect> window = cursor ? Platform::nativeWindowRect(this) : std::nullopt;
    if (cursor && window)
        return {window->size(), *cursor - window->topLeft()};
    const qreal dpr = devicePixelRatioF();
    const QPointF local = QPointF(mapFromGlobal(QCursor::pos())) * dpr;
    return {QSize(qRound(width() * dpr), qRound(height() * dpr)),
            QPoint(int(std::floor(local.x())), int(std::floor(local.y())))};
}

// The pointer, per axis, where it is along an arm; -1 where it is not.
static QPoint pointerAlong(const QSize &size, const QPoint &pointer)
{
    return QPoint(pointer.x() >= 0 && pointer.x() < size.width() ? pointer.x() : -1,
                  pointer.y() >= 0 && pointer.y() < size.height() ? pointer.y() : -1);
}

void RulerWindow::paintEvent(QPaintEvent *)
{
    const Measure m = measure();
    const QPoint pointer = pointerAlong(m.size, m.pointer);
    m_paintedPointer = pointer;

    const qreal dpr = devicePixelRatioF();
    const int thickness = qRound(kThickness * dpr);
    const int crossX = qRound(m_cross.x() * dpr);
    const int crossY = qRound(m_cross.y() * dpr);
    const QRectF hArm(0, crossY, m.size.width(), thickness);
    const QRectF vArm(crossX, 0, thickness, m.size.height());

    QPainter p(this);
    // The ticks in screen pixels, one per pixel they mark, also at fractional
    // scaling, where logical coordinates fall between screen pixels.
    p.setWorldTransform(QTransform::fromScale(1.0 / dpr, 1.0 / dpr));
    p.fillRect(hArm, kBody);
    p.fillRect(vArm, kBody);
    p.setBrush(Qt::NoBrush);

    // The measuring edges, crossing; haloed on the arms' side.
    p.setPen(QPen(kHalo, 1));
    p.drawLine(QPointF(0, crossY + 1.5), QPointF(m.size.width(), crossY + 1.5));
    p.drawLine(QPointF(crossX + 1.5, 0), QPointF(crossX + 1.5, m.size.height()));
    p.setPen(QPen(kInk, 1));
    p.drawLine(QPointF(0, crossY + 0.5), QPointF(m.size.width(), crossY + 0.5));
    p.drawLine(QPointF(crossX + 0.5, 0), QPointF(crossX + 0.5, m.size.height()));

    if (m_hovered) {
        QPainterPath outline;
        outline.addRect(hArm.adjusted(0.5, 0.5, -0.5, -0.5));
        outline.addRect(vArm.adjusted(0.5, 0.5, -0.5, -0.5));
        p.drawPath(outline.simplified());

        // The ticks, counted from the cross both ways, with a halo beside
        // the longer ones (a pixel along, where no tick is). Where the arms
        // meet, each keeps clear of the other's.
        const int corner = qRound(tickLength(0) * dpr) + 1;
        const auto drawTicks = [&](int length, int cross, const auto &line) {
            for (int i = cross % 2; i < length; i += 2) {
                const int from = std::abs(i - cross);
                if (i > cross && from <= corner)
                    continue;
                const qreal tick = std::round(tickLength(from) * dpr);
                if (from % 10 == 0 && i + 1 < length) {
                    p.setPen(QPen(kHalo, 1));
                    p.drawLine(line(i + 1.5, 2.0, tick));
                    p.setPen(QPen(kInk, 1));
                }
                p.drawLine(line(i + 0.5, 1.0, tick));
            }
        };
        drawTicks(m.size.width(), crossX, [crossY](qreal t, qreal from, qreal to) {
            return QLineF(t, crossY + from, t, crossY + to);
        });
        drawTicks(m.size.height(), crossY, [crossX](qreal t, qreal from, qreal to) {
            return QLineF(crossX + from, t, crossX + to, t);
        });
    }

    p.setPen(QPen(kPointer, 1));
    if (pointer.x() >= 0)
        p.drawLine(QPointF(pointer.x() + 0.5, hArm.top()), QPointF(pointer.x() + 0.5, hArm.bottom()));
    if (pointer.y() >= 0)
        p.drawLine(QPointF(vArm.left(), pointer.y() + 0.5), QPointF(vArm.right(), pointer.y() + 0.5));

    // Text in logical coordinates, at the font's usual size.
    p.resetTransform();
    QFont font = p.font();
    font.setPointSizeF(font.pointSizeF() * 0.85);
    p.setFont(font);
    const QFontMetrics fm(font);
    const QRectF h = horizontalArm();
    const QRectF v = verticalArm();

    // The arms' lengths, at their far ends.
    if (m_hovered) {
        const QString hText = tr("%1 px").arg(m.size.width());
        const QSizeF hSize = fm.size(Qt::TextSingleLine, hText);
        drawHaloText(p, QRectF(QPointF(h.right() - hSize.width() - 5, h.bottom() - hSize.height() - 3), hSize), hText);
        const QString vText = tr("%1 px").arg(m.size.height());
        const QSizeF vSize = fm.size(Qt::TextSingleLine, vText);
        drawHaloText(p, QRectF(QPointF(v.right() - vSize.width() - 5, v.bottom() - vSize.height() - 3), vSize), vText);
    }

    // The pointer's distance from the cross along each arm (negative to the
    // left and above), beside its line; on the other side of it near the end.
    if (pointer.x() >= 0) {
        const QString text = tr("%1 px").arg(pointer.x() - crossX);
        const QSizeF size = QSizeF(fm.size(Qt::TextSingleLine, text)) + QSizeF(8, 4);
        const qreal t = (pointer.x() + 0.5) / dpr;
        const qreal x = t + 4 + size.width() <= h.right() - 2 ? t + 4 : t - 4 - size.width();
        drawReadout(p, QRectF(QPointF(x, h.bottom() - size.height() - 3), size), text);
    }
    if (pointer.y() >= 0) {
        const QString text = tr("%1 px").arg(pointer.y() - crossY);
        const QSizeF size = QSizeF(fm.size(Qt::TextSingleLine, text)) + QSizeF(8, 4);
        const qreal t = (pointer.y() + 0.5) / dpr;
        const qreal y = t + 4 + size.height() <= v.bottom() - 2 ? t + 4 : t - 4 - size.height();
        drawReadout(p, QRectF(QPointF(v.right() - size.width() - 3, y), size), text);
    }

    if (!m_hint.isEmpty()) {
        const QSizeF size = QSizeF(fm.size(Qt::TextSingleLine, m_hint)) + QSizeF(12, 6);
        QRectF box(QPointF(0, 0), size);
        box.moveCenter(h.intersected(v).center());
        p.setPen(Qt::NoPen);
        p.setBrush(QColor(0, 0, 0, 190));
        p.drawRoundedRect(box, 3, 3);
        p.setPen(Qt::white);
        p.drawText(box, Qt::AlignCenter, m_hint);
    }
}

// All of it on the screen it is on, made smaller if need be. It moves as a
// whole, keeping its shape, even if the cross then leaves the pointer.
void RulerWindow::keepOnScreen()
{
    const QScreen *screen = QGuiApplication::screenAt(geometry().center());
    if (!screen)
        screen = QGuiApplication::screenAt(QCursor::pos());
    if (!screen)
        screen = QGuiApplication::primaryScreen();
    const QRect area = screen->geometry();
    QRect r = geometry();
    r.setSize(r.size().boundedTo(area.size()));
    r.moveLeft(std::clamp(r.left(), area.left(), area.right() - r.width() + 1));
    r.moveTop(std::clamp(r.top(), area.top(), area.bottom() - r.height() + 1));
    setLayout(r, m_cross);
}

RulerWindow::End RulerWindow::endAt(const QPoint &pos) const
{
    if (horizontalArm().contains(pos)) {
        if (pos.x() < kGrip)
            return End::Left;
        if (pos.x() >= width() - kGrip)
            return End::Right;
    }
    if (verticalArm().contains(pos)) {
        if (pos.y() < kGrip)
            return End::Top;
        if (pos.y() >= height() - kGrip)
            return End::Bottom;
    }
    return End::None;
}

void RulerWindow::setHovered(bool hovered)
{
    if (hovered == m_hovered)
        return;
    m_hovered = hovered;
    update();
}

void RulerWindow::enterEvent(QEnterEvent *event)
{
    setHovered(true);
    QWidget::enterEvent(event);
}

void RulerWindow::leaveEvent(QEvent *event)
{
    if (m_resizing == End::None)
        setHovered(false);
    QWidget::leaveEvent(event);
}

// Also whether the pointer is over an arm (and not over a window above it):
// Windows does not reliably report the pointer entering a window that is
// not active.
void RulerWindow::pollPointer()
{
    const QPoint global = QCursor::pos();
    const QPoint local = mapFromGlobal(global);
    const bool overArm = horizontalArm().contains(local) || verticalArm().contains(local);
    setHovered(m_resizing != End::None || (overArm && QApplication::topLevelAt(global) == this));

    const Measure m = measure();
    if (pointerAlong(m.size, m.pointer) != m_paintedPointer)
        update();
    updateAngle(m);
}

// Shown while Ctrl is held anywhere, figured in screen pixels like the
// arms' readouts.
void RulerWindow::updateAngle(const Measure &m)
{
    const bool held = QGuiApplication::queryKeyboardModifiers() & Qt::ControlModifier;
    if (!held || m_resizing != End::None || !isVisible()) {
        if (m_angle)
            m_angle->hide();
        return;
    }
    if (!m_angle)
        m_angle = new RulerAngleOverlay(this);
    const QScreen *screen = this->screen();
    if (m_angle->geometry() != screen->geometry())
        m_angle->setGeometry(screen->geometry());

    const qreal dpr = devicePixelRatioF();
    const QPoint cross(qRound(m_cross.x() * dpr), qRound(m_cross.y() * dpr));
    const QPoint d = m.pointer - cross; // right and down are positive, as on the arms
    const QPointF crossAt = QPointF(geometry().topLeft() - screen->geometry().topLeft())
                            + (QPointF(cross) + QPointF(0.5, 0.5)) / dpr;

    QStringList lines;
    lines << tr("Δx %1  Δy %2  ·  %3 px").arg(d.x()).arg(d.y()).arg(std::hypot(d.x(), d.y()), 0, 'f', 1);
    if (d.isNull()) {
        lines << tr("On the cross: no angle");
    } else {
        // The acute angles to the axes, which add up to 90° (π/2).
        const qreal toX = std::atan2(std::abs(d.y()), std::abs(d.x()));
        const auto angleText = [this](qreal angle) {
            return m_radians ? QString::number(angle / std::numbers::pi, 'f', 4) + QStringLiteral("π")
                             : QString::number(angle * 180 / std::numbers::pi, 'f', 2) + QStringLiteral("°");
        };
        const auto figures = [](qreal angle) {
            const QString tan = std::abs(std::cos(angle)) < 1e-12 ? QStringLiteral("∞")
                                                                 : QString::number(std::tan(angle), 'f', 4);
            return QStringLiteral("sin %1   cos %2   tan %3")
                .arg(QString::number(std::sin(angle), 'f', 4), QString::number(std::cos(angle), 'f', 4), tan);
        };
        const qreal toY = std::numbers::pi / 2 - toX;
        lines << tr("To the x-axis %1").arg(angleText(toX)) + QStringLiteral("   ") + figures(toX);
        lines << tr("To the y-axis %1").arg(angleText(toY)) + QStringLiteral("   ") + figures(toY);
    }
    m_angle->set(crossAt, crossAt + QPointF(d) / dpr, lines);
    if (!m_angle->isVisible())
        m_angle->show();
}

void RulerWindow::mousePressEvent(QMouseEvent *event)
{
    activateWindow(); // for the keys
    if (event->button() != Qt::LeftButton)
        return;
    m_resizing = endAt(event->position().toPoint());
    if (m_resizing != End::None) {
        // Done here rather than by the window system, which would move the
        // cross for the left and top ends.
        m_pressGlobal = event->globalPosition().toPoint();
        m_pressGeometry = geometry();
        m_pressCross = m_cross;
        return;
    }
    // The window system moves the window, which also works on Wayland.
    if (windowHandle())
        windowHandle()->startSystemMove();
}

void RulerWindow::mouseMoveEvent(QMouseEvent *event)
{
    if (m_resizing == End::None) {
        switch (endAt(event->position().toPoint())) {
        case End::Left:
        case End::Right: setCursor(Qt::SizeHorCursor); break;
        case End::Top:
        case End::Bottom: setCursor(Qt::SizeVerCursor); break;
        case End::None: setCursor(Qt::SizeAllCursor); break;
        }
        if (!m_pointerTimer.isActive())
            update();
        return;
    }

    // Each end on its own, so the arms can be made uneven; no further in
    // than the other arm, and the cross stays put.
    const QPoint delta = event->globalPosition().toPoint() - m_pressGlobal;
    QRect r = m_pressGeometry;
    QPoint cross = m_pressCross;
    switch (m_resizing) {
    case End::Left: {
        const int dx = std::min(delta.x(), m_pressCross.x());
        r.setLeft(r.left() + dx);
        cross.rx() -= dx;
        break;
    }
    case End::Right:
        r.setRight(std::max(r.right() + delta.x(), r.left() + m_pressCross.x() + kThickness - 1));
        break;
    case End::Top: {
        const int dy = std::min(delta.y(), m_pressCross.y());
        r.setTop(r.top() + dy);
        cross.ry() -= dy;
        break;
    }
    case End::Bottom:
        r.setBottom(std::max(r.bottom() + delta.y(), r.top() + m_pressCross.y() + kThickness - 1));
        break;
    case End::None: break;
    }
    setLayout(r, cross);
}

void RulerWindow::mouseReleaseEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton)
        m_resizing = End::None;
}

void RulerWindow::mouseDoubleClickEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton)
        close();
}

void RulerWindow::wheelEvent(QWheelEvent *event)
{
    const int steps = event->angleDelta().y() / 120;
    if (steps == 0 || !(event->modifiers() & Qt::ControlModifier))
        return;
    const qreal opacity = std::clamp(windowOpacity() + steps * kOpacityStep, kMinOpacity, 1.0);
    setWindowOpacity(opacity);
    showHint(tr("Opacity %1%").arg(qRound(opacity * 100)));
}

// Arrows move the ruler a pixel (Shift: ten); with Ctrl, they move the
// right and bottom ends instead.
void RulerWindow::keyPressEvent(QKeyEvent *event)
{
    QPoint delta;
    switch (event->key()) {
    case Qt::Key_Left: delta = QPoint(-1, 0); break;
    case Qt::Key_Right: delta = QPoint(1, 0); break;
    case Qt::Key_Up: delta = QPoint(0, -1); break;
    case Qt::Key_Down: delta = QPoint(0, 1); break;
    default: QWidget::keyPressEvent(event); return;
    }
    if (event->modifiers() & Qt::ShiftModifier)
        delta *= 10;
    if (event->modifiers() & Qt::ControlModifier) {
        const QSize least(m_cross.x() + kThickness, m_cross.y() + kThickness);
        const QSize size = (this->size() + QSize(delta.x(), delta.y())).expandedTo(least);
        setLayout(QRect(pos(), size), m_cross);
        return;
    }
    move(pos() + delta);
}

void RulerWindow::contextMenuEvent(QContextMenuEvent *event)
{
    QMenu menu(this);
    // The unit of the angles shown while Ctrl is held.
    auto *units = new QActionGroup(&menu);
    QAction *degrees = menu.addAction(tr("Angles in &Degrees"));
    QAction *radians = menu.addAction(tr("Angles in &Radians (× π)"));
    for (QAction *action : {degrees, radians}) {
        action->setCheckable(true);
        units->addAction(action);
    }
    (m_radians ? radians : degrees)->setChecked(true);
    connect(units, &QActionGroup::triggered, this, [this, radians](QAction *action) {
        m_radians = action == radians;
        QSettings().setValue(kRadiansKey, m_radians);
    });
    menu.addSeparator();
    menu.addAction(m_closeAction);
    menu.addSeparator();
    QAction *help = menu.addAction(tr("Drag an end: length  ·  Arrows: move  ·  Ctrl+arrows: length  ·  Ctrl+scroll: opacity  ·  Hold Ctrl: angle"));
    help->setEnabled(false);
    menu.exec(event->globalPos());
}

// The next ruler comes the same shape.
void RulerWindow::closeEvent(QCloseEvent *event)
{
    QSettings settings;
    settings.setValue(kSizeKey, size());
    settings.setValue(kCrossKey, m_cross);
    QWidget::closeEvent(event);
}

void RulerWindow::changeEvent(QEvent *event)
{
    if (event->type() == QEvent::LanguageChange)
        retranslate();
    QWidget::changeEvent(event);
}

void RulerWindow::showHint(const QString &text)
{
    m_hint = text;
    m_hintTimer.start();
    update();
}

void RulerWindow::retranslate()
{
    setWindowTitle(tr("Screen Ruler"));
    m_closeAction->setText(tr("C&lose"));
}
