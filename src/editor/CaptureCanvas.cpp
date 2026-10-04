#include "CaptureCanvas.h"

#include <QMouseEvent>
#include <QPainter>
#include <QStyle>

namespace {

const QColor kAccent(0x2d, 0x9c, 0xff);
// How close (logical px) the pointer must come for the frame to show.
constexpr int kReach = 4;

} // namespace

CaptureCanvas::CaptureCanvas(QWidget *parent)
    : QLabel(parent)
{
    setMouseTracking(true);
}

// Where QLabel draws the picture: its logical size, aligned in the label.
QRect CaptureCanvas::pictureRect() const
{
    const QPixmap picture = pixmap();
    if (picture.isNull())
        return {};
    return QStyle::alignedRect(layoutDirection(), alignment(), picture.deviceIndependentSize().toSize(), contentsRect());
}

void CaptureCanvas::paintEvent(QPaintEvent *event)
{
    QLabel::paintEvent(event);
    if (!m_framed)
        return;

    // One screen pixel wide, just outside the picture, so it covers none of it.
    const qreal dpr = devicePixelRatioF();
    const QRect picture = pictureRect();
    const QRectF native(QPointF(picture.topLeft()) * dpr, QSizeF(pixmap().size()));
    QPainter p(this);
    p.setWorldTransform(QTransform::fromScale(1.0 / dpr, 1.0 / dpr));
    p.setPen(QPen(kAccent, 1));
    p.setBrush(Qt::NoBrush);
    p.drawRect(native.adjusted(-0.5, -0.5, 0.5, 0.5));
}

void CaptureCanvas::mouseMoveEvent(QMouseEvent *event)
{
    setFramed(pictureRect().adjusted(-kReach, -kReach, kReach, kReach).contains(event->position().toPoint()));
    QLabel::mouseMoveEvent(event);
}

void CaptureCanvas::leaveEvent(QEvent *event)
{
    setFramed(false);
    QLabel::leaveEvent(event);
}

void CaptureCanvas::setFramed(bool framed)
{
    if (framed == m_framed)
        return;
    m_framed = framed;
    update();
}
