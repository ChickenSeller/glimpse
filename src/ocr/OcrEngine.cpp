#include "OcrEngine.h"

#include "app/Language.h"

#include <QCoreApplication>
#include <QFileInfo>

#ifdef GLIMPSE_HAVE_TESSERACT
#include "TesseractEngine.h"
#endif
#ifdef GLIMPSE_HAVE_WINDOWS_OCR
#include "WindowsOcrEngine.h"
#endif
#ifdef GLIMPSE_HAVE_PADDLE
#include "PaddleOcrEngine.h"
#endif

namespace {

const QString kTesseract = QStringLiteral("tesseract");
const QString kWindows = QStringLiteral("windows");
const QString kPaddle = QStringLiteral("paddle");

bool isCjk(QChar c)
{
    const char32_t u = c.unicode();
    return (u >= 0x3000 && u <= 0x30FF)    // CJK punctuation, Hiragana, Katakana
           || (u >= 0x3400 && u <= 0x9FFF) // CJK ideographs (incl. extension A)
           || (u >= 0xF900 && u <= 0xFAFF) // compatibility ideographs
           || (u >= 0xFF00 && u <= 0xFFEF); // full-width forms
}

} // namespace

QString OcrResult::text() const
{
    QStringList texts;
    for (const OcrLine &line : lines)
        texts << line.text;
    return texts.join(QLatin1Char('\n'));
}

namespace Ocr {

QStringList supportedLanguages()
{
    return {QStringLiteral("zh_CN"), QStringLiteral("ja"), QStringLiteral("en")};
}

QStringList defaultLanguages()
{
    // The system language plus English, which shows up in most UIs anyway.
    const QString system = Language::systemLanguage();
    if (system == QLatin1String("en"))
        return {system};
    return {system, QStringLiteral("en")};
}

QStringList engineIds()
{
    QStringList ids;
#ifdef GLIMPSE_HAVE_PADDLE
    ids << kPaddle; // most accurate, one model for all our languages
#endif
#ifdef GLIMPSE_HAVE_WINDOWS_OCR
    ids << kWindows;
#endif
#ifdef GLIMPSE_HAVE_TESSERACT
    ids << kTesseract;
#endif
    return ids;
}

QStringList allEngineIds()
{
    return {kPaddle, kWindows, kTesseract};
}

QString engineUnavailableReason(const QString &id)
{
    if (engineIds().contains(id))
        return {};
    if (id == kWindows)
        return QCoreApplication::translate("Ocr", "Only available on Windows.");
    return QCoreApplication::translate("Ocr", "Not included in this build.");
}

QString engineName(const QString &id)
{
    if (id == kTesseract)
        return QStringLiteral("Tesseract");
    if (id == kWindows)
        return QCoreApplication::translate("Ocr", "Windows OCR");
    if (id == kPaddle)
        return QStringLiteral("PaddleOCR");
    return id;
}

QString engineDescription(const QString &id)
{
    if (id == kPaddle)
        return QCoreApplication::translate(
            "Ocr", "The most accurate engine, especially for Chinese and Japanese; one model reads all three "
                   "languages. Downloads about 31 MB of models on first use.");
    if (id == kWindows)
        return QCoreApplication::translate(
            "Ocr", "Built into Windows, nothing to download. Reads one language at a time: the first selected "
                   "language whose Windows OCR component is installed.");
    if (id == kTesseract)
        return QCoreApplication::translate(
            "Ocr", "Classic open-source engine. Downloads 2-4 MB per selected language on first use.");
    return {};
}

QString defaultEngine()
{
    const QStringList ids = engineIds();
    return ids.isEmpty() ? QString() : ids.first();
}

QList<ModelFile> missingModels(const QString &engine, const QStringList &languages)
{
    QList<ModelFile> required;
    Q_UNUSED(engine)
    Q_UNUSED(languages)
#ifdef GLIMPSE_HAVE_TESSERACT
    if (engine == kTesseract)
        required = TesseractEngine::requiredModels(languages);
#endif
#ifdef GLIMPSE_HAVE_PADDLE
    if (engine == kPaddle)
        required = PaddleOcrEngine::requiredModels(); // one model covers all languages
#endif
    QList<ModelFile> missing;
    for (const ModelFile &file : std::as_const(required)) {
        if (!QFileInfo::exists(file.path))
            missing << file;
    }
    return missing;
}

std::unique_ptr<OcrEngine> createEngine(const QString &engine, const QStringList &languages, QString *error)
{
#ifdef GLIMPSE_HAVE_WINDOWS_OCR
    if (engine == kWindows)
        return WindowsOcrEngine::create(languages, error);
#endif
#ifdef GLIMPSE_HAVE_PADDLE
    if (engine == kPaddle)
        return PaddleOcrEngine::create(error);
#endif
#ifdef GLIMPSE_HAVE_TESSERACT
    if (engine == kTesseract)
        return TesseractEngine::create(languages, error);
#endif
    Q_UNUSED(languages)
    if (error)
        *error = QCoreApplication::translate("Ocr", "The text recognition engine \"%1\" is not available in this build.")
                     .arg(engine);
    return nullptr;
}

QString tidyCjkSpacing(const QString &text)
{
    QString result;
    result.reserve(text.size());
    for (qsizetype i = 0; i < text.size(); ++i) {
        const QChar c = text.at(i);
        if (c == QLatin1Char(' ') && i > 0 && i + 1 < text.size() && isCjk(text.at(i - 1)) && isCjk(text.at(i + 1)))
            continue;
        result += c;
    }
    return result;
}

} // namespace Ocr
