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
