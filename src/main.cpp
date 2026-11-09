#include "app/AppIcon.h"
#include "app/AppSettings.h"
#include "app/Autostart.h"
#include "app/CaptureController.h"
#include "app/Language.h"
#include "capture/Platform.h"
#include "app/SettingsStore.h"
#include "ocr/OcrEngine.h"
#include "translate/FirefoxTranslation.h"
#include "translate/LocalModel.h"

#include <QApplication>
#include <QFile>
#include <QFileInfo>
#include <QSystemTrayIcon>

#include <optional>

#include <kdsingleapplication.h>

namespace {

// glimpse --list-downloads <file>: every file Glimpse may download, one per
// line: official URL, path on a mirror, SHA-256 or "-" (tab-separated). For
// tools/mirror-downloads.py.
int listDownloads(const QString &path)
{
    QList<ModelFile> files = Ocr::allModels();
    files << FirefoxTranslation::allModels();
    const QList<LocalModel::Preset> presets = LocalModel::presets();
    for (const LocalModel::Preset &preset : presets)
        files << preset.file;

    QFile out(path);
    if (!out.open(QIODevice::WriteOnly | QIODevice::Text))
        return 1;
    for (const ModelFile &file : std::as_const(files)) {
        const QString line = QStringLiteral("%1\t%2%3\t%4\n")
                                 .arg(file.url.toString(QUrl::FullyEncoded), file.url.host(), file.url.path(),
                                      file.sha256.isEmpty() ? QStringLiteral("-") : QString::fromLatin1(file.sha256));
        out.write(line.toUtf8());
    }
    return 0;
}

} // namespace

int main(int argc, char *argv[])
{
#ifdef Q_OS_LINUX
    // On a Wayland session, run through XWayland unless told otherwise: Wayland
    // windows can neither place themselves nor stay on top, and the recording
    // frame, its panel and the docked toolbar need both. Screenshots and
    // recordings still go through the portal (see Platform::displayServer()).
    if (!qEnvironmentVariableIsSet("QT_QPA_PLATFORM")
        && (qEnvironmentVariable("XDG_SESSION_TYPE") == QLatin1String("wayland")
            || qEnvironmentVariableIsSet("WAYLAND_DISPLAY")))
        qputenv("QT_QPA_PLATFORM", "xcb;wayland");
#endif
    QApplication app(argc, argv);
    QApplication::setApplicationName(QStringLiteral("Glimpse"));
    QApplication::setOrganizationName(QStringLiteral("Glimpse"));
    QApplication::setApplicationVersion(QStringLiteral(GLIMPSE_VERSION));
    // After the application/organization names, which name the file, and
    // before anything reads a setting.
    SettingsStore::useProgramFolder();
    QApplication::setWindowIcon(appIcon());

    const QStringList args = QApplication::arguments();
    if (const qsizetype i = args.indexOf(QStringLiteral("--list-downloads")); i >= 0 && i + 1 < args.size())
        return listDownloads(args.at(i + 1));

    // glimpse --hotkey <id>: what a hotkey runs where the desktop owns the
    // keys (GNOME custom shortcuts, see GlobalHotkeys_gnome.cpp).
    std::optional<int> hotkey;
    if (const qsizetype i = args.indexOf(QStringLiteral("--hotkey")); i >= 0 && i + 1 < args.size())
        hotkey = args.at(i + 1).toInt();

    // glimpse --replace <pid>: started by that Glimpse to take over from it
    // (restarted as administrator); it quits right after starting this one.
    if (const qsizetype i = args.indexOf(QStringLiteral("--replace")); i >= 0 && i + 1 < args.size())
        Platform::waitForProcess(args.at(i + 1).toLongLong(), 15000);
    // glimpse --capture <id>: starts that capture mode (CaptureMode) right away.
    std::optional<int> capture;
    if (const qsizetype i = args.indexOf(QStringLiteral("--capture")); i >= 0 && i + 1 < args.size())
        capture = args.at(i + 1).toInt();

    // glimpse <image file>...: opens the files in the editor (as "Open
    // with" does). Anything not an option or an option's value is taken for
    // a file.
    QStringList files;
    for (qsizetype i = 1; i < args.size(); ++i) {
        const QString &arg = args.at(i);
        if (arg.startsWith(QLatin1String("--"))) {
            if (arg == QLatin1String("--hotkey") || arg == QLatin1String("--replace")
                || arg == QLatin1String("--capture") || arg == QLatin1String("--list-downloads"))
                ++i; // and its value
            continue;
        }
        if (QFileInfo(arg).isFile())
            files << QFileInfo(arg).absoluteFilePath();
    }

    // One Glimpse per user session: a second one could not register the
    // hotkeys and would add a second tray icon. Starting it again brings the
    // running one's toolbar up instead, or passes its hotkey or files on.
    KDSingleApplication instance;
    if (!instance.isPrimaryInstance()) {
        if (!files.isEmpty())
            instance.sendMessage(QByteArrayLiteral("open ") + files.join(QLatin1Char('\n')).toUtf8());
        else
            instance.sendMessage(hotkey ? QByteArrayLiteral("hotkey ") + QByteArray::number(*hotkey) : QByteArrayLiteral("show"));
        return 0;
    }

    // The app lives in the tray / floating toolbar; closing an editor must not quit it.
    QApplication::setQuitOnLastWindowClosed(false);
    // Qt's own slide-down for combo box lists, on top of the window system's
    // animation of the popup, makes a list drop down and then jump.
    QApplication::setEffectEnabled(Qt::UI_AnimateCombo, false);
#ifdef Q_OS_LINUX
    QGuiApplication::setDesktopFileName(QStringLiteral("glimpse"));
#endif
    // After the application/organization names, which locate the settings.
    Language::apply(AppSettings::language());

    CaptureController controller;
    Autostart::refresh();
    // Started with the session, by a hotkey or to open files, Glimpse waits
    // in the tray (if there is one).
    if ((!args.contains(Autostart::kArgument) && !hotkey && !capture && files.isEmpty())
        || !QSystemTrayIcon::isSystemTrayAvailable())
        controller.showToolbar();
    if (!files.isEmpty())
        controller.openImages(files);
    if (hotkey)
        controller.activateHotkey(*hotkey);
    if (capture)
        controller.captureNow(*capture);
    QObject::connect(&instance, &KDSingleApplication::messageReceived, &controller, [&controller](const QByteArray &message) {
        if (message.startsWith("hotkey "))
            controller.activateHotkey(message.mid(7).toInt());
        else if (message.startsWith("open "))
            controller.openImages(QString::fromUtf8(message.mid(5)).split(QLatin1Char('\n'), Qt::SkipEmptyParts));
        else
            controller.showToolbar();
    });

    return QApplication::exec();
}
