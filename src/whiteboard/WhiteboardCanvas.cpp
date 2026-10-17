#include "WhiteboardCanvas.h"

#include "WhiteboardToolbar.h"
#include "capture/Platform.h"
#include "editor/ImageActions.h"

#include <QKeyEvent>
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <QPainterPathStroker>
#include <QScreen>
#include <QWindow>

#include <algorithm>
#include <cmath>

namespace {

const QColor kWhiteBoard(0xfa, 0xfa, 0xf7);
constexpr int kHighlighterAlpha = 90;
constexpr int kHintMs = 1500;
constexpr int kMaxHistory = 200;

// A highlighter is a broad, flat-tipped pen.
qreal highlighterWidth(qreal width)
{
    return std::max(12.0, width * 4);
}

qreal arrowHead(qreal width)
{
    return std::max(14.0, width * 4);
}

bool isShape(WhiteboardCanvas::Tool tool)
{
    using Tool = WhiteboardCanvas::Tool;
    return tool == Tool::Line || tool == Tool::Arrow || tool == Tool::Rectangle || tool == Tool::Ellipse;
}

} // namespace

const QList<QColor> &WhiteboardCanvas::colors()
{
    static const QList<QColor> colors = {
        QColor(0xe5, 0x39, 0x35), // red
        QColor(0xfd, 0xd8, 0x35), // yellow
        QColor(0x43, 0xa0, 0x47), // green
        QColor(0x1e, 0x88, 0xe5), // blue
        QColor(0x21, 0x21, 0x21), // black
        QColor(0xff, 0xff, 0xff), // white
    };
    return colors;
}

const QList<qreal> &WhiteboardCanvas::penWidths()
{
    static const QList<qreal> widths = {2, 4, 8, 16};
    return widths;
}

WhiteboardCanvas::WhiteboardCanvas(QScreen *screen)
    : QWidget(nullptr, Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint | Qt::Tool)
    , m_color(colors().first())
    , m_penWidth(penWidths().at(1))
{
    setAttribute(Qt::WA_DeleteOnClose);
    setAttribute(Qt::WA_TranslucentBackground); // the live screen shows through
    setFocusPolicy(Qt::StrongFocus);
    setMouseTracking(true); // for the eraser's outline
    setScreen(screen);
    setGeometry(screen->geometry());
    setWindowTitle(tr("Screen Whiteboard"));

    m_hintTimer.setSingleShot(true);
    m_hintTimer.setInterval(kHintMs);
    connect(&m_hintTimer, &QTimer::timeout, this, [this] {
        m_hint.clear();
        update();
    });

    m_toolbar = new WhiteboardToolbar(this);
    updateCursor();
}

WhiteboardCanvas::~WhiteboardCanvas() = default;

void WhiteboardCanvas::showEvent(QShowEvent *event)
{
    QWidget::showEvent(event);
    if (m_toolbar->isVisible())
        return;
    // At the top, in the middle; out of captures and screen sharing, which
    // should show the drawing, not the tools.
    m_toolbar->adjustSize();
    m_toolbar->move(geometry().center().x() - m_toolbar->width() / 2, geometry().top() + 12);
    m_toolbar->show();
    Platform::excludeFromCapture(m_toolbar);
    activateWindow();
}

void WhiteboardCanvas::setTool(Tool tool)
{
    m_tool = tool;
    m_hover.reset();
    updateCursor();
    update();
    emit stateChanged();
}

void WhiteboardCanvas::setColor(const QColor &color)
{
    m_color = color;
    // Choosing a color means drawing, not erasing.
    if (m_tool == Tool::Eraser)
        setTool(Tool::Pen);
    emit stateChanged();
}

void WhiteboardCanvas::setPenWidth(qreal width)
{
    m_penWidth = width;
    update();
    emit stateChanged();
}

void WhiteboardCanvas::setBackground(Background background)
{
    if (background == Background::Frozen)
        m_frozen = grabBeneath();
    m_background = background;
    // Only the live screen has anything below worth reaching.
    if (background != Background::Live && m_passThrough)
        setPassThrough(false);
    update();
    emit stateChanged();
}

