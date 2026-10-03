#pragma once

#include <QString>
#include <QStringList>

// UI language. Codes are "en", "zh_CN", "ja"; an empty code follows the system.
namespace Language {

QStringList supported();

// The language's name in that language ("简体中文"), as shown in the picker.
QString nativeName(const QString &code);

// What an empty code resolves to on this system.
QString systemLanguage();

// Installs the translators for `code` (empty = system). Safe to call again at
// runtime: widgets then receive QEvent::LanguageChange.
void apply(const QString &code);

} // namespace Language
