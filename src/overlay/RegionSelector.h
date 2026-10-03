#pragma once

#include "capture/DesktopSnapshot.h"

#include <QList>
#include <QObject>
#include <QPainterPath>
#include <QPoint>
#include <QPolygon>
#include <QRect>

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
class RegionSelector : public QObject
{
    Q_OBJECT

public:
    enum class Mode { Region, Window, Freehand };

    RegionSelector(const DesktopSnapshot &snapshot, Mode mode, QObject *parent = nullptr);
    ~RegionSelector() override;

    const DesktopSnapshot &snapshot() const { return m_snapshot; }

    void start();

    // The drawn outline after a Freehand selection; empty in the other modes.
    QPainterPath shape() const { return m_shape; }

signals:
    void selected(const QRect &logicalRect);
    void canceled();

private:
    friend class detail::SelectionOverlay;

    QRect selection() const;
    bool isDragging() const { return m_dragging; }
    QPoint cursor() const { return m_cursor; }
    Mode mode() const { return m_mode; }
    const QPolygon &freehandPath() const { return m_path; }
    // The highlighted window/control in Window mode; empty geometry otherwise.
    CaptureTarget hoverTarget() const;

    void press(const QPoint &pos);
    void move(const QPoint &pos);
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
    QPainterPath m_shape; // Freehand: the finished outline

    QList<CaptureTarget> m_hoverChain; // outermost window first
    qsizetype m_hoverLevel = -1;       // index into m_hoverChain
};