void WhiteboardCanvas::setPassThrough(bool on)
{
    if (on && m_background != Background::Live)
        return;
    m_passThrough = on;
    if (QWindow *window = windowHandle())
        window->setFlag(Qt::WindowTransparentForInput, on);
    m_current.reset();
    m_erasing.reset();
    m_hover.reset();
    if (!on)
        activateWindow();
    showHint(on ? tr("The mouse works the programs below; the drawing stays")
                : tr("Drawing"));
    emit stateChanged();
}

void WhiteboardCanvas::undo()
{
    if (m_undo.isEmpty())
        return;
    m_redo.append(m_strokes);
    m_strokes = m_undo.takeLast();
    redrawInk();
    update();
    emit stateChanged();
}

void WhiteboardCanvas::redo()
{
    if (m_redo.isEmpty())
        return;
    m_undo.append(m_strokes);
    m_strokes = m_redo.takeLast();
    redrawInk();
    update();
    emit stateChanged();
}

void WhiteboardCanvas::clear()
{
    if (m_strokes.isEmpty())
        return;
    const QList<Stroke> before = m_strokes;
    m_strokes.clear();
    commit(before);
    redrawInk();
    update();
}

// Keeps what was there before a change, for undo.
void WhiteboardCanvas::commit(const QList<Stroke> &before)
{
    m_undo.append(before);
    if (m_undo.size() > kMaxHistory)
        m_undo.removeFirst();
    m_redo.clear();
    emit stateChanged();
}

void WhiteboardCanvas::redrawInk()
{
    const qreal dpr = devicePixelRatioF();
    m_ink = QImage(QSizeF(size() * dpr).toSize(), QImage::Format_ARGB32_Premultiplied);
    m_ink.setDevicePixelRatio(dpr);
    m_ink.fill(Qt::transparent);
    QPainter p(&m_ink);
    p.setRenderHint(QPainter::Antialiasing);
    for (const Stroke &stroke : std::as_const(m_strokes))
        paintStroke(p, stroke);
}

// The pen's path smoothed through its points; a shape's outline.
static QPainterPath strokePath(const QPolygonF &points, WhiteboardCanvas::Tool tool)
{
    using Tool = WhiteboardCanvas::Tool;
    QPainterPath path;
    const QPointF a = points.first();
    const QPointF b = points.last();
    switch (tool) {
    case Tool::Pen:
    case Tool::Highlighter:
        path.moveTo(a);
        for (qsizetype i = 1; i + 1 < points.size(); ++i)
            path.quadTo(points[i], (points[i] + points[i + 1]) / 2);
        path.lineTo(b);
        break;
    case Tool::Line:
    case Tool::Arrow:
        path.moveTo(a);
        path.lineTo(b);
        break;
    case Tool::Rectangle:
        path.addRect(QRectF(a, b).normalized());
        break;
    case Tool::Ellipse:
        path.addEllipse(QRectF(a, b).normalized());
        break;
    case Tool::Eraser:
        break;
    }
    return path;
}

