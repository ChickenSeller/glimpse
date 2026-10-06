#include "DesktopSnapshot.h"

#include <QPainter>

#include <algorithm>

QRect DesktopSnapshot::virtualGeometry() const
{
    QRect result;
    for (const ScreenImage &screen : screens)
        result |= screen.geometry;
    return result;
}

QImage DesktopSnapshot::crop(const QRect &logicalRect) const
{
    const QRect area = logicalRect.intersected(virtualGeometry());
    if (area.isEmpty())
        return {};

    // Fast path: the area lies on a single monitor, copy its pixels untouched.
    for (const ScreenImage &screen : screens) {
        if (!screen.geometry.contains(area))
            continue;
        const qreal dpr = screen.image.devicePixelRatio();
        const QRect local = area.translated(-screen.geometry.topLeft());
        const QRect source = QRectF(local.x() * dpr, local.y() * dpr,
                                    local.width() * dpr, local.height() * dpr)
                                 .toAlignedRect()
                                 .intersected(screen.image.rect());
        return screen.image.copy(source);
    }

    qreal dpr = 1.0;
    for (const ScreenImage &screen : screens) {
        if (screen.geometry.intersects(area))
            dpr = std::max(dpr, screen.image.devicePixelRatio());
    }

    // Gaps between monitors of different sizes stay transparent.
    QImage result(qRound(area.width() * dpr), qRound(area.height() * dpr),
                  QImage::Format_ARGB32_Premultiplied);
    result.fill(Qt::transparent);

    QPainter painter(&result);
    painter.setRenderHint(QPainter::SmoothPixmapTransform);
    painter.scale(dpr, dpr);
    painter.translate(-area.topLeft());
    for (const ScreenImage &screen : screens) {
        if (screen.geometry.intersects(area))
            painter.drawImage(QRectF(screen.geometry), screen.image, QRectF(screen.image.rect()));
    }
    painter.end();

    result.setDevicePixelRatio(dpr);
    return result;
}

QImage DesktopSnapshot::crop(const QPainterPath &logicalShape, const QColor &outside) const
{
    const QRect bounds = logicalShape.boundingRect().toAlignedRect();
    const QImage area = crop(bounds);
    if (area.isNull())
        return {};

    QImage result(area.size(), QImage::Format_ARGB32_Premultiplied);
    result.fill(Qt::transparent);
    QPainter painter(&result);
    painter.setRenderHint(QPainter::Antialiasing);
    // Outline in image pixels; the crop may have been clipped to the desktop.
    const qreal scale = area.devicePixelRatio();
    QTransform toPixels;
    toPixels.scale(scale, scale);
    toPixels.translate(-bounds.x(), -bounds.y());
    QPainterPath outline = toPixels.map(logicalShape);
    outline.setFillRule(Qt::WindingFill);
    painter.setPen(Qt::NoPen);
    painter.setBrush(Qt::white);
    painter.drawPath(outline);
    painter.setCompositionMode(QPainter::CompositionMode_SourceIn);
    // Pixel for pixel: drawn as is, an image of a scaled screen (150%) would
    // be shrunk to its logical size.
    QImage pixels = area;
    pixels.setDevicePixelRatio(1.0);
    painter.drawImage(0, 0, pixels);
    if (outside.alpha() > 0) {
        painter.setCompositionMode(QPainter::CompositionMode_DestinationOver);
        painter.fillRect(result.rect(), outside);
    }
    painter.end();

    result.setDevicePixelRatio(area.devicePixelRatio());
    return result;
}

QList<CaptureTarget> DesktopSnapshot::targetsAt(const QPoint &pos) const
{
    QList<CaptureTarget> chain;
    const std::vector<WindowNode> *level = &windows;
    while (level) {
        const auto hit = std::find_if(level->begin(), level->end(), [&pos](const WindowNode &node) {
            return node.geometry.contains(pos);
        });
        if (hit == level->end())
            break;
        chain.append({hit->geometry, hit->title, hit->program, hit->className});
        level = &hit->children;
    }
    return chain;
}
