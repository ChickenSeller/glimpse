#include "Paragraphs.h"

#include <QRegularExpression>

#include <algorithm>

namespace Ocr {

namespace {

bool isCjk(QChar c)
{
    const char16_t u = c.unicode();
    return (u >= 0x3040 && u <= 0x30ff) || (u >= 0x3400 && u <= 0x9fff) || (u >= 0xac00 && u <= 0xd7af)
           || (u >= 0xff00 && u <= 0xffef);
}

// Appends `text` to `paragraph`: CJK text runs on without spaces; other
// scripts need one.
void joinInto(QString &paragraph, const QString &text)
{
    if (paragraph.isEmpty() || text.isEmpty()) {
        paragraph += text;
        return;
    }
    const bool cjkJoin = isCjk(paragraph.back()) && isCjk(text.front());
    paragraph += (cjkJoin ? QString() : QStringLiteral(" ")) + text;
}

// The width of one character of `line`, on average.
qreal charWidth(const OcrLine &line)
{
    return qreal(line.box.width()) / std::max<qsizetype>(1, line.text.trimmed().size());
}

// How wide the first word of `line` is: up to its first space, or one
// character of CJK text, which breaks anywhere.
qreal firstWordWidth(const OcrLine &line)
{
    const QString text = line.text.trimmed();
    if (text.isEmpty())
        return 0;
    if (isCjk(text.front()))
        return charWidth(line);
    const qsizetype space = text.indexOf(QLatin1Char(' '));
    return charWidth(line) * (space < 0 ? text.size() : space);
}

// Whether most lines start with a number that counts up, as in an editor or
// a listing with line numbers: each line stands on its own there.
bool numberedLines(const QList<OcrLine> &lines)
{
    static const QRegularExpression leading(QStringLiteral("^\\s*(\\d+)\\b"));
    int numbered = 0, counting = 0;
    qint64 previous = -1;
    for (const OcrLine &line : lines) {
        const QRegularExpressionMatch m = leading.match(line.text);
        if (!m.hasMatch())
            continue;
        ++numbered;
        const qint64 n = m.captured(1).toLongLong();
        if (n > previous)
            ++counting;
        previous = n;
    }
    return lines.size() >= 3 && numbered * 10 >= lines.size() * 6 && counting * 10 >= numbered * 8;
}

} // namespace

QList<Paragraph> paragraphsOf(QList<OcrLine> lines)
{
    std::sort(lines.begin(), lines.end(), [](const OcrLine &a, const OcrLine &b) { return a.box.top() < b.box.top(); });
    const bool eachLine = numberedLines(lines);
    QList<Paragraph> paragraphs;
    for (const OcrLine &line : std::as_const(lines)) {
        const QString text = line.text.trimmed();
        if (text.isEmpty())
            continue;
        Paragraph *current = nullptr;
        for (Paragraph &p : paragraphs) {
            if (eachLine)
                break;
            const int gap = line.box.top() - p.last.bottom();
            const bool overlaps = line.box.left() < p.box.right() && line.box.right() > p.box.left();
            // Lines of one paragraph are of one size: a heading over its
            // text, or a label over a value, is not one.
            const int height = line.box.height();
            const bool sameSize = height * 4 <= p.lineHeight * 5 && p.lineHeight * 4 <= height * 5;
            if (!overlaps || !sameSize || gap <= -p.lineHeight / 2 || gap >= p.lineHeight * 0.8)
                continue;
            const qreal room = std::max(p.box.right(), line.box.right()) - p.last.right();
            const bool lastWasFull = room <= firstWordWidth(line) + charWidth(line);
            const bool indented = line.box.left() > p.last.left() + 2 * charWidth(line);
            current = lastWasFull && !indented ? &p : nullptr;
        }
        if (!current) {
            paragraphs.append({text, line.box, line.box.height(), line.box, {line.box}});
            continue;
        }
        joinInto(current->text, text);
        current->box |= line.box;
        current->last = line.box;
        current->lines << line.box;
    }
    // Reading order: top to bottom, then left to right.
    std::sort(paragraphs.begin(), paragraphs.end(), [](const Paragraph &a, const Paragraph &b) {
        return a.box.top() == b.box.top() ? a.box.left() < b.box.left() : a.box.top() < b.box.top();
    });
    return paragraphs;
}

} // namespace Ocr
