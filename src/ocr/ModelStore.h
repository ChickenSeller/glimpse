#pragma once

#include "OcrEngine.h"

#include <QList>
#include <QString>

class QWidget;

// Where OCR models live and how they get there.
namespace ModelStore {

// <app data>/ocr/<engine>, created on demand.
QString directory(const QString &engine);

// Downloads `files` with a modal progress dialog. Returns false if the user
// canceled or a download failed (an error has then been shown).
bool download(const QList<ModelFile> &files, QWidget *parent);

} // namespace ModelStore
