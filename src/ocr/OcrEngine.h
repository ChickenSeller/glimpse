#pragma once

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
};

// One OCR backend. Instances are created and used on a worker thread.
class OcrEngine
{
public:
    virtual ~OcrEngine() = default;
    virtual OcrResult recognize(const QImage &image) = 0;
};

namespace Ocr {

// Language codes shared with the UI languages: "zh_CN", "ja", "en".
QStringList supportedLanguages();
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

// Null with `error` set when the engine cannot run (e.g. missing models).
std::unique_ptr<OcrEngine> createEngine(const QString &engine, const QStringList &languages, QString *error);

// Removes the spaces engines put between CJK characters ("中 文" -> "中文").
QString tidyCjkSpacing(const QString &text);

} // namespace Ocr
