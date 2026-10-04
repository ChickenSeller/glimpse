#pragma once

#include <QImage>
#include <QTimer>
#include <QWidget>

class QAction;

// A capture pinned to the screen: a borderless window that stays on top of
// everything, like FastStone Capture's and Snipaste's pins. Drag it to move
// it, scroll to zoom, Ctrl+scroll to make it see-through; double-click or Esc
// closes it, right-click offers copy, save and the editor.
class PinWindow : public QWidget
{
    Q_OBJECT

public:
    // Placed over `logicalRect` (where the image was captured), or centered
    // on the screen under the pointer when that is empty.
    explicit PinWindow(const QImage &image, const QRect &logicalRect = {});

protected:
    void paintEvent(QPaintEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseDoubleClickEvent(QMouseEvent *event) override;
    void wheelEvent(QWheelEvent *event) override;
    void contextMenuEvent(QContextMenuEvent *event) override;
    void enterEvent(QEnterEvent *event) override;
    void leaveEvent(QEvent *event) override;
    void changeEvent(QEvent *event) override;

private:
    QSize logicalSize(qreal zoom) const;
    QRect closeButtonRect() const;
    void setCloseHovered(bool hovered);
    void setHovered(bool hovered);
    void pollHover();
    void zoomAt(qreal zoom, const QPointF &anchor);
    void showHint(const QString &text);
    void retranslate();
    void copy();
    void saveAs();
    void openInEditor();

    QImage m_image;
    qreal m_zoom = 1.0;
    bool m_hovered = false;
    bool m_closeHovered = false;
    QString m_hint;
    QTimer m_hintTimer;
    QTimer m_hoverTimer;
    QAction *m_copyAction;
    QAction *m_saveAction;
    QAction *m_editAction;
    QAction *m_actualSizeAction;
    QAction *m_closeAction;
};
