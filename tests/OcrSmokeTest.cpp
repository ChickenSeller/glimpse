// glimpse-ocr-test: renders a few lines of English, Chinese and Japanese the way
// they look on screen, runs each OCR engine on them and prints what it read.
//
//   glimpse-ocr-test [--download] [--save <png>] [engine...]
//
// --download fetches missing models (into the same directory Glimpse uses);
// without it, engines whose models are missing are skipped.

#include "ocr/ModelStore.h"
#include "ocr/OcrEngine.h"

#include <QApplication>
#include <QElapsedTimer>
#include <QFont>
#include <QPainter>

#include <cstdio>

namespace {

void print(const QString &text)
{
    const QByteArray utf8 = text.toUtf8();
    std::fwrite(utf8.constData(), 1, size_t(utf8.size()), stdout);
    std::fputc('\n', stdout);
    std::fflush(stdout);
}

const QStringList kSample = {
    QStringLiteral("Glimpse OCR test 2026, version 0.1"),
    QStringLiteral("截图工具的文字识别测试"),
    QStringLiteral("スクリーンショットの文字認識テスト"),
};

QImage renderSample()
{
    QFont font = QApplication::font();
    font.setPixelSize(16); // typical UI text at 100% scaling
    const QFontMetrics fm(font);
    int width = 0;
    for (const QString &line : kSample)
        width = std::max(width, fm.horizontalAdvance(line));
    const int lineHeight = fm.height() + 6;

    QImage image(width + 24, int(kSample.size()) * lineHeight + 16, QImage::Format_RGB32);
    image.fill(Qt::white);
    QPainter p(&image);
    p.setFont(font);
    p.setPen(QColor(0x20, 0x20, 0x20));
    for (int i = 0; i < kSample.size(); ++i)
        p.drawText(12, 8 + i * lineHeight + fm.ascent(), kSample.at(i));
    return image;
}

} // namespace

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    // Same names as Glimpse, so models land in (and are read from) its data directory.
    QApplication::setApplicationName(QStringLiteral("Glimpse"));
    QApplication::setOrganizationName(QStringLiteral("Glimpse"));

    QStringList args = QApplication::arguments().mid(1);
    const bool download = args.removeAll(QStringLiteral("--download")) > 0;
    QString savePath;
    if (const qsizetype i = args.indexOf(QStringLiteral("--save")); i >= 0 && i + 1 < args.size()) {
        savePath = args.at(i + 1);
        args.remove(i, 2);
    }
    const QStringList engines = args.isEmpty() ? Ocr::engineIds() : args;
    const QStringList languages = Ocr::supportedLanguages();

    const QImage sample = renderSample();
    if (!savePath.isEmpty())
        sample.save(savePath);
    print(QStringLiteral("Sample (%1x%2):").arg(sample.width()).arg(sample.height()));
    for (const QString &line : kSample)
        print(QStringLiteral("  ") + line);

    int failures = 0;
    for (const QString &engine : engines) {
        print(QStringLiteral("\n== %1").arg(Ocr::engineName(engine)));
        const QList<ModelFile> missing = Ocr::missingModels(engine, languages);
        if (!missing.isEmpty()) {
            if (!download) {
                print(QStringLiteral("  skipped: models missing (run with --download)"));
                continue;
            }
            if (!ModelStore::download(missing, nullptr)) {
                print(QStringLiteral("  FAILED: model download"));
                ++failures;
                continue;
            }
        }

        QElapsedTimer timer;
        timer.start();
        QString error;
        std::unique_ptr<OcrEngine> ocr = Ocr::createEngine(engine, languages, &error);
        if (!ocr) {
            print(QStringLiteral("  FAILED to start: ") + error);
            ++failures;
            continue;
        }
        const qint64 loadMs = timer.restart();
        const OcrResult result = ocr->recognize(sample);
        const qint64 runMs = timer.elapsed();
        if (!result.error.isEmpty()) {
            print(QStringLiteral("  FAILED: ") + result.error);
            ++failures;
            continue;
        }
        print(QStringLiteral("  load %1 ms, recognize %2 ms").arg(loadMs).arg(runMs));
        for (const OcrLine &line : result.lines)
            print(QStringLiteral("  [%1,%2 %3x%4] %5")
                      .arg(line.box.x())
                      .arg(line.box.y())
                      .arg(line.box.width())
                      .arg(line.box.height())
                      .arg(line.text));
    }
    return failures == 0 ? 0 : 1;
}