void WhiteboardCanvas::paintStroke(QPainter &p, const Stroke &stroke) const
{
    QColor color = stroke.color;
    qreal width = stroke.width;
    if (stroke.tool == Tool::Highlighter) {
        color.setAlpha(kHighlighterAlpha);
        width = highlighterWidth(stroke.width);
    }
    // A click without a move: a dot.
    if (stroke.points.size() == 1) {
        p.setPen(Qt::NoPen);
        p.setBrush(color);
        p.drawEllipse(stroke.points.first(), width / 2, width / 2);
        return;
    }
    // One path per stroke, so a highlighter crossing itself stays even.
    p.setPen(QPen(color, width, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
    p.setBrush(Qt::NoBrush);
    if (stroke.tool != Tool::Arrow) {
        p.drawPath(strokePath(stroke.points, stroke.tool));
        return;
    }

    const QPointF a = stroke.points.first();
    const QPointF b = stroke.points.last();
    const qreal length = QLineF(a, b).length();
    if (length < 1)
        return;
    const QPointF along = (b - a) / length;
    const QPointF across(-along.y(), along.x());
    const qreal head = std::min(arrowHead(width), length);
    const QPointF base = b - along * head;
    p.drawLine(a, base + along * std::min(width / 2, head / 2));
    p.setPen(QPen(color, 1, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
    p.setBrush(color);
    p.drawPolygon(QPolygonF({b, base + across * head * 0.5, base - across * head * 0.5}));
}

QRectF WhiteboardCanvas::strokeBounds(const Stroke &stroke) const
{
    qreal margin = stroke.tool == Tool::Highlighter ? highlighterWidth(stroke.width) : stroke.width;
    if (stroke.tool == Tool::Arrow)
        margin += arrowHead(stroke.width);
    margin += 2;
    return stroke.points.boundingRect().adjusted(-margin, -margin, margin, margin);
}

qreal WhiteboardCanvas::eraserRadius() const
{
    return std::max(8.0, m_penWidth * 2);
}

// Removes every stroke the eraser touches on its way from `from` to `to`.
void WhiteboardCanvas::eraseAlong(const QPointF &from, const QPointF &to)
{
    const qreal radius = eraserRadius();
    const int steps = std::max(1, int(QLineF(from, to).length() / (radius / 2)));
    bool changed = false;
    for (qsizetype i = m_strokes.size() - 1; i >= 0; --i) {
        const Stroke &stroke = m_strokes.at(i);
        const qreal width = stroke.tool == Tool::Highlighter ? highlighterWidth(stroke.width) : stroke.width;
        QPainterPath reach;
        if (stroke.points.size() == 1) {
            reach.addEllipse(stroke.points.first(), width / 2 + radius, width / 2 + radius);
        } else {
            QPainterPathStroker stroker;
            stroker.setWidth(width + 2 * radius);
            stroker.setCapStyle(Qt::RoundCap);
            stroker.setJoinStyle(Qt::RoundJoin);
            reach = stroker.createStroke(strokePath(stroke.points, stroke.tool));
        }
        for (int step = 0; step <= steps; ++step) {
            if (reach.contains(from + (to - from) * step / steps)) {
                update(strokeBounds(stroke).toAlignedRect());
                m_strokes.removeAt(i);
                changed = true;
                break;
            }
        }
    }
    if (changed)
        redrawInk();
}

// Shift keeps lines at multiples of 45° and makes squares and circles.
QPointF WhiteboardCanvas::constrained(const QPointF &pos, Qt::KeyboardModifiers modifiers) const
{
    if (!m_current || !(modifiers & Qt::ShiftModifier))
        return pos;
    const QPointF start = m_current->points.first();
    switch (m_current->tool) {
    case Tool::Line:
    case Tool::Arrow: {
        QLineF line(start, pos);
        line.setAngle(std::round(line.angle() / 45) * 45);
        return line.p2();
    }
    case Tool::Rectangle:
    case Tool::Ellipse: {
        const QPointF d = pos - start;
        const qreal side = std::max(std::abs(d.x()), std::abs(d.y()));
        return start + QPointF(d.x() < 0 ? -side : side, d.y() < 0 ? -side : side);
    }
    default:
        return pos;
    }
}

void WhiteboardCanvas::begin(const QPointF &pos)
{
    m_lastPos = pos;
    if (m_tool == Tool::Eraser) {
        m_erasing = m_strokes;
        eraseAlong(pos, pos);
        return;
    }
    Stroke stroke;
    stroke.tool = m_tool;
    stroke.color = m_color;
    stroke.width = m_penWidth;
    stroke.points = isShape(m_tool) ? QPolygonF({pos, pos}) : QPolygonF({pos});
    m_current = stroke;
    update(strokeBounds(stroke).toAlignedRect());
}

void WhiteboardCanvas::mousePressEvent(QMouseEvent *event)
{
    activateWindow(); // for the keys
    if (event->button() == Qt::LeftButton)
        begin(event->position());
}

void WhiteboardCanvas::mouseMoveEvent(QMouseEvent *event)
{
    const QPointF pos = event->position();
    if (m_tool == Tool::Eraser) {
        const qreal r = eraserRadius() + 2;
        if (m_hover)
            update(QRectF(*m_hover - QPointF(r, r), QSizeF(2 * r, 2 * r)).toAlignedRect());
        m_hover = pos;
        update(QRectF(pos - QPointF(r, r), QSizeF(2 * r, 2 * r)).toAlignedRect());
        if (m_erasing)
            eraseAlong(m_lastPos, pos);
    } else if (m_current) {
        const QRectF before = strokeBounds(*m_current);
        if (isShape(m_current->tool)) {
            m_current->points.last() = constrained(pos, event->modifiers());
            update(before.united(strokeBounds(*m_current)).toAlignedRect());
        } else if (QLineF(m_current->points.last(), pos).length() >= 1) {
            m_current->points.append(pos);
            // Only the new piece, with the curve's last bend before it.
            const qsizetype n = m_current->points.size();
            Stroke tail = *m_current;
            tail.points = m_current->points.mid(std::max<qsizetype>(0, n - 3));
            update(strokeBounds(tail).toAlignedRect());
        }
    }
    m_lastPos = pos;
}

void WhiteboardCanvas::mouseReleaseEvent(QMouseEvent *event)
{
    if (event->button() != Qt::LeftButton)
        return;
    if (m_erasing) {
        if (m_strokes.size() != m_erasing->size())
            commit(*m_erasing);
        m_erasing.reset();
    }
    if (!m_current)
        return;
    const Stroke stroke = *m_current;
    m_current.reset();
    // A shape needs a drag; a click alone would leave an invisible one.
    if (isShape(stroke.tool) && QLineF(stroke.points.first(), stroke.points.last()).length() < 2) {
        update(strokeBounds(stroke).toAlignedRect());
        return;
    }
    const QList<Stroke> before = m_strokes;
    m_strokes.append(stroke);
    if (m_ink.size() != QSizeF(size() * devicePixelRatioF()).toSize()) {
        redrawInk();
    } else {
        QPainter p(&m_ink);
        p.setRenderHint(QPainter::Antialiasing);
        paintStroke(p, stroke);
    }
    commit(before);
    update(strokeBounds(stroke).toAlignedRect());
}

void WhiteboardCanvas::keyPressEvent(QKeyEvent *event)
{
    if (event->matches(QKeySequence::Undo))
        undo();
    else if (event->matches(QKeySequence::Redo) || event->keyCombination() == QKeyCombination(Qt::CTRL, Qt::Key_Y))
        redo();
    else if (event->matches(QKeySequence::Copy))
        copy();
    else if (event->matches(QKeySequence::Save))
        save();
    else if (event->modifiers() & (Qt::ControlModifier | Qt::AltModifier | Qt::MetaModifier))
        QWidget::keyPressEvent(event);
    else {
        const int key = event->key();
        static const QHash<int, Tool> tools = {
            {Qt::Key_P, Tool::Pen},       {Qt::Key_H, Tool::Highlighter}, {Qt::Key_L, Tool::Line},
            {Qt::Key_A, Tool::Arrow},     {Qt::Key_R, Tool::Rectangle},   {Qt::Key_O, Tool::Ellipse},
            {Qt::Key_E, Tool::Eraser},
        };
        const qsizetype widthIndex = penWidths().indexOf(m_penWidth);
        if (key == Qt::Key_Escape)
            close();
        else if (key == Qt::Key_Delete || key == Qt::Key_Backspace)
            clear();
        else if (tools.contains(key))
            setTool(tools.value(key));
        else if (key >= Qt::Key_1 && key < Qt::Key_1 + colors().size())
            setColor(colors().at(key - Qt::Key_1));
        else if (key == Qt::Key_BracketLeft && widthIndex > 0)
            setPenWidth(penWidths().at(widthIndex - 1));
        else if (key == Qt::Key_BracketRight && widthIndex >= 0 && widthIndex + 1 < penWidths().size())
            setPenWidth(penWidths().at(widthIndex + 1));
        else
            QWidget::keyPressEvent(event);
    }
}

void WhiteboardCanvas::leaveEvent(QEvent *event)
{
    if (m_hover) {
        m_hover.reset();
        update();
    }
    QWidget::leaveEvent(event);
}

void WhiteboardCanvas::changeEvent(QEvent *event)
{
    if (event->type() == QEvent::LanguageChange)
        setWindowTitle(tr("Screen Whiteboard"));
    QWidget::changeEvent(event);
}

void WhiteboardCanvas::updateCursor()
{
    setCursor(Qt::CrossCursor);
}

void WhiteboardCanvas::paintEvent(QPaintEvent *)
{
    QPainter p(this);
    p.setCompositionMode(QPainter::CompositionMode_Source);
    switch (m_background) {
    case Background::Live:
        // Not quite clear while drawing: wholly transparent pixels would let
        // the mouse through to the programs below.
        p.fillRect(rect(), m_inkHidden || m_passThrough ? QColor(Qt::transparent) : QColor(0, 0, 0, 1));
        break;
    case Background::Frozen:
        p.drawPixmap(QPoint(0, 0), m_frozen);
        break;
    case Background::White:
        p.fillRect(rect(), kWhiteBoard);
        break;
    }
    p.setCompositionMode(QPainter::CompositionMode_SourceOver);
    if (m_inkHidden)
        return;

    if (m_ink.size() != QSizeF(size() * devicePixelRatioF()).toSize())
        redrawInk();
    p.drawImage(QPoint(0, 0), m_ink);
    p.setRenderHint(QPainter::Antialiasing);
    if (m_current)
        paintStroke(p, *m_current);
    if (m_hover && m_tool == Tool::Eraser) {
        const qreal r = eraserRadius();
        p.setBrush(Qt::NoBrush);
        p.setPen(QPen(QColor(255, 255, 255, 220), 3));
        p.drawEllipse(*m_hover, r, r);
        p.setPen(QPen(QColor(0, 0, 0, 200), 1.2));
        p.drawEllipse(*m_hover, r, r);
    }

    if (!m_hint.isEmpty()) {
        const QSizeF size = QSizeF(p.fontMetrics().size(Qt::TextSingleLine, m_hint)) + QSizeF(20, 10);
        QRectF box(QPointF(0, 0), size);
        box.moveCenter(QPointF(width() / 2.0, height() - 60));
        p.setPen(Qt::NoPen);
        p.setBrush(QColor(0, 0, 0, 190));
        p.drawRoundedRect(box, 4, 4);
        p.setPen(Qt::white);
        p.drawText(box, Qt::AlignCenter, m_hint);
    }
}

// The screen below the canvas, without the drawing.
QPixmap WhiteboardCanvas::grabBeneath()
{
    // Kept out of the capture where the platform can; elsewhere drawn without
    // the ink for the moment of the grab.
    m_inkHidden = true;
    repaint();
    Platform::flushCompositor();
    Platform::excludeFromCapture(this, true);
    QPixmap shot = screen()->grabWindow(0);
    Platform::excludeFromCapture(this, false);
    m_inkHidden = false;
    update();
    shot.setDevicePixelRatio(shot.width() / qreal(width()));
    return shot;
}

QImage WhiteboardCanvas::render()
{
    QImage image;
    switch (m_background) {
    case Background::Live: image = grabBeneath().toImage(); break;
    case Background::Frozen: image = m_frozen.toImage(); break;
    case Background::White:
        image = QImage(m_ink.size(), QImage::Format_RGB32);
        image.fill(kWhiteBoard);
        break;
    }
    image = image.convertToFormat(QImage::Format_RGB32);
    image.setDevicePixelRatio(1.0);
    QPainter p(&image);
    QImage ink = m_ink;
    ink.setDevicePixelRatio(1.0);
    p.drawImage(image.rect(), ink);
    p.end();
    image.setDevicePixelRatio(devicePixelRatioF());
    return image;
}

void WhiteboardCanvas::copy()
{
    ImageActions::copyToClipboard(render());
    showHint(tr("Copied"));
}

void WhiteboardCanvas::save()
{
    if (!ImageActions::saveAs(this, render()).isEmpty())
        showHint(tr("Saved"));
}

void WhiteboardCanvas::showHint(const QString &text)
{
    m_hint = text;
    m_hintTimer.start();
    update();
}
