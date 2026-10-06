#pragma once

#include "ScreenRecorder.h"

#include <QObject>
#include <QRect>

#include <memory>
#include <vector>

class QScreen;
class QTimer;
class QWidget;

namespace Ui {
class RecordingPanel;
}

// A recording in progress: the area is marked by a red frame just outside
// it, and a small panel (elapsed time, Pause, Stop) sits next to it. Both are
// kept out of the video. Layout of the panel lives in RecordingPanel.ui.
class RecordingSession : public QObject
{
    Q_OBJECT

public:
    RecordingSession(const QRect &logicalRect, const ScreenRecorder::Options &options, QObject *parent = nullptr);
    ~RecordingSession() override;

    void start();
    void stop();

signals:
    void finished(const QString &filePath, qint64 durationMs);
    void failed(const QString &message);

protected:
    bool eventFilter(QObject *watched, QEvent *event) override;

private:
    void createChrome();
    void placePanel();
    void updatePanel();
    void togglePause();

    QScreen *m_screen = nullptr;
    QRect m_rect; // logical, global, within m_screen
    std::unique_ptr<ScreenRecorder> m_recorder;
    std::vector<std::unique_ptr<QWidget>> m_frame;
    std::unique_ptr<QWidget> m_panel;
    std::unique_ptr<Ui::RecordingPanel> m_panelUi;
    QTimer *m_clockTimer = nullptr;
    bool m_stopping = false;
};
