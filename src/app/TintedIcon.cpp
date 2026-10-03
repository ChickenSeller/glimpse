#include "TintedIcon.h"

#include <QAction>
#include <QIconEngine>
#include <QPainter>
#include <QPixmap>

namespace {

class TintedIconEngine : public QIconEngine
{
public:
    TintedIconEngine(const QIcon &source, const QColor &color)
        : m_source(source)
        , m_color(color)
    {
    }

    void paint(QPainter *painter, const QRect &rect, QIcon::Mode mode, QIcon::State state) override
    {
        const qreal scale = painter->device() ? painter->device()->devicePixelRatioF() : 1.0;
        painter->drawPixmap(rect, scaledPixmap(rect.size(), mode, state, scale));
    }

    QPixmap pixmap(const QSize &size, QIcon::Mode mode, QIcon::State state) override
    {
        return scaledPixmap(size, mode, state, 1.0);
    }

    QPixmap scaledPixmap(const QSize &size, QIcon::Mode mode, QIcon::State state, qreal scale) override
    {
        // Render the source in Normal mode; we apply our own disabled look.
        QPixmap pm = m_source.pixmap(size, scale, QIcon::Normal, state);
        if (pm.isNull())
            return pm;

        QColor color = m_color;
        if (mode == QIcon::Disabled)
            color.setAlphaF(color.alphaF() * 0.4);

        QPainter p(&pm);
        p.setCompositionMode(QPainter::CompositionMode_SourceIn);
        p.fillRect(QRectF(QPointF(0, 0), pm.deviceIndependentSize()), color);
        return pm;
    }

    QSize actualSize(const QSize &size, QIcon::Mode mode, QIcon::State state) override
    {
        return m_source.actualSize(size, mode, state);
    }

    QIconEngine *clone() const override { return new TintedIconEngine(m_source, m_color); }

private:
    QIcon m_source;
    QColor m_color;
};

} // namespace

QIcon tintedIcon(const QIcon &source, const QColor &color)
{
    if (source.isNull())
        return source;
    return QIcon(new TintedIconEngine(source, color));
}

void ActionIconTinter::add(QAction *action)
{
    m_icons.insert(action, action->icon());
}

void ActionIconTinter::apply(const QColor &color) const
{
    for (auto it = m_icons.cbegin(); it != m_icons.cend(); ++it)
        it.key()->setIcon(tintedIcon(it.value(), color));
}
