#pragma once

#include <QImage>
#include <QList>
#include <QRect>
#include <QString>

// Puts translations into a capture the way a scanlation's typesetter does:
// the original text is erased line by line, the background filled in from
// around each line, and the translation set in the text's own color, at its
// size and line spacing, aligned as it was.
namespace Typesetter {

struct Paragraph {
    QRect box;          // the whole paragraph, image pixels
    QList<QRect> lines; // each recognized line of it
    QString text;       // what to set there
    QString source = {}; // the text recognized there: tells the original's size and weight
};

QImage typeset(const QImage &source, const QList<Paragraph> &paragraphs);

} // namespace Typesetter
