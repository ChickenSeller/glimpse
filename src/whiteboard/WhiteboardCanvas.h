#pragma once

#include <QImage>
#include <QList>
#include <QPixmap>
#include <QPolygonF>
#include <QTimer>
#include <QWidget>

#include <optional>

class WhiteboardToolbar;

// Draw on the screen, like ZoomIt's or Epic Pen's pens: a full-screen layer
// on top of everything with its own toolbar. Over the live screen, which goes
// on changing underneath (videos play, pages update), or over a frozen
// picture of it, or a whiteboard. Pass-through lets the mouse reach
// the programs below while the drawing stays on screen.
class WhiteboardCanvas : public QWidget
{
    Q_OBJECT

public:
    enum class Tool { Pen, Highlighter, Line, Arrow, Rectangle, Ellipse, Eraser };
    enum class Background { Live, Frozen, White };

    // Covers `screen`, over the live screen.
    explicit WhiteboardCanvas(QScreen *screen);
    ~WhiteboardCanvas() override;

    Tool tool() const { return m_tool; }
    void setTool(Tool tool);
    QColor color() const { return m_color; }
    void setColor(const QColor &color);
    qreal penWidth() const { return m_penWidth; }
    void setPenWidth(qreal width);
    Background background() const { return m_background; }
    void setBackground(Background background);
    // The mouse goes to the programs below; the drawing stays.
    bool passThrough() const { return m_passThrough; }
    void setPassThrough(bool on);

    bool canUndo() const { return !m_undo.isEmpty(); }
    bool canRedo() const { return !m_redo.isEmpty(); }
    bool isEmpty() const { return m_strokes.isEmpty(); }
    void undo();
    void redo();
    void clear();
    // The background (the live screen as it is now) with the drawing on it.
    void copy();
    void save();

    static const QList<QColor> &colors();
    static const QList<qreal> &penWidths();

signals:
    // Tool, color, width, background, pass-through or history changed.
    void stateChanged();

protected:
    void paintEvent(QPaintEvent *event) override;
    void showEvent(QShowEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    void keyPressEvent(QKeyEvent *event) override;
    void leaveEvent(QEvent *event) override;
    void changeEvent(QEvent *event) override;

private:
    struct Stroke {
        Tool tool = Tool::Pen;
        QColor color;
        qreal width = 0;
        QPolygonF points; // the pen's path, or a shape's start and end
    };

    void begin(const QPointF &pos);
    QPointF constrained(const QPointF &pos, Qt::KeyboardModifiers modifiers) const;
    void commit(const QList<Stroke> &before);
    void redrawInk();
    void paintStroke(QPainter &p, const Stroke &stroke) const;
    QRectF strokeBounds(const Stroke &stroke) const;
    qreal eraserRadius() const;
    void eraseAlong(const QPointF &from, const QPointF &to);
    void updateCursor();
    QPixmap grabBeneath();
    QImage render();
    void showHint(const QString &text);

    WhiteboardToolbar *m_toolbar = nullptr;
    Tool m_tool = Tool::Pen;
    QColor m_color;
    qreal m_penWidth;
    Background m_background = Background::Live;
    bool m_passThrough = false;

    QList<Stroke> m_strokes;
    QList<QList<Stroke>> m_undo;
    QList<QList<Stroke>> m_redo;
    std::optional<Stroke> m_current;  // being drawn
    std::optional<QList<Stroke>> m_erasing; // the strokes before this erase
    QPointF m_lastPos;
    std::optional<QPointF> m_hover;   // the eraser's place, while it is chosen

    QImage m_ink;     // the finished strokes, in screen pixels
    QPixmap m_frozen; // the Frozen background
    bool m_inkHidden = false; // while grabbing the screen below
    QString m_hint;
    QTimer m_hintTimer;
};
