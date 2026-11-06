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
    const bool detecting = m_header < 0;
    if (detecting) {
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
    if (shift == 0) {
        // Header and footer are only known from a frame that lined up; what
        // changed here may have been no scrolling at all (a hover effect).
        if (detecting) {
            m_bodyHeight += m_footer;
            m_header = m_footer = -1;
        }
        return Result::NoOverlap;
    }

    // Newly revealed rows are the last ones of the scrolling part: `shift`
    // of them, or when Tracking, those beyond the lowest point so far.
    m_position += shift;
    const int revealed = m_scrolling == Scrolling::Tracking ? m_position - m_maxPosition : shift;
    m_previous = frame;
    m_previousSignature = std::move(sig);
    m_lastShift = shift;
    if (revealed <= 0)
        return Result::Moved;
    if (revealed > contentHeight())
        return Result::Gap; // only after undoLast(): rows in between were never seen here
    appendRows(frame, h - m_footer - revealed, revealed);
    m_segments.push_back(revealed);
    m_footerRows = frame.copy(0, h - m_footer, frame.width(), m_footer);
    m_maxPosition = m_position;
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
    // When Tracking, shifts up are tried too, each right after the same shift
    // down; frames are grabbed often, so the true shift is a near-perfect match,
    // and no guess is made on blank overlaps (the user's steps vary).
    const bool tracking = m_scrolling == Scrolling::Tracking;
    int bestInformative = 0;
    double bestInformativeScore = 0.0;
    int bestBlank = 0;
    // A narrow overlap can be off by a row or two; the user can always scroll
    // back, unlike a program that cannot tell.
    const int minOverlap = tracking ? std::max(kMinOverlap, content / 8) : kMinOverlap;
    // 1, 2, ... or 1, -1, 2, -2, ...: ties keep the smaller distance.
    std::vector<int> shifts;
    for (int distance = 1; distance <= content - minOverlap; ++distance) {
        shifts.push_back(distance);
        if (tracking)
            shifts.push_back(-distance);
    }
    for (const int shift : shifts) {
        const int distance = std::abs(shift);
        int compared = 0, matched = 0;   // all overlapping segments
        int dCompared = 0, dMatched = 0; // segments with content
        // previous row first + shift + i  <->  next row first + i (down);
        // previous row first + i  <->  next row first + distance + i (up)
        for (int i = skipTop; i < std::min(content - distance, content - skipBottom); ++i) {
            const size_t a = size_t(first + std::max(shift, 0) + i) * kSegments;
            const size_t b = size_t(first + std::max(-shift, 0) + i) * kSegments;
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
        if (compared < minOverlap * kSegments || double(matched) / compared < kMinMatch)
            continue;
        if (dCompared >= kMinDistinctive) {
            const double score = double(dMatched) / dCompared;
            // Ties keep the smaller shift: periodic content repeats at larger ones.
            if (score >= kMinMatch && score > bestInformativeScore + 1e-9) {
                bestInformativeScore = score;
                bestInformative = shift;
            }
        } else if (!tracking && (bestBlank == 0 || std::abs(shift - m_lastShift) < std::abs(bestBlank - m_lastShift))) {
            bestBlank = shift;
        }
    }
    if (bestInformative != 0)
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

bool ScrollStitcher::undoLast()
{
    if (m_segments.empty())
        return false;
    const int rows = m_segments.back();
    m_segments.pop_back();
    m_bodyHeight -= rows;
    // The view has not moved: once it is lined up again, what lies below the
    // stitched part is appended anew. When scrolling down by program
    // (revealed = shift), this would leave a gap, so it is only for Tracking.
    m_maxPosition -= rows;
    return true;
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
    const QImage footer = footerImage();
    for (int y = 0; y < footer.height(); ++y)
        std::memcpy(result.scanLine(m_bodyHeight + y), footer.constScanLine(y), size_t(result.width()) * 4);
    return result;
}

QImage ScrollStitcher::footerImage() const
{
    // Until a second frame lined up, the footer is still the first frame's.
    return m_footerRows.isNull() && m_footer > 0
               ? m_previous.copy(0, m_previous.height() - m_footer, m_previous.width(), m_footer)
               : m_footerRows;
}

const uchar *ScrollStitcher::rowAt(int y, const QImage &footer) const
{
    if (y < m_bodyHeight)
        return m_body.constScanLine(y);
    y -= m_bodyHeight;
    return y < footer.height() ? footer.constScanLine(y) : nullptr;
}

QImage ScrollStitcher::preview(const QSize &maxSize) const
{
    const int w = m_body.width();
    const int h = height();
    if (m_frames == 0 || w <= 0 || h <= 0 || maxSize.isEmpty())
        return {};
    const double scale = std::min({double(maxSize.width()) / w, double(maxSize.height()) / h, 1.0});
    const QSize target(std::max(1, int(w * scale)), std::max(1, int(h * scale)));
    // Picked rows and columns at twice the size, then smoothed down: text
    // turns grey instead of breaking up.
    const QSize sampled(std::min(w, target.width() * 2), std::min(h, target.height() * 2));
    QImage out(sampled, QImage::Format_RGB32);
    const QImage footer = footerImage();
    for (int y = 0; y < sampled.height(); ++y) {
        const auto *src = reinterpret_cast<const quint32 *>(rowAt(int(qint64(y) * h / sampled.height()), footer));
        auto *dst = reinterpret_cast<quint32 *>(out.scanLine(y));
        for (int x = 0; x < sampled.width(); ++x)
            dst[x] = src ? src[qint64(x) * w / sampled.width()] : 0xff000000;
    }
    return sampled == target ? out : out.scaled(target, Qt::IgnoreAspectRatio, Qt::SmoothTransformation);
}
