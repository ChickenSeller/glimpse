#pragma once

#include "ScrollStitcher.h"

#include <QImage>
#include <QObject>
#include <QPointer>
#include <QPoint>
#include <QRect>

#include <memory>

class GlobalHotkeys;
class QLabel;
class QPushButton;
class QScreen;
class QWidget;

// Scrolling capture of one screen area, FastStone style: the pointer is put
// over the area and the wheel is turned step by step; after each step the area
// is grabbed and stitched. Stops at the end of the content, on Esc or Stop.
// It begins once the user clicks in the area: where, is where the wheel turns
// (a window may hold several scrolling panes). The click is caught, so it
// does not reach the application below.
// The area is marked by a frame drawn just outside it, with a small panel
// showing progress.
//
// Manual scrolling (an area picked with Ctrl held, as in FastStone) leaves the
// scrolling to the user, by wheel, keyboard or scroll bar; the area is grabbed
// every so often and whatever scrolled into view is stitched on. Stops on
// Esc or Stop. Turning the wheel during automatic scrolling switches to
// manual scrolling from there on.
//
// Beside the area, where there is room, a preview shows the stitched image so
// far. Ctrl+Z or Undo takes back the last rows stitched on (and switches to
// manual scrolling: scrolling back up a little picks up from there).
class ScrollCaptureSession : public QObject
{
    Q_OBJECT

public:
    enum class Scrolling { Automatic, Manual };

    ScrollCaptureSession(const QRect &logicalRect, Scrolling scrolling, QObject *parent = nullptr);
    ~ScrollCaptureSession() override;

    // How opaque the preview beside the area is (0 to 1); 0 shows none.
    // Before start().
    void setPreviewOpacity(qreal opacity) { m_previewOpacity = opacity; }
    void start();
    // As if the user clicked at `point` (global, logical) to begin automatic
    // scrolling; for tests, which cannot click.
    void clickAt(const QPoint &point);
    // Ends the capture with what it has, as Stop does.
    void stop() { m_stopRequested = true; }
    // Ends it without a result.
    void abort();
    // After elevationNeeded(): goes on with the user scrolling.
    void continueManually();

protected:
    bool eventFilter(QObject *watched, QEvent *event) override;

signals:
    // The stitched image; null if nothing could be captured.
    void finished(const QImage &image);
    // Automatic scrolling found the area's window running as administrator,
    // out of reach of the wheel input; the session waits for continueManually()
    // or abort(), with Esc and Ctrl+Z released for the question.
    void elevationNeeded();

private:
    void showClickHint();
    // After the click that picks where to scroll.
    void beginScrolling(const QPoint &scrollPoint);
    void step();
    void manualStep();
    void finish();
    void createChrome();
    void placePreview(const QRect &panel);
    void updatePanel();
    void updatePreview();
    // From automatic to manual scrolling: the user scrolls from here on.
    void takeOver(int firstGrabDelayMs);
    // Esc and Ctrl+Z, held while capturing so the scrolled program does not
    // get them.
    void holdKeys(bool hold);
    // Over a window running as administrator, Windows lets Glimpse see no
    // keys at all (not even its hotkeys) while that window has the focus: the
    // panel takes the focus instead, so that Esc and Ctrl+Z reach it.
    void focusPanel();
    void undo();

    Scrolling m_scrolling;
    QScreen *m_screen = nullptr;
    QRect m_rect;         // logical, global, within m_screen
    QPoint m_scrollPoint; // where the pointer sits while scrolling
    QPoint m_restoreCursor;
    ScrollStitcher m_stitcher;

    std::vector<std::unique_ptr<QWidget>> m_frame; // four border strips
    std::unique_ptr<QWidget> m_panel;
    QLabel *m_status = nullptr;
    QPushButton *m_undo = nullptr;
    std::unique_ptr<QWidget> m_preview;
    QPointer<QWidget> m_clickCatcher; // over the area until the click
    std::unique_ptr<QWidget> m_clickHint; // says to click, until then
    GlobalHotkeys *m_hotkeys = nullptr;
    bool m_panelOverlaps = false; // no room outside: hidden while grabbing

    int m_notches = 2;
    int m_steps = 0;
    int m_unchanged = 0;
    bool m_retrying = false;
    bool m_lost = false; // Manual: scrolled too far to line up
    bool m_stopRequested = false;
    bool m_done = false;
    bool m_discard = false;
    bool m_reachChecked = false; // Manual: looked whether the area is out of reach
    qreal m_previewOpacity = 0.7;
};
