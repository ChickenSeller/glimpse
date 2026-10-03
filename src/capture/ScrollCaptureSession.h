#pragma once

#include "ScrollStitcher.h"

#include <QImage>
#include <QObject>
#include <QPoint>
#include <QRect>

#include <memory>

class QLabel;
class QScreen;
class QWidget;

// Scrolling capture of one screen area, FastStone style: the pointer is put
// over the area and the wheel is turned step by step; after each step the area
// is grabbed and stitched. Stops at the end of the content, on Esc or Stop.
// The area is marked by a frame drawn just outside it, with a small panel
// showing progress.
class ScrollCaptureSession : public QObject
{
    Q_OBJECT

public:
    explicit ScrollCaptureSession(const QRect &logicalRect, QObject *parent = nullptr);
    ~ScrollCaptureSession() override;

    void start();

signals:
    // The stitched image; null if nothing could be captured.
    void finished(const QImage &image);

private:
    void step();
    void finish();
    void createChrome();
    void updatePanel();

    QScreen *m_screen = nullptr;
    QRect m_rect;         // logical, global, within m_screen
    QPoint m_scrollPoint; // where the pointer sits while scrolling
    QPoint m_restoreCursor;
    ScrollStitcher m_stitcher;

    std::vector<std::unique_ptr<QWidget>> m_frame; // four border strips
    std::unique_ptr<QWidget> m_panel;
    QLabel *m_status = nullptr;
    bool m_panelOverlaps = false; // no room outside: hidden while grabbing

    int m_notches = 2;
    int m_steps = 0;
    int m_unchanged = 0;
    bool m_retrying = false;
    bool m_stopRequested = false;
    bool m_done = false;
};
