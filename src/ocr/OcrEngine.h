#pragma once

#include <QByteArray>
#include <QImage>
#include <QList>
#include <QRect>
#include <QString>
#include <QStringList>
#include <QUrl>

#include <memory>

struct OcrLine {
    QString text;
    QRect box; // in image pixels
};

struct OcrResult {
    QList<OcrLine> lines;
    QString error; // non-empty if recognition failed

    QString text() const;
};

// A data file an engine needs (trained model, dictionary), fetched on first use.
struct ModelFile {
    QString name;
    QUrl url;
    QString path; // where it must end up on disk
    QByteArray sha256 = {}; // hex; when known, a download must match it
};

// One OCR backend. Instances are created and used on a worker thread.
class OcrEngine
{
public:
    virtual ~OcrEngine() = default;
    virtual OcrResult recognize(const QImage &image) = 0;
};

namespace Ocr {

// Language codes, partly shared with the UI languages: "zh_CN", "zh_TW",
// "en", "ja", "fr".
QStringList supportedLanguages();
// Its name in its own script.
QString languageName(const QString &code);
QStringList defaultLanguages();

// Engines compiled into this build, best first ("paddle", "windows", "tesseract").
QStringList engineIds();
// Every engine Glimpse knows, available here or not (for showing them disabled).
QStringList allEngineIds();
// Why `id` cannot be used in this build/on this platform; empty if it can.
QString engineUnavailableReason(const QString &id);
QString engineName(const QString &id);
// One or two sentences on strengths and requirements, for the settings page.
QString engineDescription(const QString &id);
QString defaultEngine();

// Models the engine needs for `languages` that are not downloaded yet.
QList<ModelFile> missingModels(const QString &engine, const QStringList &languages);
// Every model any engine of this build may download (for mirroring them).
QList<ModelFile> allModels();

// Null with `error` set when the engine cannot run (e.g. missing models).
std::unique_ptr<OcrEngine> createEngine(const QString &engine, const QStringList &languages, QString *error);

// Removes the spaces engines put between CJK characters ("中 文" -> "中文").
QString tidyCjkSpacing(const QString &text);

// Whether `next`, to the right of `line` on the same row, is too far from it
// to continue it: wider than a couple of characters' height, as between
// columns, not words.
bool isColumnGap(const QRect &line, const QRect &next);

// `box` narrowed to the text's ink in `image`: detectors pad their boxes,
// and the room between lines (which tells paragraphs from lists) is then lost.
QRect inkBox(const QImage &image, const QRect &box);

// An icon before a line's text (a menu's, a list's) is read as a letter or
// symbol ("8 Appearance", "Q Search"): when the line starts with a token of
// one or two characters standing well apart from the rest, it is dropped
// from the text and the box starts after it, leaving the icon alone.
void dropLeadingIcon(const QImage &image, OcrLine &line);

} // namespace Ocr
