#include "Autostart.h"

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QSaveFile>
#include <QSettings>
#include <QStandardPaths>

namespace {

// The program itself; inside an AppImage, the AppImage, which is what stays put.
QString program()
{
    const QString appImage = qEnvironmentVariable("APPIMAGE");
    return appImage.isEmpty() ? QCoreApplication::applicationFilePath() : appImage;
}

#ifdef Q_OS_WIN
const QString kRunKey = QStringLiteral("HKEY_CURRENT_USER\\Software\\Microsoft\\Windows\\CurrentVersion\\Run");
const QString kValueName = QStringLiteral("Glimpse");

QString command()
{
    return QStringLiteral("\"%1\" %2").arg(QDir::toNativeSeparators(program()), Autostart::kArgument);
}
#else
QString desktopFile()
{
    return QDir(QStandardPaths::writableLocation(QStandardPaths::GenericConfigLocation))
        .filePath(QStringLiteral("autostart/glimpse.desktop"));
}

QByteArray desktopEntry()
{
    // Exec quoting: the path in double quotes, with ", `, $ and \ escaped.
    QString path = program();
    for (const char *c : {"\\", "\"", "`", "$"})
        path.replace(QLatin1String(c), QLatin1Char('\\') + QLatin1String(c));
    return QStringLiteral("[Desktop Entry]\n"
                          "Type=Application\n"
                          "Name=Glimpse\n"
                          "Exec=\"%1\" %2\n"
                          "Icon=glimpse\n"
                          "Terminal=false\n"
                          "X-GNOME-Autostart-enabled=true\n")
        .arg(path, Autostart::kArgument)
        .toUtf8();
}
#endif

} // namespace

namespace Autostart {

bool isSupported()
{
#if defined(Q_OS_WIN) || defined(Q_OS_LINUX)
    return true;
#else
    return false;
#endif
}

bool isEnabled()
{
#ifdef Q_OS_WIN
    return QSettings(kRunKey, QSettings::NativeFormat).contains(kValueName);
#elif defined(Q_OS_LINUX)
    return QFile::exists(desktopFile());
#else
    return false;
#endif
}

void setEnabled(bool enabled)
{
#ifdef Q_OS_WIN
    QSettings run(kRunKey, QSettings::NativeFormat);
    if (enabled)
        run.setValue(kValueName, command());
    else
        run.remove(kValueName);
#elif defined(Q_OS_LINUX)
    if (!enabled) {
        QFile::remove(desktopFile());
        return;
    }
    QDir().mkpath(QFileInfo(desktopFile()).absolutePath());
    QSaveFile file(desktopFile());
    if (file.open(QIODevice::WriteOnly)) {
        file.write(desktopEntry());
        file.commit();
    }
#else
    Q_UNUSED(enabled)
#endif
}

void refresh()
{
    if (!isEnabled())
        return;
#ifdef Q_OS_WIN
    if (QSettings(kRunKey, QSettings::NativeFormat).value(kValueName).toString() != command())
        setEnabled(true);
#elif defined(Q_OS_LINUX)
    QFile file(desktopFile());
    if (!file.open(QIODevice::ReadOnly) || file.readAll() != desktopEntry()) {
        file.close();
        setEnabled(true);
    }
#endif
}

} // namespace Autostart
