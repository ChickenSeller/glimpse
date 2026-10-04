#pragma once

#include <QHash>
#include <QRect>

class QPainter;

// An on-video keyboard (87 keys: function row, main block, editing block and
// arrows) whose held keys light up and fade out shortly after release. Keys
// are identified by Windows virtual-key codes, which InputMonitor reports
// (left/right Shift, Ctrl and Alt distinguished).
class KeyboardOverlay
{
public:
    void setKey(int virtualKey, bool pressed, qint64 nowMs);
    // The size the keyboard takes for a key unit of `unit` pixels.
    static QSize sizeFor(qreal unit);
    // Draws the keyboard into `area` (it keeps its aspect ratio, centered at
    // the bottom of `area`).
    void paint(QPainter &painter, const QRectF &area, qint64 nowMs) const;

private:
    struct State {
        bool pressed = false;
        qint64 releasedAt = -1;
    };
    QHash<int, State> m_keys;
};
