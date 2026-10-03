#include "BarcodeScanner.h"

// An installed ZXing (find_package) puts its headers under ZXing/; a source
// build from FetchContent exposes them unprefixed.
#if __has_include(<ZXing/ReadBarcode.h>)
#include <ZXing/ReadBarcode.h>
#else
#include <ReadBarcode.h>
#endif

namespace {

// Small codes on screen are often just a few pixels per module; the
// detector does better on a nearest-neighbour upscale.
constexpr int kUpscaleBelow = 600;

QList<ScannedCode> decode(const QImage &gray, int scale)
{
    const ZXing::ImageView view(gray.constBits(), gray.width(), gray.height(), ZXing::ImageFormat::Lum,
                                int(gray.bytesPerLine()));
    ZXing::ReaderOptions options;
    options.setTryHarder(true);
    options.setTryRotate(true);
    options.setTryInvert(true); // light-on-dark codes, common in dark themes

    QList<ScannedCode> codes;
    for (const ZXing::Barcode &barcode : ZXing::ReadBarcodes(view, options)) {
        if (!barcode.isValid())
            continue;
        ScannedCode code;
        code.text = QString::fromStdString(barcode.text());
        code.format = QString::fromStdString(ZXing::ToString(barcode.format()));
        for (const auto &point : barcode.position())
            code.corners << QPoint(point.x / scale, point.y / scale);
        codes.append(code);
    }
    return codes;
}

} // namespace

QList<ScannedCode> scanBarcodes(const QImage &image)
{
    if (image.isNull())
        return {};

    const QImage gray = image.convertToFormat(QImage::Format_Grayscale8);
    QList<ScannedCode> codes = decode(gray, 1);
    if (codes.isEmpty() && std::min(gray.width(), gray.height()) < kUpscaleBelow)
        codes = decode(gray.scaled(gray.size() * 2, Qt::IgnoreAspectRatio, Qt::FastTransformation), 2);
    return codes;
}
