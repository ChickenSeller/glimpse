#pragma once

#include "CaptureMode.h"

#include <QHash>
#include <QObject>
#include <QPointer>
#include <QRect>

#include <functional>
#include <memory>

class CaptureToolbar;
class RecordingSession;
class DesktopSnapshot;
class GlobalHotkeys;
class QAction;
class QDialog;
class QMenu;
class QSystemTrayIcon;
class ScreenGrabber;
class AboutDialog;
class Updater;
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
    // A hotkey that reached Glimpse other than through GlobalHotkeys (see
    // main.cpp): `id` as registered, a CaptureMode.
    void activateHotkey(int id);
    void showSettings();
    void showAbout();

protected:
    bool eventFilter(QObject *watched, QEvent *event) override;

private:
    void setupTray();
    void retranslate();
    // (Re)registers all configured hotkeys; returns the ones another program owns.
    QStringList registerHotkeys();
    void beginCapture(Mode mode);
    // From a menu or dialog that is still fading out as the capture starts.
    void beginCaptureAfterFade(Mode mode);
    void startCapture(Mode mode, bool afterFade);
    void onSnapshot(const DesktopSnapshot &snapshot);
    void onGrabFailed(const QString &message);
    // The screenshot portal refused for lack of permission: asks for it from a
    // focused window, then starts the capture again.
    void onPermissionNeeded();
    void requestPermission(bool resetFirst);
    void closePermissionDialog();
    void endCapture();
    void openEditor(const QImage &image);
    // A finished capture: the capture window, and the clipboard if so set.
    void showCapture(const QImage &image);
    void pinToScreen(const QImage &image, const QRect &where);
    void showQrResult(const QImage &image);
    void showOcrResult(const QImage &image);
    // Whether the translation engine from the settings can translate now,
    // with a missing model downloaded (after asking); if not, says why.
    bool prepareTranslation();
    // The OCR engine and languages to use, with missing models downloaded
    // (after asking); false if text cannot be recognized now.
    bool prepareOcr(const QString &title, QString *engine, QStringList *languages);
    void showColorResult(const QColor &color);
    void startScrollingCapture(const QRect &rect);
    void startRecording(const QRect &rect);
    // Runs the "delay before capture" countdown (if any), then `then`.
    void afterDelay(std::function<void()> then);
    void grabAfterDelay();
    // Hands a finished capture to the result window, QR or OCR dialog.
    void deliver(const QImage &image);

    ScreenGrabber *m_grabber = nullptr;
    GlobalHotkeys *m_hotkeys = nullptr;
    std::unique_ptr<CaptureToolbar> m_toolbar;
    std::unique_ptr<QMenu> m_trayMenu;
    QSystemTrayIcon *m_tray = nullptr;
    Updater *m_updater = nullptr;
    QHash<CaptureMode, bool> m_hotkeyRegistered; // per configured hotkey: registered, or taken
    QHash<Mode, QAction *> m_trayCaptureActions;
    QAction *m_trayShowToolbar = nullptr;
    QAction *m_traySettings = nullptr;
    QAction *m_trayAbout = nullptr;
    QAction *m_trayExit = nullptr;
    QPointer<SettingsDialog> m_settings;
    QPointer<AboutDialog> m_about;
    QPointer<QDialog> m_permissionDialog; // while asking for screenshot permission

    Mode m_mode = Mode::FullScreen;
    bool m_busy = false;
    bool m_grabbedOnce = false; // a grab has gone through, so permission is settled
    bool m_probing = false; // a grab only to settle permission before a delay
    QPointer<RecordingSession> m_recording; // while a recording runs
    QRect m_pinRect; // where the region being pinned was chosen
    bool m_restoreToolbar = false;
};
