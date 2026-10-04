#pragma once

#include "KeyboardOverlay.h"

#include <QElapsedTimer>
#include <QImage>
#include <QList>
#include <QMediaCaptureSession>
#include <QObject>
#include <QPoint>
#include <QPointer>
#include <QRect>
#include <QString>

class InputMonitor;
class QMediaRecorder;
class QScreen;
class QScreenCapture;
class QTimer;
class QVideoFrame;
class QVideoFrameInput;
class QVideoSink;

// Records one screen area to an MP4 (H.264) file. Frames come from
// QScreenCapture, are cropped to the area, get the pointer highlight, click
// ripples and key labels painted in (so they exist only in the video), and
// go to QMediaRecorder through QVideoFrameInput.
class ScreenRecorder : public QObject
{
    Q_OBJECT

public:
    // How pressed keys are shown: text labels ("Ctrl + S"), a full keyboard
    // with the held keys lit, or both.
    enum class KeyStyle { Labels, Keyboard, Both };

    struct Options {
        QString filePath;
        int frameRate = 30;
        bool highlightCursor = true;
        bool showClicks = true;
        bool showKeys = false;
        KeyStyle keyStyle = KeyStyle::Keyboard;
    };

    ScreenRecorder(QScreen *screen, const QRect &logicalRect, const Options &options, QObject *parent = nullptr);
    ~ScreenRecorder() override;

    void start();
    // Finishes the file; finished() follows once it is written.
    void stop();
    void setPaused(bool paused);
    bool isPaused() const { return m_paused; }
    // Recorded time, pauses excluded.
    qint64 durationMs() const;
    int framesWritten() const { return m_framesWritten; }
    int framesDropped() const { return m_framesDropped; }

signals:
    void finished(const QString &filePath);
    void failed(const QString &message);

private:
    struct Click {
        QPoint pos; // physical, relative to the screen
        Qt::MouseButton button;
        qint64 at;  // ms on m_clock
    };
    struct Key {
        QString text;
        int count;
        qint64 at;
    };

    void onFrame(const QVideoFrame &frame);
    // Writes one video frame: the latest screen content with the pointer and
    // overlays as they are now. Runs at the frame rate, also while the screen
    // content does not change (the capture only delivers changed frames).
    void writeFrame();
    void paintOverlays(QImage &image, const QPoint &cropOrigin, qreal scale);
    QPoint cursorPos(qreal scale) const; // physical, relative to the screen
    void fail(const QString &message);

    QPointer<QScreen> m_screen;
    QRect m_rect; // logical, global
    Options m_options;

    QMediaCaptureSession m_source;
    QMediaCaptureSession m_output;
    QScreenCapture *m_capture = nullptr;
    QVideoSink *m_sink = nullptr;
    QTimer *m_frameTimer = nullptr;
    QImage m_area;          // latest screen content of the area, physical
    QPoint m_areaOrigin;    // its top-left, physical, relative to the screen
    qreal m_scale = 1.0;    // physical pixels per logical one
    QVideoFrameInput *m_input = nullptr;
    QMediaRecorder *m_recorder = nullptr;
    InputMonitor *m_inputMonitor = nullptr;

    QElapsedTimer m_clock;
    qint64 m_pausedTotal = 0; // ms
    qint64 m_pausedSince = -1;
    bool m_paused = false;
    bool m_stopping = false;
    bool m_failed = false;
    int m_framesWritten = 0;
    int m_framesDropped = 0;

    QList<Click> m_clicks;
    Qt::MouseButtons m_buttonsDown;
    QList<Key> m_keys;
    KeyboardOverlay m_keyboard;
};
