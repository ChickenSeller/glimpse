#include "RegionSelector.h"

#include "capture/Platform.h"

#include <QClipboard>
#include <QCursor>
#include <QGuiApplication>
#include <QKeyEvent>
#include <QMouseEvent>
#include <QPainter>
#include <QScreen>
#include <QWheelEvent>
#include <QWidget>

#include <algorithm>
#include <cmath>

namespace {

const QColor kAccent(0x2d, 0x9c, 0xff);
const QColor kDim(0, 0, 0, 110);
const QColor kCrosshair(0xff, 0x40, 0x40);
// Movement (logical px) before a press turns into a drag, so a slightly
// shaky click in window mode still picks the window.
constexpr int kDragThreshold = 4;
// Color mode magnifier: kZoomCells × kZoomCells native pixels, kZoomCell px each.
constexpr int kZoomCells = 15;
constexpr int kZoomCell = 8;

// The native pixel of `image` (whose top-left is at logical `origin`) under
// the logical point `pos`, moved by `nudge` and kept inside the image.
QPoint samplePixel(const QImage &image, const QPoint &origin, const QPointF &pos, const QPoint &nudge)
{
    const qreal dpr = image.devicePixelRatio();
    const QPointF local = (pos - QPointF(origin)) * dpr;
    return QPoint(std::clamp(int(std::floor(local.x())) + nudge.x(), 0, image.width() - 1),
                  std::clamp(int(std::floor(local.y())) + nudge.y(), 0, image.height() - 1));
}

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

// "Title (app.exe)  ·  ClassName  ·  800 × 600", as wide as it needs. Only a
// title too long for the screen is shortened; the rest always shows whole.
QString windowLabel(const QFontMetrics &fm, const CaptureTarget &target, const QString &size, int screenWidth)
{
    QStringList tail;
    if (!target.className.isEmpty())
        tail << target.className;
    tail << size;
    const QString separator = QStringLiteral("  \u00b7  ");
    if (target.title.isEmpty()) {
        if (!target.program.isEmpty())
            tail.prepend(target.program);
        return tail.join(separator);
    }
    const QString program = target.program.isEmpty() ? QString() : QStringLiteral(" (%1)").arg(target.program);
    const QString rest = program + separator + tail.join(separator);
    const int room = screenWidth - 24 - fm.horizontalAdvance(rest);
    return fm.elidedText(target.title, Qt::ElideRight, std::max(room, fm.horizontalAdvance(QStringLiteral("\u2026")))) + rest;
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
        , m_name(shot.name)
    {
        setAttribute(Qt::WA_OpaquePaintEvent);
        setAttribute(Qt::WA_NoSystemBackground);
        setMouseTracking(true);
        // The crosshair itself marks the pointer.
        setCursor(selector->mode() == RegionSelector::Mode::Crosshair ? Qt::BlankCursor : Qt::CrossCursor);
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
        if (m_selector->mode() == RegionSelector::Mode::Color) {
            // Undimmed: the colors on screen are the ones being picked.
            drawMagnifier(p);
            return;
        }
        if (m_selector->mode() == RegionSelector::Mode::Crosshair) {
            drawCrosshair(p);
            return;
        }
        if (m_selector->mode() == RegionSelector::Mode::Freehand && m_selector->freehandPath().size() > 1) {
            p.fillRect(rect(), kDim);
            drawFreehand(p, m_selector->freehandPath().translated(-m_origin));
            return;
        }

        const QRect sel = m_selector->selection().translated(-m_origin);
        if (!sel.isEmpty()) {
            drawHighlight(p, sel, 1);
            if (sel.intersects(rect()))
                drawTag(p, rect(), sel.topLeft(), sizeText(sel));
            return;
        }

        if (m_selector->mode() == RegionSelector::Mode::Window) {
            const CaptureTarget target = m_selector->hoverTarget();
            const QRect local = target.geometry.translated(-m_origin);
            drawHighlight(p, local, 2);
            if (local.intersects(rect())) {
                drawTag(p, rect(), local.intersected(rect()).topLeft(), windowLabel(p.fontMetrics(), target, sizeText(local), rect().width()));
            }
            return;
        }

        p.fillRect(rect(), kDim);
        const QPointF c = m_selector->preciseCursor() - QPointF(m_origin);
        if (!QRectF(rect()).contains(c))
            return;
        // Guides through the native pixel under the pointer, one native pixel wide.
        const qreal dpr = m_image.devicePixelRatio();
        const QPoint pixel(int(std::floor(c.x() * dpr)), int(std::floor(c.y() * dpr)));
        p.save();
        useNativePixels(p);
        p.setPen(QPen(kAccent, 1, Qt::DashLine));
        p.drawLine(QPointF(0, pixel.y() + 0.5), QPointF(m_image.width(), pixel.y() + 0.5));
        p.drawLine(QPointF(pixel.x() + 0.5, 0), QPointF(pixel.x() + 0.5, m_image.height()));
        p.restore();
        drawTag(p, rect(), c.toPoint() + QPoint(12, 0), QStringLiteral("%1, %2").arg(pixel.x()).arg(pixel.y()));
    }

    void mousePressEvent(QMouseEvent *event) override
    {
        if (m_selector->mode() == RegionSelector::Mode::Crosshair) {
            m_selector->setPreciseCursor(preciseGlobal(event));
            if (event->button() == Qt::LeftButton)
                m_selector->lockAtCursor();
            else if (event->button() == Qt::RightButton && m_selector->lockedSample())
                m_selector->unlock();
            else if (event->button() == Qt::RightButton)
                m_selector->cancel();
            return;
        }
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

    void mouseMoveEvent(QMouseEvent *event) override
    {
        m_selector->setPreciseCursor(preciseGlobal(event));
        m_selector->move(toGlobal(event));
    }

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
        if (event->key() == Qt::Key_Escape) {
            m_selector->cancel();
            return;
        }
        if (m_selector->mode() == RegionSelector::Mode::Crosshair && event->key() == Qt::Key_C
            && event->modifiers().testFlag(Qt::ControlModifier)) {
            if (event->modifiers().testFlag(Qt::ShiftModifier))
                m_selector->copyColor();
            else
                m_selector->copyPosition();
            return;
        }
        if (m_selector->mode() == RegionSelector::Mode::Crosshair) {
            switch (event->key()) {
            case Qt::Key_Left: m_selector->nudgeBy(-1, 0); return;
            case Qt::Key_Right: m_selector->nudgeBy(1, 0); return;
            case Qt::Key_Up: m_selector->nudgeBy(0, -1); return;
            case Qt::Key_Down: m_selector->nudgeBy(0, 1); return;
            case Qt::Key_Return:
            case Qt::Key_Enter:
            case Qt::Key_Space: m_selector->lockAtCursor(); return;
            default: break;
            }
        }
        if (m_selector->mode() == RegionSelector::Mode::Color) {
            switch (event->key()) {
            case Qt::Key_Left: m_selector->nudgeBy(-1, 0); return;
            case Qt::Key_Right: m_selector->nudgeBy(1, 0); return;
            case Qt::Key_Up: m_selector->nudgeBy(0, -1); return;
            case Qt::Key_Down: m_selector->nudgeBy(0, 1); return;
            case Qt::Key_Return:
            case Qt::Key_Enter:
            case Qt::Key_Space: m_selector->pickColor(); return;
            default: break;
            }
        }
        QWidget::keyPressEvent(event);
    }

private:
    // Built from the local position plus our known origin: on Wayland a window
    // does not know its global position, so globalPosition() is unreliable there.
    QPoint toGlobal(const QMouseEvent *event) const { return m_origin + event->position().toPoint(); }

    // The pointer's logical position, at the center of the physical pixel
    // under its hotspot where the platform tells it exactly.
    QPointF preciseGlobal(const QMouseEvent *event) const
    {
        if (const std::optional<QPoint> native = Platform::nativeCursorPos()) {
            const qreal dpr = m_image.devicePixelRatio();
            const QPoint nativeOrigin(qRound(m_origin.x() * dpr), qRound(m_origin.y() * dpr));
            const QPointF local = (QPointF(*native - nativeOrigin) + QPointF(0.5, 0.5)) / dpr;
            if (QRectF(rect()).contains(local))
                return QPointF(m_origin) + local;
        }
        return QPointF(m_origin) + event->position();
    }

    // The native pixels a capture of `r` (local logical coordinates) takes:
    // the same rounding as DesktopSnapshot::crop.
    QRect toNative(const QRect &r) const
    {
        const qreal dpr = m_image.devicePixelRatio();
        return QRectF(r.x() * dpr, r.y() * dpr, r.width() * dpr, r.height() * dpr).toAlignedRect();
    }

    // Makes painter coordinates native pixels of this screen. At fractional
    // scaling (150%) logical coordinates fall between native pixels.
    void useNativePixels(QPainter &p) const
    {
        const qreal scale = 1.0 / m_image.devicePixelRatio();
        p.setWorldTransform(QTransform::fromScale(scale, scale));
    }

    QString sizeText(const QRect &r) const
    {
        const QRect native = toNative(r);
        return QStringLiteral("%1 × %2").arg(native.width()).arg(native.height());
    }

    // Dims everything but `r` (local coordinates) and borders it. The
    // screenshot underneath is left as drawn: drawing it again for the
    // undimmed part would resample it whenever `r` starts between native
    // pixels, and the content would jitter while dragging.
    void drawHighlight(QPainter &p, const QRect &r, int borderWidth)
    {
        const qreal dpr = m_image.devicePixelRatio();
        const QRect native = toNative(r);
        const QRect all(QPoint(0, 0), m_image.size());
        p.save();
        useNativePixels(p);
        p.setClipRegion(QRegion(all).subtracted(QRegion(native)));
        p.fillRect(all, kDim);
        p.setClipping(false);
        const int width = std::max(1, qRound(borderWidth * dpr));
        const qreal inset = width / 2.0;
        p.setPen(QPen(kAccent, width));
        p.setBrush(Qt::NoBrush);
        p.drawRect(QRectF(native).adjusted(inset, inset, -inset, -inset));
        p.restore();
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

    // A magnifier of the pixels around `pixel` of `image`, with its color
    // swatch, `lines` of values and dimmer `hints`, placed beside `near`.
    void drawPanel(QPainter &p, const QImage &image, const QPoint &pixel, const QPointF &near,
                   const QStringList &lines, const QStringList &hints)
    {
        const QColor color = image.pixelColor(pixel);
        const QFontMetrics fm = p.fontMetrics();
        const int zoomSize = kZoomCells * kZoomCell;
        const int swatchSize = fm.height() * 2;
        int textWidth = 0;
        for (int i = 0; i < lines.size(); ++i)
            textWidth = std::max(textWidth, fm.horizontalAdvance(lines[i]) + (i < 2 ? swatchSize + 8 : 0));
        for (const QString &hint : hints)
            textWidth = std::max(textWidth, fm.horizontalAdvance(hint));
        const int textLines = std::max<int>(2, lines.size()) + hints.size();
        QRect box(0, 0, std::max(zoomSize, textWidth + 12), zoomSize + textLines * fm.height() + 12);
        // Beside the cursor, flipped to the other side near a screen edge.
        QPoint pos = near.toPoint() + QPoint(24, 24);
        if (pos.x() + box.width() > width())
            pos.setX(int(near.x()) - 24 - box.width());
        if (pos.y() + box.height() > height())
            pos.setY(int(near.y()) - 24 - box.height());
        box.moveTopLeft(pos);

        const int half = kZoomCells / 2;
        QImage area = image.copy(pixel.x() - half, pixel.y() - half, kZoomCells, kZoomCells);
        area.setDevicePixelRatio(1.0);
        const QRect zoom(box.left() + (box.width() - zoomSize) / 2, box.top(), zoomSize, zoomSize);
        p.fillRect(box, QColor(0x20, 0x20, 0x20)); // opaque: nothing behind may show through
        p.drawImage(zoom, area); // nearest-neighbour: every pixel a crisp square

        const QRect center(zoom.left() + half * kZoomCell, zoom.top() + half * kZoomCell, kZoomCell, kZoomCell);
        p.setPen(QPen(Qt::black, 1));
        p.drawRect(center.adjusted(-1, -1, 0, 0));
        p.setPen(QPen(Qt::white, 1));
        p.drawRect(center.adjusted(-2, -2, 1, 1));
        p.setPen(QPen(kAccent, 1));
        p.drawRect(box.adjusted(0, 0, -1, -1));

        const int left = box.left() + 6;
        int y = zoom.bottom() + 7;
        const QRect swatch(left, y, swatchSize, swatchSize);
        p.fillRect(swatch, color);
        p.setPen(QPen(Qt::white, 1));
        p.drawRect(swatch.adjusted(0, 0, -1, -1));
        for (int i = 0; i < lines.size(); ++i) {
            // The first two lines sit beside the swatch, the rest below it.
            const int x = i < 2 ? swatch.right() + 9 : left;
            p.drawText(x, y + i * fm.height() + fm.ascent(), lines[i]);
        }
        y += std::max<int>(2, lines.size()) * fm.height();
        p.setPen(QColor(255, 255, 255, 170));
        for (const QString &hint : hints) {
            p.drawText(left, y + fm.ascent(), hint);
            y += fm.height();
        }
    }

    // Color mode: the panel for the pixel under the cursor, on its screen only.
    void drawMagnifier(QPainter &p)
    {
        const RegionSelector::Sample sample = m_selector->sampleAtCursor();
        if (sample.screen != m_name)
            return;
        const QColor color = m_image.pixelColor(sample.pixel);
        drawPanel(p, m_image, sample.pixel, m_selector->preciseCursor() - QPointF(m_origin),
                  {color.name(QColor::HexRgb).toUpper(),
                   QStringLiteral("%1, %2, %3").arg(color.red()).arg(color.green()).arg(color.blue())},
                  {RegionSelector::tr("Click: pick  ·  Arrows: 1 px")});
    }

    // Center of a native pixel of this screen, in local logical coordinates.
    QPointF pixelCenter(const QPoint &pixel) const
    {
        const qreal dpr = m_image.devicePixelRatio();
        return QPointF((pixel.x() + 0.5) / dpr, (pixel.y() + 0.5) / dpr);
    }

    // Crosshair mode: lines through the locked (or hovered) pixel; with a lock,
    // a line to the cursor and the distance between them.
    void drawCrosshair(QPainter &p)
    {
        const RegionSelector::Sample live = m_selector->sampleAtCursor();
        const std::optional<RegionSelector::Sample> lock = m_selector->lockedSample();
        const RegionSelector::Sample target = lock ? *lock : live;

        if (target.screen == m_name) {
            const QPointF c = pixelCenter(target.pixel);
            // Dark under light, so the lines show on any background.
            for (const auto &[color, offset] : {std::pair{QColor(0, 0, 0, 150), 1.0}, std::pair{kCrosshair, 0.0}}) {
                p.setPen(QPen(color, 1));
                p.drawLine(QPointF(0, c.y() + offset), QPointF(width(), c.y() + offset));
                p.drawLine(QPointF(c.x() + offset, 0), QPointF(c.x() + offset, height()));
            }
        }
        if (lock && live.screen == m_name) {
            const QPointF cursor = pixelCenter(live.pixel);
            p.setRenderHint(QPainter::Antialiasing);
            if (lock->screen == m_name) {
                p.setPen(QPen(kCrosshair, 1, Qt::DashLine));
                p.drawLine(pixelCenter(lock->pixel), cursor);
            }
            p.setPen(QPen(kCrosshair, 1.5));
            p.drawLine(cursor - QPointF(6, 0), cursor + QPointF(6, 0));
            p.drawLine(cursor - QPointF(0, 6), cursor + QPointF(0, 6));
            p.setRenderHint(QPainter::Antialiasing, false);
        }
        if (live.screen != m_name)
            return;

        const ScreenImage *targetShot = m_selector->screenImage(target.screen);
        if (!targetShot)
            return;
        const QColor color = targetShot->image.pixelColor(target.pixel);
        const QPoint pos = m_selector->globalPixel(target);
        QStringList lines = {color.name(QColor::HexRgb).toUpper(),
                             QStringLiteral("%1, %2, %3").arg(color.red()).arg(color.green()).arg(color.blue()),
                             QStringLiteral("X %1   Y %2").arg(pos.x()).arg(pos.y())};
        if (lock) {
            const QPoint delta = m_selector->globalPixel(live) - pos;
            lines << QStringLiteral("\u0394 %1, %2   %3 px")
                         .arg(delta.x())
                         .arg(delta.y())
                         .arg(std::hypot(delta.x(), delta.y()), 0, 'f', 1);
        }
        QStringList hints;
        if (!m_selector->message().isEmpty()) {
            hints << m_selector->message();
        } else {
            hints << (lock ? RegionSelector::tr("Click: lock here  ·  Right-click: unlock")
                           : RegionSelector::tr("Click: lock  ·  Arrows: 1 px"))
                  << RegionSelector::tr("Ctrl+C: position  ·  Ctrl+Shift+C: color  ·  Esc: exit");
        }
        drawPanel(p, targetShot->image, target.pixel, m_selector->preciseCursor() - QPointF(m_origin), lines, hints);
    }

    RegionSelector *m_selector;
    QImage m_image;
    QPoint m_origin;
    QString m_name;
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
    m_preciseCursor = m_cursor;
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
    if (m_mode == Mode::Color) {
        pickColor();
        return;
    }
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

void RegionSelector::setPreciseCursor(const QPointF &pos)
{
    if (pos == m_preciseCursor)
        return;
    m_preciseCursor = pos;
    if (!m_lock)
        m_nudge = {}; // the mouse takes over from the arrow keys
    m_message.clear();
    updateOverlays();
}

void RegionSelector::nudgeBy(int dx, int dy)
{
    m_message.clear();
    if (m_lock) {
        // A locked point moves itself, within its monitor.
        if (const ScreenImage *shot = screenImage(m_lock->screen)) {
            m_lock->pixel = QPoint(std::clamp(m_lock->pixel.x() + dx, 0, shot->image.width() - 1),
                                   std::clamp(m_lock->pixel.y() + dy, 0, shot->image.height() - 1));
        }
    } else if (Platform::moveCursorBy(dx, dy)) {
        // The pointer itself moves, and with it everything that follows it;
        // its mouse move brings the new position.
        return;
    } else {
        m_nudge += QPoint(dx, dy);
    }
    updateOverlays();
}

RegionSelector::Sample RegionSelector::sampleAtCursor() const
{
    for (const ScreenImage &shot : std::as_const(m_snapshot.screens)) {
        if (QRectF(shot.geometry).contains(m_preciseCursor))
            return {shot.name, samplePixel(shot.image, shot.geometry.topLeft(), m_preciseCursor,
                                           m_lock ? QPoint() : m_nudge)};
    }
    return {};
}

const ScreenImage *RegionSelector::screenImage(const QString &name) const
{
    for (const ScreenImage &shot : std::as_const(m_snapshot.screens)) {
        if (shot.name == name)
            return &shot;
    }
    return nullptr;
}

QPoint RegionSelector::globalPixel(const Sample &sample) const
{
    const ScreenImage *shot = screenImage(sample.screen);
    if (!shot)
        return {};
    const qreal dpr = shot->image.devicePixelRatio();
    return QPoint(qRound(shot->geometry.x() * dpr), qRound(shot->geometry.y() * dpr)) + sample.pixel;
}

void RegionSelector::pickColor()
{
    const Sample sample = sampleAtCursor();
    const ScreenImage *shot = screenImage(sample.screen);
    if (!shot)
        return;
    const QColor color = shot->image.pixelColor(sample.pixel);
    m_pressed = false;
    closeOverlays();
    emit colorPicked(color);
}

void RegionSelector::lockAtCursor()
{
    const Sample sample = sampleAtCursor();
    if (sample.screen.isEmpty())
        return;
    m_lock = sample;
    m_nudge = {};
    m_message.clear();
    updateOverlays();
}

void RegionSelector::unlock()
{
    m_lock.reset();
    m_message.clear();
    updateOverlays();
}

void RegionSelector::copyPosition()
{
    const Sample sample = m_lock ? *m_lock : sampleAtCursor();
    if (sample.screen.isEmpty())
        return;
    const QPoint pos = globalPixel(sample);
    const QString text = QStringLiteral("%1, %2").arg(pos.x()).arg(pos.y());
    QGuiApplication::clipboard()->setText(text);
    m_message = tr("Copied %1").arg(text);
    updateOverlays();
}

void RegionSelector::copyColor()
{
    const Sample sample = m_lock ? *m_lock : sampleAtCursor();
    const ScreenImage *shot = screenImage(sample.screen);
    if (!shot)
        return;
    const QString text = shot->image.pixelColor(sample.pixel).name(QColor::HexRgb).toUpper();
    QGuiApplication::clipboard()->setText(text);
    m_message = tr("Copied %1").arg(text);
    updateOverlays();
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
