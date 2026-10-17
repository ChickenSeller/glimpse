#pragma once

#include <QTimer>
#include <QWidget>

#include <optional>

class QAction;

// An on-screen ruler in screen pixels: a horizontal and a vertical arm in a
// cross, see-through, on top of everything. Only the arms' measuring edges,
// which cross, show until the ruler is pointed at; then their ticks and
// outline appear too. Each arm has a line that follows the pointer along it,
// wherever on the screen the pointer is, and tells how far it is from the
// cross. Drag the ruler to move it, drag an arm's end to change its
// length; arrows nudge it, Ctrl+arrows change the arms' lengths.
// Ctrl+scroll makes it fainter; double-click or Esc closes it.
class RulerWindow : public QWidget
{
    Q_OBJECT

public:
    // Its cross in the middle of the screen, shaped like the last ruler.
    RulerWindow();

protected:
    void paintEvent(QPaintEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    void mouseDoubleClickEvent(QMouseEvent *event) override;
    void wheelEvent(QWheelEvent *event) override;
    void keyPressEvent(QKeyEvent *event) override;
    void contextMenuEvent(QContextMenuEvent *event) override;
    void closeEvent(QCloseEvent *event) override;
    void enterEvent(QEnterEvent *event) override;
    void leaveEvent(QEvent *event) override;
    void changeEvent(QEvent *event) override;

private:
    enum class End { None, Left, Right, Top, Bottom };

    // In screen (physical) pixels, from the window's top-left corner: its
    // size (the arms' lengths) and the pointer.
    struct Measure {
        QSize size;
        QPoint pointer;
    };
    Measure measure() const;
    QRect horizontalArm() const; // logical, local
    QRect verticalArm() const;
    End endAt(const QPoint &pos) const;
    // Sets the window's geometry and where the arms cross within it.
    void setLayout(const QRect &geometry, const QPoint &cross);
    void keepOnScreen();
    void pollPointer();
    void setHovered(bool hovered);
    void showHint(const QString &text);
    void retranslate();

    QPoint m_cross;         // local: where the measuring edges cross
    bool m_hovered = false; // the ticks and the outline show only then
    std::optional<QPoint> m_paintedPointer; // what the last paint showed
    End m_resizing = End::None;
    QPoint m_pressGlobal;
    QRect m_pressGeometry;
    QPoint m_pressCross;
    QString m_hint;
    QTimer m_hintTimer;
    QTimer m_pointerTimer;
    QAction *m_closeAction;
};
