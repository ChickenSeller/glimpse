#include "Typesetter.h"

#include <QFontMetricsF>
#include <QGuiApplication>
#include <QPainter>
#include <QTextLayout>

#include <algorithm>
#include <array>
#include <cmath>

namespace Typesetter {

namespace {

struct Rgb {
    double r = 0, g = 0, b = 0;
};

Rgb rgbOf(QRgb c)
{
    return {double(qRed(c)), double(qGreen(c)), double(qBlue(c))};
}

Rgb operator+(const Rgb &a, const Rgb &b)
{
    return {a.r + b.r, a.g + b.g, a.b + b.b};
}

Rgb operator-(const Rgb &a, const Rgb &b)
{
    return {a.r - b.r, a.g - b.g, a.b - b.b};
}

Rgb operator*(const Rgb &a, double t)
{
    return {a.r * t, a.g * t, a.b * t};
}

double distance(const Rgb &a, const Rgb &b)
{
    return std::abs(a.r - b.r) + std::abs(a.g - b.g) + std::abs(a.b - b.b);
}

QRgb toQRgb(const Rgb &c)
{
    const auto channel = [](double v) { return int(std::lround(std::clamp(v, 0.0, 255.0))); };
    return qRgb(channel(c.r), channel(c.g), channel(c.b));
}

// Per channel, so that a stray text pixel among them does not count.
Rgb median(QList<Rgb> colors)
{
    if (colors.isEmpty())
        return {};
    const auto mid = [&colors](double Rgb::*channel) {
        QList<double> values;
        for (const Rgb &c : std::as_const(colors))
            values << c.*channel;
        std::nth_element(values.begin(), values.begin() + values.size() / 2, values.end());
        return values.at(values.size() / 2);
    };
    return {mid(&Rgb::r), mid(&Rgb::g), mid(&Rgb::b)};
}

enum class Side { Left, Right, Top, Bottom };

// The background along one side of `r`: at each step along it, the median
// of the few pixels just outside it, then smoothed along the side, so that
// text touching the edge (or a neighbour's) leaves no mark.
QList<Rgb> edge(const QImage &image, const QRect &r, Side side)
{
    const bool vertical = side == Side::Left || side == Side::Right;
    const int count = vertical ? r.height() : r.width();
    QList<Rgb> raw;
    raw.reserve(count);
    for (int i = 0; i < count; ++i) {
        QList<Rgb> samples;
        for (int depth = 1; depth <= 3; ++depth) {
            QPoint p;
            switch (side) {
            case Side::Left: p = {r.left() - depth, r.top() + i}; break;
            case Side::Right: p = {r.right() + depth, r.top() + i}; break;
            case Side::Top: p = {r.left() + i, r.top() - depth}; break;
            case Side::Bottom: p = {r.left() + i, r.bottom() + depth}; break;
            }
            if (image.rect().contains(p))
                samples << rgbOf(image.pixel(p));
        }
        if (samples.isEmpty()) { // at the image's edge: the box's own edge
            const QPoint p = side == Side::Left    ? QPoint(r.left(), r.top() + i)
                             : side == Side::Right ? QPoint(r.right(), r.top() + i)
                             : side == Side::Top   ? QPoint(r.left() + i, r.top())
                                                   : QPoint(r.left() + i, r.bottom());
            samples << rgbOf(image.pixel(p));
        }
        raw << median(samples);
    }
    QList<Rgb> smooth;
    smooth.reserve(count);
    constexpr int kWindow = 3; // each side of the sample
    for (int i = 0; i < count; ++i)
        smooth << median(raw.mid(std::max(0, i - kWindow), std::min<qsizetype>(count, i + kWindow + 1)
                                                               - std::max(0, i - kWindow)));
    return smooth;
}

// Fills `r` with a background grown in from its four sides (a Coons patch):
// flat colors and gradients come out seamless.
void erase(QImage &image, const QRect &r)
{
    if (r.width() < 2 || r.height() < 2)
        return;
    const QList<Rgb> left = edge(image, r, Side::Left);
    const QList<Rgb> right = edge(image, r, Side::Right);
    const QList<Rgb> top = edge(image, r, Side::Top);
    const QList<Rgb> bottom = edge(image, r, Side::Bottom);
    const Rgb c00 = (left.first() + top.first()) * 0.5;
    const Rgb c10 = (right.first() + top.last()) * 0.5;
    const Rgb c01 = (left.last() + bottom.first()) * 0.5;
    const Rgb c11 = (right.last() + bottom.last()) * 0.5;
    for (int y = 0; y < r.height(); ++y) {
        const double v = double(y) / (r.height() - 1);
        auto *line = reinterpret_cast<QRgb *>(image.scanLine(r.top() + y));
        for (int x = 0; x < r.width(); ++x) {
            const double u = double(x) / (r.width() - 1);
            const Rgb across = left[y] * (1 - u) + right[y] * u;
            const Rgb down = top[x] * (1 - v) + bottom[x] * v;
            const Rgb corners = c00 * ((1 - u) * (1 - v)) + c10 * (u * (1 - v)) + c01 * ((1 - u) * v) + c11 * (u * v);
            line[r.left() + x] = toQRgb(across + down - corners);
        }
    }
}

int medianOf(QList<int> values)
{
    if (values.isEmpty())
        return 0;
    std::nth_element(values.begin(), values.begin() + values.size() / 2, values.end());
    return values.at(values.size() / 2);
}

// How the text in some lines of an image looks.
struct Ink {
    QColor color;
    int height = 0;     // a line's ink, top to bottom (OCR boxes may have margins)
    double stroke = 0;  // how thick its strokes are, on average
};

// The text's color is that of the pixels farthest from the background; its
// strokes are the short horizontal runs of text pixels.
Ink measure(const QImage &image, const QList<QRect> &lines, const Rgb &background)
{
    double farthest = 0;
    for (const QRect &line : lines) {
        const QRect r = line.intersected(image.rect());
        for (int y = r.top(); y <= r.bottom(); ++y) {
            for (int x = r.left(); x <= r.right(); ++x)
                farthest = std::max(farthest, distance(rgbOf(image.pixel(x, y)), background));
        }
    }
    // Too faint to tell: black or white, whichever reads.
    if (farthest < 60) {
        const bool light = qGray(toQRgb(background)) > 140;
        return {light ? QColor(0x20, 0x20, 0x20) : QColor(0xf5, 0xf5, 0xf5)};
    }
    Rgb sum;
    int n = 0;
    QList<int> runs;    // horizontal stretches of text pixels: stroke widths
    QList<int> heights;
    for (const QRect &line : lines) {
        const QRect r = line.intersected(image.rect());
        int inkTop = -1, inkBottom = -1;
        for (int y = r.top(); y <= r.bottom(); ++y) {
            const qsizetype runsBefore = runs.size();
            int run = 0;
            for (int x = r.left(); x <= r.right() + 1; ++x) {
                bool ink = false;
                if (x <= r.right()) {
                    const Rgb c = rgbOf(image.pixel(x, y));
                    const double d = distance(c, background);
                    if (d >= farthest * 0.6) {
                        sum = sum + c;
                        ++n;
                    }
                    ink = d >= farthest * 0.5;
                }
                if (ink) {
                    ++run;
                } else if (run) {
                    runs << run;
                    run = 0;
                }
            }
            if (runs.size() > runsBefore) {
                if (inkTop < 0)
                    inkTop = y;
                inkBottom = y;
            }
        }
        if (inkTop >= 0)
            heights << inkBottom - inkTop + 1;
    }
    const int height = medianOf(heights);
    // Strokes, not bars: long runs are lines across (an underline, a dash).
    double strokes = 0;
    int count = 0;
    for (int run : std::as_const(runs)) {
        if (run * 3 <= std::max(3, height)) {
            strokes += run;
            ++count;
        }
    }
    return {QColor(toQRgb(sum * (1.0 / std::max(1, n)))), height, count ? strokes / count : 0};
}

// The original text's size and weight, found by setting it in our font:
// the pixel size at which its ink is as tall as in the capture, and bold if
// its strokes are nearer our bold's than our regular's.
// `width`: how wide the original's one line was (0: it was wrapped). Its
// ink may be taller than its text (an icon in the line, two lines read as
// one), which width tells: set that size, the text must still fit.
QFont originalFont(const QString &source, const Ink &ink, int width)
{
    QFont font = QGuiApplication::font();
    font.setPixelSize(100);
    const QFontMetricsF at100(font);
    const qreal tall = at100.tightBoundingRect(source).height();
    if (tall <= 0 || ink.height <= 0)
        return {};
    qreal size = 100.0 * ink.height / tall;
    const qreal wide = at100.horizontalAdvance(source);
    if (width > 0 && wide > 0)
        size = std::min(size, 100.0 * width * 1.1 / wide);
    font.setPixelSize(std::max(6, int(std::lround(size))));

    const QString sample = source.left(40);
    const auto strokeOf = [&sample](QFont f, bool bold) {
        f.setBold(bold);
        const QFontMetrics metrics(f);
        QImage image(metrics.horizontalAdvance(sample) + 8, metrics.height() + 8, QImage::Format_RGB32);
        image.fill(Qt::white);
        QPainter p(&image);
        p.setRenderHint(QPainter::TextAntialiasing);
        p.setFont(f);
        p.setPen(Qt::black);
        p.drawText(4, 4 + metrics.ascent(), sample);
        p.end();
        return measure(image, {image.rect()}, Rgb{255, 255, 255}).stroke;
    };
    const double regular = strokeOf(font, false);
    const double bold = strokeOf(font, true);
    // Leaning to regular: a capture's antialiasing thickens strokes a little.
    font.setBold(ink.stroke > 0 && bold > regular && ink.stroke >= regular + (bold - regular) * 0.7);
    return font;
}

// Whether the lines are centered in the paragraph (a heading, a caption)
// rather than starting at its left.
bool centered(const QRect &box, const QList<QRect> &lines)
{
    if (lines.size() < 2)
        return false;
    int ragged = 0;
    for (const QRect &line : lines) {
        if (std::abs(line.center().x() - box.center().x()) > box.width() / 20)
            return false;
        ragged = std::max(ragged, line.left() - box.left());
    }
    return ragged > box.width() / 10;
}

struct Layout {
    QFont font;
    qreal lineHeight = 0;
    int lines = 0;
    qreal width = 0; // the widest line
};

Layout layOut(QTextLayout &layout, const QFont &font, qreal width, qreal lineHeight)
{
    layout.setFont(font);
    layout.beginLayout();
    Layout result{font, lineHeight};
    for (QTextLine line = layout.createLine(); line.isValid(); line = layout.createLine()) {
        line.setLineWidth(width);
        line.setPosition(QPointF(0, result.lines * lineHeight));
        result.width = std::max(result.width, line.naturalTextWidth());
        ++result.lines;
    }
    layout.endLayout();
    return result;
}

// `room`: how wide the text may run, from the paragraph's left; a single
// line may use free space to its right before it is made smaller.
void setText(QPainter &painter, const Paragraph &paragraph, const Ink &ink, int room)
{
    const QString text = paragraph.text.trimmed();
    if (text.isEmpty())
        return;
    const QList<QRect> lines = paragraph.lines.isEmpty() ? QList<QRect>{paragraph.box} : paragraph.lines;
    const QRect &box = paragraph.box;

    // The original's size: its lines' height is about the glyphs' (OCR boxes
    // hug the ink); its spacing: from one line's top to the next.
    QList<int> heights;
    for (const QRect &line : lines)
        heights << line.height();
    const int height = std::max(6, ink.height > 0 ? std::min(ink.height, medianOf(heights)) : medianOf(heights));
    QList<int> steps;
    for (qsizetype i = 1; i < lines.size(); ++i) {
        if (lines[i].top() > lines[i - 1].top())
            steps << lines[i].top() - lines[i - 1].top();
    }
    const qreal pitch = steps.isEmpty() ? height * 1.35 : medianOf(steps);

    QTextOption option(centered(box, lines) ? Qt::AlignHCenter : Qt::AlignLeft);
    option.setWrapMode(QTextOption::WrapAtWordBoundaryOrAnywhere);
    QTextLayout layout(text);
    layout.setTextOption(option);

    // The original's size and weight; without the original text, the
    // translation's own glyphs as tall as the original's ink. Then shrunk
    // only as far as needed to fit the paragraph's box.
    QFont font = paragraph.source.trimmed().isEmpty()
                     ? QFont()
                     : originalFont(paragraph.source.trimmed(), ink, lines.size() == 1 ? box.width() : 0);
    qreal start = font.pixelSize();
    if (start > 0) {
        // Narrowed by the original's width when its ink was taller than its text.
        start = std::clamp(start, std::min(height * 0.3, 7.0), height * 1.6);
    } else {
        font = QGuiApplication::font();
        font.setPixelSize(100);
        const qreal tall = QFontMetricsF(font).tightBoundingRect(text.left(40)).height();
        start = std::clamp(tall > 0 ? 100.0 * height / tall : height * 0.9, height * 0.7, height * 1.6);
    }
    const qreal smallest = std::max(7.0, start * 0.5);
    const qreal width = lines.size() == 1 ? std::max(box.width(), room) : box.width();
    Layout fit;
    for (qreal size = start;; size = std::max(smallest, size * 0.92)) {
        font.setPixelSize(int(std::lround(size)));
        const qreal lineHeight = std::max(pitch * size / start, QFontMetricsF(font).height());
        fit = layOut(layout, font, width, lineHeight);
        const qreal used = (fit.lines - 1) * lineHeight + QFontMetricsF(font).height();
        if ((used <= box.height() + height * 0.4 && fit.width <= width) || size <= smallest)
            break;
    }

    // One line stays centered on where the text was; more start at the first.
    const qreal fontHeight = QFontMetricsF(fit.font).height();
    const qreal top = fit.lines == 1 || lines.size() == 1
                          ? box.center().y() + 0.5 - ((fit.lines - 1) * fit.lineHeight + fontHeight) / 2
                          : lines.first().center().y() + 0.5 - fontHeight / 2;
    painter.setPen(ink.color);
    layout.draw(&painter, QPointF(box.left(), top));
}

} // namespace

QImage typeset(const QImage &source, const QList<Paragraph> &paragraphs)
{
    const QImage original = source.convertToFormat(QImage::Format_RGB32);
    QImage image = original;
    image.setDevicePixelRatio(1.0); // boxes are in image pixels

    // How the text looks first, from the untouched capture.
    QList<Ink> styles;
    for (const Paragraph &p : paragraphs) {
        const QRect ring = p.box.adjusted(-3, -3, 3, 3).intersected(original.rect());
        QList<Rgb> around;
        for (Side side : {Side::Left, Side::Right, Side::Top, Side::Bottom})
            around << edge(original, ring, side);
        styles << measure(original, p.lines.isEmpty() ? QList<QRect>{p.box} : p.lines, median(around));
    }

    // Then every paragraph erased, each from what is around it by then (a
    // neighbour already erased gives clean samples), before any is set:
    // a translation running long must not be erased by the next one.
    for (const Paragraph &p : paragraphs) {
        // A little room for antialiased edges, in proportion to the text.
        const int pad = std::clamp((p.lines.isEmpty() ? p.box : p.lines.first()).height() / 5, 2, 6);
        erase(image, p.box.adjusted(-pad, -pad, pad, pad).intersected(image.rect()));
    }

    QPainter painter(&image);
    painter.setRenderHint(QPainter::TextAntialiasing);
    painter.setRenderHint(QPainter::Antialiasing);
    for (qsizetype i = 0; i < paragraphs.size(); ++i) {
        // Up to the next text on the same height (with a gap), or the edge.
        const QRect &box = paragraphs.at(i).box;
        int right = image.width() - 4;
        for (qsizetype j = 0; j < paragraphs.size(); ++j) {
            const QRect &other = paragraphs.at(j).box;
            if (j != i && other.left() > box.left() && other.top() < box.bottom() && other.bottom() > box.top())
                right = std::min(right, other.left() - box.height());
        }
        setText(painter, paragraphs.at(i), styles.at(i), std::max(box.width(), right - box.left()));
    }
    painter.end();

    image.setDevicePixelRatio(source.devicePixelRatio());
    return image;
}

} // namespace Typesetter
