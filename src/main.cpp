#include "app/AppIcon.h"
#include "app/AppSettings.h"
#include "app/Autostart.h"
#include "app/CaptureController.h"
#include "app/Language.h"
#include "ocr/OcrEngine.h"
#include "translate/FirefoxTranslation.h"
#include "translate/LocalModel.h"

#include <QApplication>
#include <QFile>
#include <QSystemTrayIcon>

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
    QApplication::setWindowIcon(appIcon());

    const QStringList args = QApplication::arguments();
    if (const qsizetype i = args.indexOf(QStringLiteral("--list-downloads")); i >= 0 && i + 1 < args.size())
        return listDownloads(args.at(i + 1));

    // One Glimpse per user session: a second one could not register the
    // hotkeys and would add a second tray icon. Starting it again brings the
    // running one's toolbar up instead.
    KDSingleApplication instance;
    if (!instance.isPrimaryInstance()) {
        instance.sendMessage(QByteArrayLiteral("show"));
        return 0;
    }

    // The app lives in the tray / floating toolbar; closing an editor must not quit it.
    QApplication::setQuitOnLastWindowClosed(false);
#ifdef Q_OS_LINUX
    QGuiApplication::setDesktopFileName(QStringLiteral("glimpse"));
#endif
    // After the application/organization names, which locate the settings.
    Language::apply(AppSettings::language());

    CaptureController controller;
    Autostart::refresh();
    // Started with the session, Glimpse waits in the tray (if there is one).
    if (!args.contains(Autostart::kArgument) || !QSystemTrayIcon::isSystemTrayAvailable())
        controller.showToolbar();
    QObject::connect(&instance, &KDSingleApplication::messageReceived, &controller,
                     [&controller] { controller.showToolbar(); });

    return QApplication::exec();
}
