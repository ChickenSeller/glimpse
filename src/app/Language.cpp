#include "Language.h"

#include <QCoreApplication>
#include <QDir>
#include <QLibraryInfo>
#include <QLocale>
#include <QTranslator>

namespace {

const QString kEnglish = QStringLiteral("en");
const QString kChinese = QStringLiteral("zh_CN");
const QString kJapanese = QStringLiteral("ja");

QTranslator *appTranslator = nullptr;
QTranslator *qtTranslator = nullptr;
QTranslator *annotatorTranslator = nullptr;

void removeTranslators()
{
    for (QTranslator **translator : {&appTranslator, &qtTranslator, &annotatorTranslator}) {
        if (*translator) {
            QCoreApplication::removeTranslator(*translator);
            delete *translator;
            *translator = nullptr;
        }
    }
}

} // namespace

namespace Language {

QStringList supported()
{
    return {kEnglish, kChinese, kJapanese};
}

QString nativeName(const QString &code)
{
    if (code == kChinese)
        return QStringLiteral("简体中文");
    if (code == kJapanese)
        return QStringLiteral("日本語");
    return QStringLiteral("English");
}

QString systemLanguage()
{
    // uiLanguages() is in the user's order of preference, e.g. "zh-Hans-CN", "ja-JP".
    const QStringList preferred = QLocale::system().uiLanguages();
    for (const QString &tag : preferred) {
        const QLocale locale(tag);
        if (locale.language() == QLocale::Chinese)
            return kChinese;
        if (locale.language() == QLocale::Japanese)
            return kJapanese;
        if (locale.language() == QLocale::English)
            return kEnglish;
    }
    return kEnglish;
}

void apply(const QString &code)
{
    const QString language = supported().contains(code) ? code : systemLanguage();

    removeTranslators();
    QLocale::setDefault(QLocale(language));
    if (language == kEnglish)
        return; // source strings are English

    // Qt's own strings: standard buttons, file dialogs, key names. The package
    // has them next to the executable, merged into qt_<language>.qm by
    // windeployqt; a Qt installation (development, Linux) has qtbase_<language>.qm.
    qtTranslator = new QTranslator;
    const QStringList qtDirectories = {
        QDir(QCoreApplication::applicationDirPath()).filePath(QStringLiteral("translations")),
        QLibraryInfo::path(QLibraryInfo::TranslationsPath),
    };
    bool qtLoaded = false;
    for (const QString &directory : qtDirectories) {
        for (const QString &name : {QStringLiteral("qt_"), QStringLiteral("qtbase_")}) {
            if (!qtLoaded && qtTranslator->load(name + language, directory))
                qtLoaded = true;
        }
    }
    if (qtLoaded)
        QCoreApplication::installTranslator(qtTranslator);

    // kImageAnnotator ships its own catalog: next to the executable for our
    // in-tree build, in the system data directory for a distro package.
    annotatorTranslator = new QTranslator;
    const QString annotatorFile = QStringLiteral("kImageAnnotator_") + language;
    if (annotatorTranslator->load(annotatorFile, QDir(QCoreApplication::applicationDirPath()).filePath(QStringLiteral("translations")))
        || annotatorTranslator->load(annotatorFile, QStringLiteral("/usr/share/kImageAnnotator/translations")))
        QCoreApplication::installTranslator(annotatorTranslator);

    // Ours are compiled into the binary by qt_add_translations().
    appTranslator = new QTranslator;
    if (appTranslator->load(QStringLiteral(":/i18n/glimpse_") + language))
        QCoreApplication::installTranslator(appTranslator);
}

} // namespace Language
