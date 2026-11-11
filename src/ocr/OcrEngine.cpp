#include "OcrEngine.h"

#include "app/Language.h"

#include <QCoreApplication>
#include <QFileInfo>

#include <algorithm>

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
    return {QStringLiteral("zh_CN"), QStringLiteral("zh_TW"), QStringLiteral("en"), QStringLiteral("ja"),
            QStringLiteral("fr")};
}

QString languageName(const QString &code)
{
    // Each in its own script, as its readers know it: not translated.
    if (code == QLatin1String("zh_CN"))
        return QStringLiteral("简体中文");
    if (code == QLatin1String("zh_TW"))
        return QStringLiteral("繁體中文");
    if (code == QLatin1String("ja"))
        return QStringLiteral("日本語");
    if (code == QLatin1String("fr"))
        return QStringLiteral("Français");
    return QStringLiteral("English");
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
            "Ocr", "The most accurate engine, especially for Chinese and Japanese; one model reads all these "
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

QList<ModelFile> allModels()
{
    QList<ModelFile> all;
#ifdef GLIMPSE_HAVE_TESSERACT
    all << TesseractEngine::requiredModels(supportedLanguages());
#endif
#ifdef GLIMPSE_HAVE_PADDLE
    all << PaddleOcrEngine::requiredModels();
#endif
    return all;
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

bool isColumnGap(const QRect &line, const QRect &next)
{
    // A word space is about a third of the text's height; the space around
    // inline code or a link a little more. The taller of the two: a word
    // without capitals or a comma has a small box.
    const int height = std::max({1, line.height(), next.height()});
    return next.left() - line.right() > height * 3 / 2;
}

QRect inkBox(const QImage &image, const QRect &box)
{
    const QRect r = box.intersected(image.rect());
    if (r.width() < 3 || r.height() < 3)
        return box;
    // The background: the most common of the box's edge colors, roughly
    // (each channel's median).
    QList<int> reds, greens, blues;
    const auto sample = [&](int x, int y) {
        const QRgb c = image.pixel(x, y);
        reds << qRed(c);
        greens << qGreen(c);
        blues << qBlue(c);
    };
    for (int x = r.left(); x <= r.right(); ++x) {
        sample(x, r.top());
        sample(x, r.bottom());
    }
    for (int y = r.top(); y <= r.bottom(); ++y) {
        sample(r.left(), y);
        sample(r.right(), y);
    }
    const auto median = [](QList<int> &v) {
        std::nth_element(v.begin(), v.begin() + v.size() / 2, v.end());
        return v.at(v.size() / 2);
    };
    const int br = median(reds), bg = median(greens), bb = median(blues);
    const auto distance = [&](QRgb c) {
        return std::abs(qRed(c) - br) + std::abs(qGreen(c) - bg) + std::abs(qBlue(c) - bb);
    };
    int farthest = 0;
    for (int y = r.top(); y <= r.bottom(); ++y) {
        for (int x = r.left(); x <= r.right(); ++x)
            farthest = std::max(farthest, distance(image.pixel(x, y)));
    }
    if (farthest < 60)
        return box; // too faint to tell
    int top = r.bottom(), bottom = r.top(), left = r.right(), right = r.left();
    for (int y = r.top(); y <= r.bottom(); ++y) {
        for (int x = r.left(); x <= r.right(); ++x) {
            if (distance(image.pixel(x, y)) * 2 >= farthest) {
                top = std::min(top, y);
                bottom = std::max(bottom, y);
                left = std::min(left, x);
                right = std::max(right, x);
            }
        }
    }
    if (top > bottom || left > right)
        return box;
    return QRect(QPoint(left, top), QPoint(right, bottom)).adjusted(-1, -1, 1, 1).intersected(r);
}

void dropLeadingIcon(const QImage &image, OcrLine &line)
{
    const QString text = line.text.trimmed();
    const qsizetype space = text.indexOf(QLatin1Char(' '));
    if (space <= 0 || space > 3 || text.size() - space < 3)
        return;
    const QString token = text.left(space);
    const bool symbols = std::none_of(token.cbegin(), token.cend(), [](QChar c) { return c.isLetterOrNumber(); });
    if (token.size() > 2 && !symbols)
        return;

    const QRect r = line.box.intersected(image.rect());
    if (r.width() < 8 || r.height() < 6)
        return;
    // Ink as in inkBox(): far from the box's edge color.
    QList<int> reds, greens, blues;
    for (int x = r.left(); x <= r.right(); ++x) {
        for (int y : {r.top(), r.bottom()}) {
            const QRgb c = image.pixel(x, y);
            reds << qRed(c);
            greens << qGreen(c);
            blues << qBlue(c);
        }
    }
    const auto median = [](QList<int> &v) {
        std::nth_element(v.begin(), v.begin() + v.size() / 2, v.end());
        return v.at(v.size() / 2);
    };
    const int br = median(reds), bg = median(greens), bb = median(blues);
    QList<int> column(r.width());
    int farthest = 0;
    for (int x = r.left(); x <= r.right(); ++x) {
        int most = 0;
        for (int y = r.top(); y <= r.bottom(); ++y) {
            const QRgb c = image.pixel(x, y);
            most = std::max(most, std::abs(qRed(c) - br) + std::abs(qGreen(c) - bg) + std::abs(qBlue(c) - bb));
        }
        column[x - r.left()] = most;
        farthest = std::max(farthest, most);
    }
    if (farthest < 60)
        return;
    // The first mark, then the first blank stretch after it: an icon stands
    // half the text's height or more away (a word space is far less).
    const int height = r.height();
    const auto ink = [&](int i) { return column[i] * 2 >= farthest; };
    int i = 0;
    while (i < r.width() && !ink(i))
        ++i;
    const int start = i;
    for (; i < r.width(); ++i) {
        if (ink(i))
            continue;
        int blank = i;
        while (blank < r.width() && !ink(blank))
            ++blank;
        if (blank - i >= std::max(4, height / 2)) {
            if (i - start > height * 8 / 5 || blank >= r.width())
                return; // too wide for an icon, or nothing after it
            line.text = text.mid(space + 1).trimmed();
            line.box.setLeft(r.left() + blank);
            return;
        }
        if (i - start > height * 8 / 5)
            return;
        i = blank - 1;
    }
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
