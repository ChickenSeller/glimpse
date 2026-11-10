#pragma once

#include <QString>
#include <QStringList>

#include <functional>

class QObject;

// Text translation through online services or on this computer, chosen in
// the settings like the OCR engine.
namespace Translate {

// "google-free" (no key), "index" (Bilibili, no key), "firefox" (Bergamot, offline), "local" (llama.cpp,
// offline), "deepl", "google", "microsoft", "claude", "openai" (any OpenAI-compatible API).
QStringList engineIds();
QString engineName(const QString &id);
// One or two sentences on quality, cost and what the key is, for the settings page.
QString engineDescription(const QString &id);
QString defaultEngine();
// Most online services need the user's API key; the free Google service and
// the offline engines do not. (The OpenAI-compatible one takes a key, but a
// server on this computer may not want one.)
bool needsKey(const QString &engine);
bool takesKey(const QString &engine);
// Whether the settings have what the engine needs (key, address, model);
// the files of the offline engines are not checked.
bool isConfigured(const QString &engine);

// The language-model engines take a prompt the user can change: "claude",
// "openai" and "local" (for general models; see LocalModel::hasOwnPrompt).
// {language} in it stands for the target language.
bool takesPrompt(const QString &engine);
QString defaultPrompt(const QString &engine);
// The prompt to send: the user's or the default, for `target`.
QString prompt(const QString &engine, const QString &target);

// Target languages: "zh-Hans", "zh-Hant", "ja", "en", "ko", "fr", "de", "es", "ru".
QStringList targetLanguages();
// The language's name in itself ("日本語"), so it reads right in any UI language.
QString languageName(const QString &code);
// The UI language's counterpart (zh_CN -> zh-Hans, ja -> ja, else en).
QString defaultTarget();

// Translates `texts` (one entry per paragraph; the source language is detected)
// into `target` with `engine`, using the key stored in the settings. `done` is
// called on `context`'s thread with one translation per text, or with an error
// message. Nothing is called if `context` is destroyed first. `localModel`
// picks the local engine's model (see LocalModel; empty: the settings' one).
using Callback = std::function<void(const QStringList &translations, const QString &error)>;
void translate(const QString &engine, const QStringList &texts, const QString &target, QObject *context,
               Callback done, const QString &localModel = QString());

} // namespace Translate
