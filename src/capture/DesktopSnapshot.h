#pragma once

#include <QImage>
#include <QList>
#include <QPainterPath>
#include <QRect>
#include <QString>

#include <vector>

// One monitor's frozen contents.
struct ScreenImage {
    QString name;   // QScreen::name(), used to find the screen again
    QRect geometry; // logical coordinates on the virtual desktop
    QImage image;   // native pixels; devicePixelRatio() = pixels per logical unit
};

// A window or child control that can be picked as a capture target.
struct WindowNode {
    QRect geometry; // logical coordinates on the virtual desktop
    QString title;
    std::vector<WindowNode> children; // topmost first
};

struct CaptureTarget {
    QRect geometry;
    QString title;
};

// A frozen copy of the whole desktop, taken before any selection UI is shown.
class DesktopSnapshot
{
public:
    QList<ScreenImage> screens;
    std::vector<WindowNode> windows; // top-level windows, topmost first; may be empty

    bool isEmpty() const { return screens.isEmpty(); }
    QRect virtualGeometry() const;

    // Returns the area under a logical rectangle at native resolution. A rect
    // spanning monitors with different scale factors is rendered at the highest one.
    QImage crop(const QRect &logicalRect) const;
    // The area inside a logical outline, cropped to its bounding box; pixels
    // outside the outline are `outside` (transparent by default).
    QImage crop(const QPainterPath &logicalShape, const QColor &outside = Qt::transparent) const;

    // The windows/controls under `pos`, from the top-level window inward.
    QList<CaptureTarget> targetsAt(const QPoint &pos) const;
};
