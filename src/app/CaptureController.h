#pragma once

#include "CaptureMode.h"

#include <QHash>
#include <QObject>
#include <QPointer>

#include <memory>

class CaptureToolbar;
class DesktopSnapshot;
class GlobalHotkeys;
class QAction;
class QMenu;
class QSystemTrayIcon;
class ScreenGrabber;
class SettingsDialog;

// Owns the toolbar and tray icon and runs one capture at a time:
// hide our UI -> grab the desktop -> (select) -> open the result.
class CaptureController : public QObject
{
    Q_OBJECT

public:
    using Mode = CaptureMode;

    explicit CaptureController(QObject *parent = nullptr);
    ~CaptureController() override;

    void showToolbar();
    void showSettings();

protected:
    bool eventFilter(QObject *watched, QEvent *event) override;

public slots:
    void captureFullScreen();
    void captureRegion();
    void captureWindow();

private:
    void setupTray();
    void retranslate();
    // (Re)registers all configured hotkeys; returns the ones another program owns.
    QStringList registerHotkeys();
    void beginCapture(Mode mode);
    void onSnapshot(const DesktopSnapshot &snapshot);
    void onGrabFailed(const QString &message);
    void endCapture();
    void openEditor(const QImage &image);

    ScreenGrabber *m_grabber = nullptr;
    GlobalHotkeys *m_hotkeys = nullptr;
    std::unique_ptr<CaptureToolbar> m_toolbar;
    std::unique_ptr<QMenu> m_trayMenu;
    QSystemTrayIcon *m_tray = nullptr;
    QHash<Mode, QAction *> m_trayCaptureActions;
    QAction *m_trayShowToolbar = nullptr;
    QAction *m_traySettings = nullptr;
    QAction *m_trayExit = nullptr;
    QPointer<SettingsDialog> m_settings;

    Mode m_mode = Mode::FullScreen;
    bool m_busy = false;
    bool m_restoreToolbar = false;
};
