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
// Color mode: a magnifier follows the cursor; arrow keys move by one native
// pixel; a click (or Enter) emits colorPicked() instead of selected().
// Crosshair mode: full-screen crosshair with position and color. A click locks
// it (arrow keys then move the lock) and distances are shown from there;
// Ctrl+C copies the position, Ctrl+Shift+C the color. Only Esc ends it.
class RegionSelector : public QObject
{
    Q_OBJECT

public:
    enum class Mode { Region, Window, Freehand, Color, Crosshair };

    // A native pixel on one monitor.
    struct Sample {
        QString screen; // ScreenImage::name; empty if none
        QPoint pixel;   // in that monitor's image
    };

    RegionSelector(const DesktopSnapshot &snapshot, Mode mode, QObject *parent = nullptr);
    ~RegionSelector() override;

    const DesktopSnapshot &snapshot() const { return m_snapshot; }

    void start();

    // The drawn outline after a Freehand selection; empty in the other modes.
    QPainterPath shape() const { return m_shape; }

signals:
    void selected(const QRect &logicalRect);
    void canceled();
    void colorPicked(const QColor &color);

private:
    friend class detail::SelectionOverlay;

    QRect selection() const;
    bool isDragging() const { return m_dragging; }
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

    QList<CaptureTarget> m_hoverChain; // outermost window first
    qsizetype m_hoverLevel = -1;       // index into m_hoverChain
};
