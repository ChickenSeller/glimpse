#pragma once

#include "capture/DesktopSnapshot.h"

#include <QColor>
#include <QList>
#include <QObject>
#include <QPainterPath>
#include <QPoint>
#include <QPolygon>
#include <QRect>

#include <optional>

namespace detail {
class SelectionOverlay;
}

// Shows the frozen desktop on every monitor and lets the user pick an area.
// All coordinates are logical virtual-desktop coordinates; each overlay window
// only translates its local events, so a drag may cross monitors.
//
// Region mode: drag out a rectangle.
// Window mode: hover highlights the window/control under the cursor (wheel
// walks out to parents and back in), click takes it; dragging still works.
// Freehand mode: drag to draw an outline; releasing closes it. selected()
// then carries its bounding box and shape() the outline itself.
// Adjustable (Region and Window modes): a rectangle or window, once picked,
// stays selected with handles; drag them or the inside, or use the arrow keys
// (Shift: 10 px, Ctrl: resize), then Enter or a double-click takes it and
// Ctrl+C takes it to be copied. A press outside starts a new one.
// While dragging out a rectangle, holding Space moves it instead.
// Crosshair mode: full-screen crosshair with position and color. A click (or
// Space) locks it (arrow keys then move the lock) and distances are shown from
// there; Ctrl+C copies the position, Ctrl+Shift+C the color. A double-click
// (or Enter) emits colorPicked() for the pixel under it; otherwise only Esc
// ends it.
class RegionSelector : public QObject
{
    Q_OBJECT

public:
    enum class Mode { Region, Window, Freehand, Crosshair };

    // A native pixel on one monitor.
    struct Sample {
        QString screen; // ScreenImage::name; empty if none
        QPoint pixel;   // in that monitor's image
    };

    RegionSelector(const DesktopSnapshot &snapshot, Mode mode, QObject *parent = nullptr);
    ~RegionSelector() override;

    const DesktopSnapshot &snapshot() const { return m_snapshot; }

    void start();
    void setAdjustable(bool on) { m_adjustable = on; }

    // The drawn outline after a Freehand selection; empty in the other modes.
    QPainterPath shape() const { return m_shape; }
    // Whether Ctrl was held when the selection was made.
    bool controlHeld() const { return m_controlHeld; }
    // Whether the selection was taken with Ctrl+C, to be copied and done.
    bool copyRequested() const { return m_copy; }

signals:
    void selected(const QRect &logicalRect);
    void canceled();
    void colorPicked(const QColor &color);

private:
    friend class detail::SelectionOverlay;

    // Where a press on the adjustable selection takes hold of it.
    enum class Handle { None, Move, Left, Top, Right, Bottom, TopLeft, TopRight, BottomLeft, BottomRight };

    QRect selection() const;
    bool isAdjusting() const { return m_adjusting; }
    Handle handleAt(const QPoint &pos) const;
    // The pointer's shape over `pos` (logical, global) in Region / Window mode.
    Qt::CursorShape cursorShapeAt(const QPoint &pos) const;
    void adjustBy(int dx, int dy, bool resize);
    void confirm(bool copy);
    void setSpaceHeld(bool held) { m_spaceHeld = held; }
    bool isDragging() const { return m_dragging; }
    bool isPressed() const { return m_pressed; }
    QPoint cursor() const { return m_cursor; }
    QPointF preciseCursor() const { return m_preciseCursor; }
    QPoint nudge() const { return m_nudge; }
    // The pixel under the cursor (moved by the arrow keys while not locked).
    Sample sampleAtCursor() const;
    std::optional<Sample> lockedSample() const { return m_lock; }
    const ScreenImage *screenImage(const QString &name) const;
    // Virtual-desktop position in native pixels.
    QPoint globalPixel(const Sample &sample) const;
    QString message() const { return m_message; }
    Mode mode() const { return m_mode; }
    const QPolygon &freehandPath() const { return m_path; }
    // The highlighted window/control in Window mode; empty geometry otherwise.
    CaptureTarget hoverTarget() const;

    void press(const QPoint &pos);
    void move(const QPoint &pos);
    // Color mode: the exact pointer position (fractional on scaled screens).
    void setPreciseCursor(const QPointF &pos);
    void nudgeBy(int dx, int dy);
    void pickColor();
    // Crosshair mode actions.
    void lockAtCursor();
    void unlock();
    void copyPosition();
    void copyColor();
    void release(const QPoint &pos);
    void wheel(int steps);
    void resetSelection();
    void cancel();
    void finish(const QRect &rect);
    void beginAdjusting(const QRect &rect);
    QRect boundedMove(const QRect &rect, const QPoint &delta) const;
    void updateHover();
    void closeOverlays();
    void updateOverlays();

    DesktopSnapshot m_snapshot;
    Mode m_mode;
    QList<detail::SelectionOverlay *> m_overlays;
    QPoint m_anchor;
    QPoint m_cursor;
    bool m_pressed = false;
    bool m_dragging = false;
    QPolygon m_path;      // Freehand: the outline drawn so far
    QPointF m_preciseCursor; // Color: logical, fractional
    QPoint m_nudge;          // Color: arrow-key offset in native pixels
    std::optional<Sample> m_lock; // Crosshair: the locked point
    QString m_message;            // Crosshair: feedback after copying
    QPainterPath m_shape; // Freehand: the finished outline
    bool m_controlHeld = false;
    bool m_adjustable = false;
    bool m_adjusting = false;
    QRect m_adjustRect;          // the selection being adjusted
    Handle m_grab = Handle::None; // what a press is dragging
    QPoint m_grabOrigin;
    QRect m_grabRect;
    bool m_spaceHeld = false;
    bool m_copy = false;

    QList<CaptureTarget> m_hoverChain; // outermost window first
    qsizetype m_hoverLevel = -1;       // index into m_hoverChain
};
