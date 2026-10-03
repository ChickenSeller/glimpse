#include "TesseractEngine.h"

#include "ModelStore.h"

#include <QCoreApplication>
#include <QDir>
#include <QPainter>

#include <tesseract/baseapi.h>
#include <tesseract/resultiterator.h>

namespace {

// Pixels of blank margin around the image; Tesseract misses text touching the edge.
constexpr int kBorder = 12;

QString traineddataName(const QString &language)
{
    if (language == QLatin1String("zh_CN"))
        return QStringLiteral("chi_sim");
    if (language == QLatin1String("ja"))
        return QStringLiteral("jpn");
    return QStringLiteral("eng");
}

QString dataDir()
{
    return ModelStore::directory(QStringLiteral("tesseract"));
}

// Screen text is small and often light-on-dark. Tesseract does best on dark
// text on a light background at roughly 2x typical screen size.
QImage prepare(const QImage &source, int scale)
{
    QImage gray = source.convertToFormat(QImage::Format_Grayscale8);
    gray.setDevicePixelRatio(1.0);

    qint64 sum = 0;
    for (int y = 0; y < gray.height(); ++y) {
        const uchar *row = gray.constScanLine(y);
        for (int x = 0; x < gray.width(); ++x)
            sum += row[x];
    }
    if (gray.width() * gray.height() > 0 && sum / (qint64(gray.width()) * gray.height()) < 128)
        gray.invertPixels();

    if (scale > 1)
        gray = gray.scaled(gray.size() * scale, Qt::IgnoreAspectRatio, Qt::SmoothTransformation);

    QImage padded(gray.width() + 2 * kBorder, gray.height() + 2 * kBorder, QImage::Format_Grayscale8);
    padded.fill(255);
    QPainter(&padded).drawImage(kBorder, kBorder, gray);
    return padded;
}

} // namespace

TesseractEngine::TesseractEngine()
    : m_api(std::make_unique<tesseract::TessBaseAPI>())
{
}

TesseractEngine::~TesseractEngine()
{
    if (m_api)
        m_api->End();
}

QList<ModelFile> TesseractEngine::requiredModels(const QStringList &languages)
{
    QList<ModelFile> files;
    for (const QString &language : languages) {
        const QString name = traineddataName(language) + QStringLiteral(".traineddata");
        files << ModelFile{name,
                           QUrl(QStringLiteral("https://raw.githubusercontent.com/tesseract-ocr/tessdata_fast/main/")
                                + name),
                           QDir(dataDir()).filePath(name)};
    }
    return files;
}

std::unique_ptr<OcrEngine> TesseractEngine::create(const QStringList &languages, QString *error)
{
    QStringList models;
    for (const QString &language : languages)
        models << traineddataName(language);
    if (models.isEmpty())
        models << QStringLiteral("eng");

    std::unique_ptr<TesseractEngine> engine(new TesseractEngine);
    const QByteArray path = QDir::toNativeSeparators(dataDir()).toLocal8Bit();
    const QByteArray language = models.join(QLatin1Char('+')).toLatin1();
    if (engine->m_api->Init(path.constData(), language.constData(), tesseract::OEM_LSTM_ONLY) != 0) {
        if (error)
            *error = QCoreApplication::translate("Ocr", "Tesseract could not load its language data (%1).")
                         .arg(QString::fromLatin1(language));
        return nullptr;
    }
    engine->m_api->SetPageSegMode(tesseract::PSM_AUTO);
    return engine;
}

OcrResult TesseractEngine::recognize(const QImage &image)
{
    OcrResult result;
    if (image.isNull())
        return result;

    // HiDPI captures already have enough pixels per glyph.
    const int scale = image.devicePixelRatio() >= 1.75 ? 1 : 2;
    const QImage input = prepare(image, scale);

    m_api->SetImage(input.constBits(), input.width(), input.height(), 1, int(input.bytesPerLine()));
    m_api->SetSourceResolution(int(96 * image.devicePixelRatio() * scale));
    if (m_api->Recognize(nullptr) != 0) {
        result.error = QCoreApplication::translate("Ocr", "Tesseract failed to recognize the image.");
        return result;
    }

    std::unique_ptr<tesseract::ResultIterator> it(m_api->GetIterator());
    if (!it)
        return result;
    const auto level = tesseract::RIL_TEXTLINE;
    bool first = true;
    do {
        const std::unique_ptr<char[]> utf8(it->GetUTF8Text(level));
        if (!utf8)
            continue;
        const QString text = Ocr::tidyCjkSpacing(QString::fromUtf8(utf8.get()).trimmed());
        if (text.isEmpty())
            continue;
        // A blank line between blocks keeps paragraphs apart in the copied text.
        if (!first && it->IsAtBeginningOf(tesseract::RIL_BLOCK))
            result.lines << OcrLine{};
        first = false;

        int left = 0, top = 0, right = 0, bottom = 0;
        it->BoundingBox(level, &left, &top, &right, &bottom);
        const QRect box(QPoint((left - kBorder) / scale, (top - kBorder) / scale),
                        QPoint((right - kBorder) / scale, (bottom - kBorder) / scale));
        result.lines << OcrLine{text, box};
    } while (it->Next(level));
    return result;
}
