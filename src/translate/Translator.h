#pragma once

#include <QString>
#include <QStringList>

#include <functional>

class QObject;

// Text translation through online services, chosen in the settings like the
// OCR engine. Every engine needs the user's own API key.
namespace Translate {

// "google-free" (no key), "firefox" (Bergamot, offline), "local" (llama.cpp,
// offline), "deepl", "google", "microsoft", "claude".
QStringList engineIds();
QString engineName(const QString &id);
// One or two sentences on quality, cost and what the key is, for the settings page.
QString engineDescription(const QString &id);
QString defaultEngine();
// Most online services need the user's API key; the free Google service and
// the offline engines do not.
bool needsKey(const QString &engine);

// Target languages: "zh-Hans", "zh-Hant", "ja", "en", "ko", "fr", "de", "es", "ru".
QStringList targetLanguages();
// The language's name in itself ("日本語"), so it reads right in any UI language.
QString languageName(const QString &code);
// The UI language's counterpart (zh_CN -> zh-Hans, ja -> ja, else en).
QString defaultTarget();

// Translates `texts` (one entry per paragraph; the source language is detected)
// into `target` with `engine`, using the key stored in the settings. `done` is
// called on `context`'s thread with one translation per text, or with an error
// message. Nothing is called if `context` is destroyed first.
using Callback = std::function<void(const QStringList &translations, const QString &error)>;
void translate(const QString &engine, const QStringList &texts, const QString &target, QObject *context,
               Callback done);

} // namespace Translate
