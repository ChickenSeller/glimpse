#pragma once

#include <QTimer>
#include <QWidget>

// The "delay before capture" countdown: a small seconds counter in the
// bottom-right corner of the screen under the pointer. Clicking it or pressing
// Esc cancels. It hides itself before finished() so it is never captured.
class DelayCountdown : public QWidget
{
    Q_OBJECT

public:
    explicit DelayCountdown(int seconds, QWidget *parent = nullptr);

    void start();

signals:
    void finished();
    void canceled();

protected:
    void paintEvent(QPaintEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;

private:
    void tick();
    void cancel();

    int m_remaining;
    QTimer m_timer;
};
