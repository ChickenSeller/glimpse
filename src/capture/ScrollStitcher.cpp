#include "ScrollStitcher.h"

#include <QHashFunctions>

#include <algorithm>
#include <cstdlib>
#include <cstring>

namespace {

// Rows that must overlap between two frames for a shift to count.
constexpr int kMinOverlap = 16;
// Share of overlapping row segments that must match. Only a guard against
// nonsense (the best-matching shift wins anyway); it must tolerate a caret,
// hover effects, a rotating banner and headers that turn sticky once scrolled.
constexpr double kMinMatch = 0.75;
// Static bands larger than this are not header/footer but an unscrolled view.
constexpr int kMinContent = 24;
// Content segments an overlap needs before it can pin down the shift on its own.
constexpr int kMinDistinctive = 16;

// Columns left out at each side when comparing rows: a scroll bar (whose
// thumb moves with every step) or an overlay scroll indicator lives there.
int sideMargin(const QImage &image)
{
    return std::min(qRound(24 * image.devicePixelRatio()), image.width() / 10);
}

} // namespace

ScrollStitcher::Signature ScrollStitcher::signature(const QImage &image)
{
    Signature sig;
    sig.hashes.resize(size_t(image.height()) * kSegments);
    sig.content.resize(sig.hashes.size());
    const int margin = sideMargin(image);
    const int usable = image.width() - 2 * margin;
    for (int y = 0; y < image.height(); ++y) {
        const auto *row = reinterpret_cast<const quint32 *>(image.constScanLine(y)) + margin;
        for (int k = 0; k < kSegments; ++k) {
            const int from = usable * k / kSegments;
            const int to = usable * (k + 1) / kSegments;
            const size_t i = size_t(y) * kSegments + k;
            sig.hashes[i] = qHashBits(row + from, size_t(to - from) * 4);
            sig.content[i] = !std::all_of(row + from, row + to, [first = row[from]](quint32 px) { return px == first; });
        }
    }
    return sig;
}

bool ScrollStitcher::rowsEqual(const Signature &a, int rowA, const Signature &b, int rowB) const
{
    return std::equal(a.hashes.begin() + qsizetype(rowA) * kSegments, a.hashes.begin() + qsizetype(rowA + 1) * kSegments,
                      b.hashes.begin() + qsizetype(rowB) * kSegments);
}

ScrollStitcher::Result ScrollStitcher::addFrame(const QImage &input)
{
    QImage frame = input.convertToFormat(QImage::Format_RGB32);
    frame.setDevicePixelRatio(input.devicePixelRatio());
    Signature sig = signature(frame);

    if (m_frames == 0) {
        m_body = QImage(frame.width(), frame.height() * 4, QImage::Format_RGB32);
        m_body.setDevicePixelRatio(frame.devicePixelRatio());
        appendRows(frame, 0, frame.height());
        m_previous = frame;
        m_previousSignature = std::move(sig);
        m_frames = 1;
        return Result::Added;
    }

    if (frame.size() != m_previous.size() || sig == m_previousSignature)
        return Result::NoChange;

    const int h = frame.height();
    if (m_header < 0) {
        // Rows identical at the same place in both frames do not scroll.
        int top = 0;
        while (top < h && rowsEqual(sig, top, m_previousSignature, top))
            ++top;
        int bottom = 0;
        while (bottom < h - top && rowsEqual(sig, h - 1 - bottom, m_previousSignature, h - 1 - bottom))
            ++bottom;
        if (h - top - bottom < kMinContent) // only a sliver moved: no usable header/footer split
            top = bottom = 0;
        m_header = top;
        m_footer = bottom;
        // The first frame's footer rows were appended as body; they belong at the end.
        m_bodyHeight -= m_footer;
    }

    // Elements that became sticky after the first frame (a navigation bar
    // pinned on scroll) sit unchanged in place in both frames and hide what
    // scrolls beneath them: leave those rows out of the matching.
    int staticTop = 0;
    while (m_header + staticTop < h - m_footer && rowsEqual(sig, m_header + staticTop, m_previousSignature, m_header + staticTop))
        ++staticTop;
    int staticBottom = 0;
    while (staticBottom < h - m_footer - m_header - staticTop
           && rowsEqual(sig, h - m_footer - 1 - staticBottom, m_previousSignature, h - m_footer - 1 - staticBottom))
        ++staticBottom;
    if (contentHeight() - staticTop - staticBottom < kMinContent)
        staticTop = staticBottom = 0; // (almost) nothing moved: no sticky split to make

    const int shift = findShift(sig, staticTop, staticBottom);
    if (shift <= 0)
        return Result::NoOverlap;

    // Newly revealed rows are the last `shift` rows of the scrolling part.
    appendRows(frame, h - m_footer - shift, shift);
    m_footerRows = frame.copy(0, h - m_footer, frame.width(), m_footer);
    m_lastShift = shift;
    m_previous = frame;
    m_previousSignature = std::move(sig);
    ++m_frames;
    return Result::Added;
}

