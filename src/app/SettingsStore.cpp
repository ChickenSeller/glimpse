#include "SettingsStore.h"

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QVariant>

namespace {

// config.ini once useProgramFolder() takes it; empty: the system's.
QString s_file;

} // namespace

namespace SettingsStore {

void useProgramFolder()
{
    const QDir dir(QCoreApplication::applicationDirPath());
    const QString file = dir.filePath(QStringLiteral("config.ini"));

    // Glimpse 0.7.1 and 0.7.2 kept it in Glimpse/Glimpse.ini, QSettings' own
    // layout. Its folder goes too when nothing else is left in it.
    const QString earlier = dir.filePath(QStringLiteral("Glimpse/Glimpse.ini"));
    if (!QFileInfo::exists(file) && QFileInfo::exists(earlier) && QFile::rename(earlier, file))
        dir.rmdir(QStringLiteral("Glimpse"));

    const bool existed = QFileInfo::exists(file);
    if (existed) {
        if (!QFileInfo(file).isWritable())
            return;
    } else {
        // Can the folder take it? Tried by making the (empty) file.
        QFile probe(file);
        if (!probe.open(QIODevice::WriteOnly))
            return;
    }

    s_file = file;
    // Read by kImageAnnotator's settings (see cmake/PatchImageAnnotator.cmake).
    qApp->setProperty("glimpseSettingsFile", file);
    // The first time, everything set so far comes along: from Glimpse/Glimpse.ini
    // when it could not be moved (in use for a moment), else from the system,
    // whose copy stays for an older Glimpse.
    if (!existed) {
        const QSettings from = QFileInfo::exists(earlier) ? QSettings(earlier, QSettings::IniFormat) : QSettings();
        QSettings settings = open();
        const QStringList keys = from.allKeys();
        for (const QString &key : keys)
            settings.setValue(key, from.value(key));
    }
}

void useFile(const QString &file)
{
    s_file = file;
    qApp->setProperty("glimpseSettingsFile", file);
}

QSettings open()
{
    if (s_file.isEmpty())
        return QSettings();
    return QSettings(s_file, QSettings::IniFormat);
}

} // namespace SettingsStore
