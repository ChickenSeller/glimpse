#include "app/AppIcon.h"
#include "app/AppSettings.h"
#include "app/CaptureController.h"
#include "app/Language.h"

#include <QApplication>

#include <kdsingleapplication.h>

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    QApplication::setApplicationName(QStringLiteral("Glimpse"));
    QApplication::setOrganizationName(QStringLiteral("Glimpse"));
    QApplication::setApplicationVersion(QStringLiteral(GLIMPSE_VERSION));
    QApplication::setWindowIcon(appIcon());

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
    controller.showToolbar();
    QObject::connect(&instance, &KDSingleApplication::messageReceived, &controller,
                     [&controller] { controller.showToolbar(); });

    return QApplication::exec();
}
