#pragma once

#include <QLabel>

// The capture window's picture. While the pointer is over the picture, a thin
// frame just outside it shows exactly what is saved, which is otherwise hard
// to tell where the capture is transparent. Promoted from QLabel in
// EditorWindow.ui.
class CaptureCanvas : public QLabel
{
    Q_OBJECT

public:
    explicit CaptureCanvas(QWidget *parent = nullptr);

protected:
    void paintEvent(QPaintEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void leaveEvent(QEvent *event) override;

private:
    QRect pictureRect() const;
    void setFramed(bool framed);

    bool m_framed = false;
};
