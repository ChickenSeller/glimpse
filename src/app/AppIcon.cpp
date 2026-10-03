#include "AppIcon.h"

#include <QPainter>
#include <QPainterPath>
#include <QPixmap>

#include <algorithm>

namespace {

QPixmap paintIcon(int size)
{
    QPixmap pm(size, size);
    pm.fill(Qt::transparent);

    QPainter p(&pm);
    p.setRenderHint(QPainter::Antialiasing);

    const qreal margin = size * 0.06;
    const QRectF body(margin, margin, size - 2 * margin, size - 2 * margin);
    p.setPen(Qt::NoPen);
    p.setBrush(QColor(0x2d, 0x7d, 0xd2));
    p.drawRoundedRect(body, size * 0.2, size * 0.2);

    // Viewfinder corners.
    QPen pen(Qt::white, std::max(1.0, size * 0.08));
    pen.setCapStyle(Qt::RoundCap);
    pen.setJoinStyle(Qt::RoundJoin);
    p.setPen(pen);
    p.setBrush(Qt::NoBrush);

    const qreal a = size * 0.27;
    const qreal b = size * 0.73;
    const qreal l = size * 0.15;
    QPainterPath corners;
    corners.moveTo(a, a + l); corners.lineTo(a, a); corners.lineTo(a + l, a);
    corners.moveTo(b - l, a); corners.lineTo(b, a); corners.lineTo(b, a + l);
    corners.moveTo(b, b - l); corners.lineTo(b, b); corners.lineTo(b - l, b);
    corners.moveTo(a + l, b); corners.lineTo(a, b); corners.lineTo(a, b - l);
    p.drawPath(corners);

    p.setPen(Qt::NoPen);
    p.setBrush(Qt::white);
    p.drawEllipse(QPointF(size / 2.0, size / 2.0), size * 0.08, size * 0.08);

    return pm;
}

} // namespace

QIcon appIcon()
{
    static const QIcon icon = [] {
        QIcon result;
        for (int size : {16, 20, 24, 32, 48, 64, 128, 256})
            result.addPixmap(paintIcon(size));
        return result;
    }();
    return icon;
}
