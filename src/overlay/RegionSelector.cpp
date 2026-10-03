#include "RegionSelector.h"

#include <QCursor>
#include <QGuiApplication>
#include <QKeyEvent>
#include <QMouseEvent>
#include <QPainter>
#include <QScreen>
#include <QWheelEvent>
#include <QWidget>

#include <algorithm>

namespace {

const QColor kAccent(0x2d, 0x9c, 0xff);
const QColor kDim(0, 0, 0, 110);
// Movement (logical px) before a press turns into a drag, so a slightly
// shaky click in window mode still picks the window.
constexpr int kDragThreshold = 4;
constexpr int kMaxTitleWidth = 360;

QScreen *findScreen(const QString &name)
{
    const auto screens = QGuiApplication::screens();
    for (QScreen *screen : screens) {
        if (screen->name() == name)
            return screen;
    }
    return nullptr;
}

// Draws a small dark tag; prefers sitting just above `anchor`, falls back to inside.
void drawTag(QPainter &p, const QRect &bounds, const QPoint &anchor, const QString &text)
{
    const QFontMetrics fm = p.fontMetrics();
    const QSize size = fm.size(Qt::TextSingleLine, text) + QSize(12, 6);
    QPoint pos(anchor.x(), anchor.y() - size.height() - 4);
    if (pos.y() < bounds.top())
        pos.setY(anchor.y() + 4);
    pos.setX(std::clamp(pos.x(), bounds.left(), std::max(bounds.left(), bounds.right() - size.width())));

    const QRect box(pos, size);
    p.setPen(Qt::NoPen);
    p.setBrush(QColor(0, 0, 0, 190));
    p.drawRoundedRect(box, 3, 3);
    p.setPen(Qt::white);
    p.drawText(box, Qt::AlignCenter, text);
}

} // namespace

namespace detail {

class SelectionOverlay : public QWidget
{
public:
    SelectionOverlay(RegionSelector *selector, const ScreenImage &shot)
        : QWidget(nullptr, Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint | Qt::Tool)
        , m_selector(selector)
        , m_image(shot.image)
        , m_origin(shot.geometry.topLeft())
    {
        setAttribute(Qt::WA_OpaquePaintEvent);
        setAttribute(Qt::WA_NoSystemBackground);
        setMouseTracking(true);
        setCursor(Qt::CrossCursor);
        if (QScreen *screen = findScreen(shot.name))
            setScreen(screen);
        setGeometry(shot.geometry);
    }

    QRect logicalGeometry() const { return QRect(m_origin, size()); }

protected:
    void paintEvent(QPaintEvent *) override
    {
        QPainter p(this);
        p.drawImage(QPointF(0, 0), m_image);
        p.fillRect(rect(), kDim);

        if (m_selector->mode() == RegionSelector::Mode::Freehand && m_selector->freehandPath().size() > 1) {
            drawFreehand(p, m_selector->freehandPath().translated(-m_origin));
            return;
        }

        const QRect sel = m_selector->selection().translated(-m_origin);
        if (!sel.isEmpty()) {
            if (sel.intersects(rect())) {
                drawHighlight(p, sel, 1);
                drawTag(p, rect(), sel.topLeft(), sizeText(sel));
            }
            return;
        }

        if (m_selector->mode() == RegionSelector::Mode::Window) {
            const CaptureTarget target = m_selector->hoverTarget();
            const QRect local = target.geometry.translated(-m_origin);
            if (local.intersects(rect())) {
                drawHighlight(p, local, 2);
                const QString title = p.fontMetrics().elidedText(target.title, Qt::ElideRight, kMaxTitleWidth);
                drawTag(p, rect(), local.intersected(rect()).topLeft(),
                        title.isEmpty() ? sizeText(local) : QStringLiteral("%1  ·  %2").arg(title, sizeText(local)));
            }
            return;
        }

        const QPoint c = m_selector->cursor() - m_origin;
        if (!rect().contains(c))
            return;
        p.setPen(QPen(kAccent, 1, Qt::DashLine));
        p.drawLine(QPointF(0, c.y() + 0.5), QPointF(width(), c.y() + 0.5));
        p.drawLine(QPointF(c.x() + 0.5, 0), QPointF(c.x() + 0.5, height()));
        const qreal dpr = m_image.devicePixelRatio();
        drawTag(p, rect(), c + QPoint(12, 0),
                QStringLiteral("%1, %2").arg(qRound(c.x() * dpr)).arg(qRound(c.y() * dpr)));
    }