int ScrollStitcher::findShift(const Signature &next, int skipTop, int skipBottom) const
{
    const int first = m_header;
    const int content = contentHeight();
    const Signature &prev = m_previousSignature;

    // Solid segments (blank background) match at any shift. A shift is judged
    // on the segments that carry content; if the overlap is blank for every
    // plausible shift, the content alone cannot tell, and the shift closest to
    // the last one (scrolling moves by a fixed number of wheel notches) is taken.
    int bestInformative = 0;
    double bestInformativeScore = 0.0;
    int bestBlank = 0;
    for (int shift = 1; shift <= content - kMinOverlap; ++shift) {
        int compared = 0, matched = 0;   // all overlapping segments
        int dCompared = 0, dMatched = 0; // segments with content
        // previous row first + shift + i  <->  next row first + i
        for (int i = skipTop; i < std::min(content - shift, content - skipBottom); ++i) {
            const size_t a = size_t(first + shift + i) * kSegments;
            const size_t b = size_t(first + i) * kSegments;
            for (int k = 0; k < kSegments; ++k) {
                const bool same = prev.hashes[a + k] == next.hashes[b + k];
                ++compared;
                matched += same;
                if (prev.content[a + k]) {
                    ++dCompared;
                    dMatched += same;
                }
            }
        }
        if (compared < kMinOverlap * kSegments || double(matched) / compared < kMinMatch)
            continue;
        if (dCompared >= kMinDistinctive) {
            const double score = double(dMatched) / dCompared;
            // Ties keep the smaller shift: periodic content repeats at larger ones.
            if (score >= kMinMatch && score > bestInformativeScore + 1e-9) {
                bestInformativeScore = score;
                bestInformative = shift;
            }
        } else if (bestBlank == 0 || std::abs(shift - m_lastShift) < std::abs(bestBlank - m_lastShift)) {
            bestBlank = shift;
        }
    }
    if (bestInformative > 0)
        return bestInformative;
    return m_lastShift > 0 ? bestBlank : 0;
}

void ScrollStitcher::appendRows(const QImage &source, int first, int count)
{
    if (count <= 0)
        return;
    if (m_bodyHeight + count > m_body.height()) {
        QImage grown(m_body.width(), std::max(m_body.height() * 2, m_bodyHeight + count), QImage::Format_RGB32);
        grown.setDevicePixelRatio(m_body.devicePixelRatio());
        for (int y = 0; y < m_bodyHeight; ++y)
            std::memcpy(grown.scanLine(y), m_body.constScanLine(y), size_t(m_body.width()) * 4);
        m_body = grown;
    }
    for (int y = 0; y < count; ++y)
        std::memcpy(m_body.scanLine(m_bodyHeight + y), source.constScanLine(first + y), size_t(source.width()) * 4);
    m_bodyHeight += count;
}

int ScrollStitcher::height() const
{
    return m_bodyHeight + std::max(0, m_footer);
}

int ScrollStitcher::contentHeight() const
{
    return m_previous.height() - std::max(0, m_header) - std::max(0, m_footer);
}

QImage ScrollStitcher::image() const
{
    if (m_frames == 0)
        return {};
    QImage result(m_body.width(), height(), QImage::Format_RGB32);
    result.setDevicePixelRatio(m_body.devicePixelRatio());
    for (int y = 0; y < m_bodyHeight; ++y)
        std::memcpy(result.scanLine(y), m_body.constScanLine(y), size_t(result.width()) * 4);
    // Until a second frame lined up, the footer is still the first frame's.
    const QImage footer = m_footerRows.isNull() && m_footer > 0
                              ? m_previous.copy(0, m_previous.height() - m_footer, m_previous.width(), m_footer)
                              : m_footerRows;
    for (int y = 0; y < footer.height(); ++y)
        std::memcpy(result.scanLine(m_bodyHeight + y), footer.constScanLine(y), size_t(result.width()) * 4);
    return result;
}
