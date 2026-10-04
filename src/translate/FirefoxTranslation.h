#pragma once

#include "ocr/OcrEngine.h" // ModelFile

#include <QList>
#include <QString>
#include <QStringList>

// Offline translation with Firefox's engine (Mozilla Bergamot) and its small
// per-direction models (35-70 MB, fetched on first use). The engine lives in
// glimpse-bergamot (built with MSVC) and is loaded at run time.
//
// Models translate between English and another language; other pairs go
// through English. The source language is taken from the script of the text
// (kana: Japanese, Hangul: Korean, Han: Chinese, otherwise English).
namespace FirefoxTranslation {

// Why the engine cannot run in this build; empty if it can.
QString unavailableReason();

// "ja", "ko", "zh-Hans" or "en".
QString detectLanguage(const QString &text);

// Files still to download for translating `texts` into `target`. Sets
// `unsupported` (and returns nothing) when a needed direction has no model.
QList<ModelFile> missingModels(const QStringList &texts, const QString &target, QString *unsupported);

// Blocking: call off the GUI thread, after the models are downloaded. Calls
// are serialized; loaded models are kept until release().
QStringList translate(const QStringList &texts, const QString &target, QString *error);
void release();

} // namespace FirefoxTranslation