    void mousePressEvent(QMouseEvent *event) override
    {
        if (event->button() == Qt::LeftButton) {
            m_selector->press(toGlobal(event));
        } else if (event->button() == Qt::RightButton) {
            // Right-click drops an in-progress selection first, then cancels.
            if (m_selector->isDragging())
                m_selector->resetSelection();
            else
                m_selector->cancel();
        }
    }

    void mouseMoveEvent(QMouseEvent *event) override { m_selector->move(toGlobal(event)); }

    void mouseReleaseEvent(QMouseEvent *event) override
    {
        if (event->button() == Qt::LeftButton)
            m_selector->release(toGlobal(event));
    }

    void wheelEvent(QWheelEvent *event) override
    {
        const int delta = event->angleDelta().y();
        if (delta != 0)
            m_selector->wheel(delta > 0 ? 1 : -1);
    }

    void keyPressEvent(QKeyEvent *event) override
    {
        if (event->key() == Qt::Key_Escape)
            m_selector->cancel();
        else
            QWidget::keyPressEvent(event);
    }

private:
    // Built from the local position plus our known origin: on Wayland a window
    // does not know its global position, so globalPosition() is unreliable there.
    QPoint toGlobal(const QMouseEvent *event) const { return m_origin + event->position().toPoint(); }

    QString sizeText(const QRect &r) const
    {
        const qreal dpr = m_image.devicePixelRatio();
        return QStringLiteral("%1 × %2").arg(qRound(r.width() * dpr)).arg(qRound(r.height() * dpr));
    }

    // Shows `r` (local coordinates) undimmed with an accent border.
    void drawHighlight(QPainter &p, const QRect &r, int borderWidth)
    {
        const qreal dpr = m_image.devicePixelRatio();
        const QRect visible = r.intersected(rect());
        p.drawImage(QRectF(visible), m_image,
                    QRectF(visible.x() * dpr, visible.y() * dpr, visible.width() * dpr, visible.height() * dpr));
        const qreal inset = borderWidth / 2.0;
        p.setPen(QPen(kAccent, borderWidth));
        p.setBrush(Qt::NoBrush);
        p.drawRect(QRectF(r).adjusted(inset, inset, -inset, -inset));
    }

