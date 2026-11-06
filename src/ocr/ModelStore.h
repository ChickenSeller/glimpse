#pragma once

#include "OcrEngine.h"

#include <QList>
#include <QString>
#include <QUrl>

class QWidget;

// Where downloaded models live and how they get there.
namespace ModelStore {

// <app data>/ocr/<engine>, created on demand.
QString directory(const QString &engine);

// Downloads `files` with a modal progress dialog, trying sources() in turn.
// Returns false if the user canceled or a download failed (an error has
// then been shown).
bool download(const QList<ModelFile> &files, QWidget *parent);

// Asks for `files`, which `message` says what they are needed for: to
// download them, or to choose copies already on this computer (fetched in a
// browser or a download manager, or brought from another computer). True
// once all of them are in place.
bool obtain(const QList<ModelFile> &files, const QString &title, const QString &message, QWidget *parent);

// Copies `source` to where `file` goes, checked like a download. Empty on
// success, else the error.
QString install(const QString &source, const ModelFile &file, QWidget *parent);

// Every download is also offered by a mirror, which keeps the official
// address under its own: https://huggingface.co/a/b is
// <mirror>/huggingface.co/a/b (tools/mirror-downloads.py fills one).
enum class SourceOrder {
    MirrorFirst,   // the mirror, then the official address
    OfficialFirst, // the official address, then the mirror
    OfficialOnly,
};
SourceOrder sourceOrder();
void setSourceOrder(SourceOrder order);
QString defaultMirror();
QString mirror(); // the mirror's base URL
void setMirror(const QString &base);

QUrl mirrorUrl(const QUrl &official, const QString &base);
// The addresses to try for `official`, in order.
QList<QUrl> sources(const QUrl &official);

} // namespace ModelStore
