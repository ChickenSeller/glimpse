#pragma once

#include <QImage>
#include <QList>
#include <QPolygon>
#include <QString>

struct ScannedCode {
    QString text;
    QString format;  // e.g. "QR Code", "EAN-13"
    QPolygon corners; // in image pixels: top-left, top-right, bottom-right, bottom-left
};

// Finds and decodes every QR code / barcode in `image` (ZXing-C++).
QList<ScannedCode> scanBarcodes(const QImage &image);