    // Shows the inside of the outline undimmed; the dashed segment shows how
    // releasing will close it.
    void drawFreehand(QPainter &p, const QPolygon &path)
    {
        QPainterPath inside;
        inside.addPolygon(path);
        inside.closeSubpath();
        p.save();
        p.setClipPath(inside);
        p.drawImage(QPointF(0, 0), m_image);
        p.restore();

        p.setRenderHint(QPainter::Antialiasing);
        p.setPen(QPen(kAccent, 2, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
        p.drawPolyline(path);
        p.setPen(QPen(kAccent, 1, Qt::DashLine));
        p.drawLine(path.last(), path.first());

        const QRect bounds = path.boundingRect();
        if (bounds.intersects(rect()))
            drawTag(p, rect(), bounds.topLeft(), sizeText(bounds));
    }

    RegionSelector *m_selector;
    QImage m_image;
    QPoint m_origin;
};

} // namespace detail

RegionSelector::RegionSelector(const DesktopSnapshot &snapshot, Mode mode, QObject *parent)
    : QObject(parent)
    , m_snapshot(snapshot)
    , m_mode(mode)
{
}

RegionSelector::~RegionSelector()
{
    qDeleteAll(m_overlays);
}

void RegionSelector::start()
{
    m_cursor = QCursor::pos();
    updateHover();

    for (const ScreenImage &shot : std::as_const(m_snapshot.screens)) {
        auto *overlay = new detail::SelectionOverlay(this, shot);
        m_overlays.append(overlay);
        overlay->showFullScreen();
    }

    detail::SelectionOverlay *focus = m_overlays.isEmpty() ? nullptr : m_overlays.first();
    for (detail::SelectionOverlay *overlay : std::as_const(m_overlays)) {
        if (overlay->logicalGeometry().contains(m_cursor))
            focus = overlay;
    }
    if (focus) {
        focus->raise();
        focus->activateWindow();
        focus->setFocus();
    }
}

QRect RegionSelector::selection() const
{
    if (!m_dragging || m_mode == Mode::Freehand)
        return {};
    const int left = std::min(m_anchor.x(), m_cursor.x());
    const int top = std::min(m_anchor.y(), m_cursor.y());
    const int right = std::max(m_anchor.x(), m_cursor.x());
    const int bottom = std::max(m_anchor.y(), m_cursor.y());
    return QRect(left, top, right - left, bottom - top);
}

CaptureTarget RegionSelector::hoverTarget() const
{
    if (m_mode != Mode::Window || m_hoverLevel < 0 || m_hoverLevel >= m_hoverChain.size())
        return {};
    return m_hoverChain.at(m_hoverLevel);
}

void RegionSelector::press(const QPoint &pos)
{
    m_anchor = m_cursor = pos;
    m_pressed = true;
    m_dragging = m_mode != Mode::Window;
    m_path.clear();
    if (m_mode == Mode::Freehand)
        m_path.append(pos);
    updateOverlays();
}

void RegionSelector::move(const QPoint &pos)
{
    m_cursor = pos;
    if (m_mode == Mode::Freehand && m_dragging && m_path.last() != pos)
        m_path.append(pos);
    if (m_pressed && !m_dragging && (pos - m_anchor).manhattanLength() >= kDragThreshold)
        m_dragging = true;
    if (!m_dragging)
        updateHover();
    updateOverlays();
}

void RegionSelector::release(const QPoint &pos)
{
    if (!m_pressed)
        return;
    m_pressed = false;
    m_cursor = pos;

    if (!m_dragging) {
        // A click: in window mode it takes the highlighted target.
        const QRect target = hoverTarget().geometry;
        if (!target.isEmpty())
            finish(target);
        return;
    }

    if (m_mode == Mode::Freehand) {
        if (m_path.last() != pos)
            m_path.append(pos);
        const QRect bounds = m_path.boundingRect();
        if (m_path.size() < 3 || bounds.width() < 2 || bounds.height() < 2) {
            resetSelection();
            return;
        }
        m_shape = QPainterPath();
        m_shape.addPolygon(m_path);
        m_shape.closeSubpath();
        finish(bounds);
        return;
    }

    const QRect rect = selection();
    if (rect.width() < 2 || rect.height() < 2) {
        resetSelection();
        return;
    }
    finish(rect);
}

void RegionSelector::wheel(int steps)
{
    if (m_mode != Mode::Window || m_dragging || m_hoverChain.isEmpty())
        return;
    // Wheel up walks out towards the top-level window, down walks back in.
    const qsizetype level = std::clamp<qsizetype>(m_hoverLevel - steps, 0, m_hoverChain.size() - 1);
    if (level != m_hoverLevel) {
        m_hoverLevel = level;
        updateOverlays();
    }
}

void RegionSelector::resetSelection()
{
    m_pressed = false;
    m_dragging = false;
    m_path.clear();
    updateHover();
    updateOverlays();
}

void RegionSelector::cancel()
{
    m_pressed = false;
    m_dragging = false;
    closeOverlays();
    emit canceled();
}

void RegionSelector::finish(const QRect &rect)
{
    m_pressed = false;
    m_dragging = false;
    closeOverlays();
    emit selected(rect);
}

void RegionSelector::updateHover()
{
    if (m_mode != Mode::Window)
        return;

    QList<CaptureTarget> chain = m_snapshot.targetsAt(m_cursor);
    if (chain.isEmpty()) {
        // Nothing but desktop under the cursor: offer the whole monitor.
        for (const ScreenImage &shot : std::as_const(m_snapshot.screens)) {
            if (shot.geometry.contains(m_cursor)) {
                chain.append({shot.geometry, tr("Screen %1").arg(shot.name)});
                break;
            }
        }
    }

    const auto sameGeometry = [](const QList<CaptureTarget> &a, const QList<CaptureTarget> &b) {
        return std::equal(a.begin(), a.end(), b.begin(), b.end(),
                          [](const CaptureTarget &x, const CaptureTarget &y) { return x.geometry == y.geometry; });
    };
    // Keep a level chosen with the wheel while the cursor stays over the same stack.
    if (sameGeometry(chain, m_hoverChain))
        return;
    m_hoverChain = chain;
    m_hoverLevel = m_hoverChain.size() - 1; // innermost object by default
}

void RegionSelector::closeOverlays()
{
    for (detail::SelectionOverlay *overlay : std::as_const(m_overlays))
        overlay->hide();
}

void RegionSelector::updateOverlays()
{
    for (detail::SelectionOverlay *overlay : std::as_const(m_overlays))
        overlay->update();
}
