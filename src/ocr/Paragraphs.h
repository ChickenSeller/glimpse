#pragma once

#include "OcrEngine.h"

#include <QList>
#include <QRect>
#include <QString>

namespace Ocr {

// Recognized lines joined into the paragraphs they were wrapped from.
struct Paragraph {
    QString text;
    QRect box;
    int lineHeight = 0; // its first line's
    QRect last;         // its last line
    QList<QRect> lines;
};

// Joins OCR lines into paragraphs: a line continues the paragraph above when
// it starts close below it, overlaps it horizontally, and the line above it
// was full (its first word would not have fitted there; a line short of that
// ended its paragraph) and it is not indented further. Translating whole
// paragraphs gives far better results than line by line. In reading order.
QList<Paragraph> paragraphsOf(QList<OcrLine> lines);

} // namespace Ocr
