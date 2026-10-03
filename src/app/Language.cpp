#include "Language.h"

#include <QCoreApplication>
#include <QLibraryInfo>
#include <QLocale>
#include <QTranslator>

namespace {

const QString kEnglish = QStringLiteral("en");
const QString kChinese = QStringLiteral("zh_CN");
const QString kJapanese = QStringLiteral("ja");

QTranslator *appTranslator = nullptr;
QTranslator *qtTranslator = nullptr;

void removeTranslators()
{
    for (QTranslator **translator : {&appTranslator, &qtTranslator}) {
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

    // Qt's own strings: standard buttons, file dialogs, key names.
    qtTranslator = new QTranslator;
    if (qtTranslator->load(QStringLiteral("qtbase_") + language,
                           QLibraryInfo::path(QLibraryInfo::TranslationsPath)))
        QCoreApplication::installTranslator(qtTranslator);

    // Ours are compiled into the binary by qt_add_translations().
    appTranslator = new QTranslator;
    if (appTranslator->load(QStringLiteral(":/i18n/glimpse_") + language))
        QCoreApplication::installTranslator(appTranslator);
}

} // namespace Language
