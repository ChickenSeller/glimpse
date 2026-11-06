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
// Content segments an overlap needs before it can pin down the shift on its own
// (two rows' worth of the 32 segments a row is cut into): a sliver of overlap
// at a large shift matches by chance.
constexpr int kMinDistinctive = 64;
// A column of segments moves with a shift when this share of its content
// rows match (a caret or a hover effect costs a few).
constexpr double kColumnMatch = 0.9;
// Share of the content that, the same in place, shows the view unmoved
// right away. Animations (a spinning icon, an animated picture) keep frames
// from ever repeating exactly: down to kMaybeUnmoved, the view is unmoved
// when what changed is found nowhere else in the previous frame. (A large
// still part, a toolbar above a list that scrolled, keeps the share high
// too; there what changed is the list, found a few rows away.)
constexpr double kUnmoved = 0.95;
constexpr double kMaybeUnmoved = 0.85;
// Share of the changed segments that, found shifted in the previous frame,
// show that something scrolled.
constexpr double kExplained = 0.3;
// Otherwise a shift is taken when it stands out: its content matches at least
// this well, and this many times better than at any shift apart from it.
// Overshooting leaves every shift matching poorly and none standing out.
constexpr double kMinPeak = 0.4;
constexpr double kPeakMargin = 1.5;
constexpr int kPeakWidth = 2; // shifts this close to the peak are the peak
// Content rows a column needs for that to tell anything.
constexpr int kColumnContent = 4;
// Share of the content that must move with a shift. Columns that do not move
// at all (a sidebar that sticks once scrolled, buttons fixed to the window's
// edge) disagree with every shift; the content beside them decides. Whole
// columns matching at a wrong shift are next to impossible, so a third is
// plenty, and leaves room for a sidebar halfway between sticking and moving.
constexpr double kMinMoving = 0.3;

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

double ScrollStitcher::contentMatch(const Signature &next, int shift) const
{
    const Signature &prev = m_previousSignature;
    const int rows = int(prev.hashes.size() / kSegments);
    size_t content = 0, same = 0;
    for (int y = std::max(0, -shift); y < std::min(rows, rows - shift); ++y) {
        const size_t a = size_t(y + shift) * kSegments;
        const size_t b = size_t(y) * kSegments;
        for (int k = 0; k < kSegments; ++k) {
            if (prev.content[a + k]) {
                ++content;
                same += prev.hashes[a + k] == next.hashes[b + k];
            }
        }
    }
    return content > 0 ? double(same) / content : 0.0;
}

bool ScrollStitcher::unmoved(const Signature &next) const
{
    const double still = contentMatch(next, 0);
    if (still >= kUnmoved)
        return true;
    if (still < kMaybeUnmoved)
        return false;
    // The content segments that changed in place.
    const Signature &prev = m_previousSignature;
    const int rows = int(next.hashes.size() / kSegments);
    std::vector<std::pair<int, int>> changed; // row, segment
    for (int y = 0; y < rows; ++y) {
        for (int k = 0; k < kSegments; ++k) {
            const size_t i = size_t(y) * kSegments + k;
            if (next.content[i] && next.hashes[i] != prev.hashes[i])
                changed.emplace_back(y, k);
        }
    }
    if (changed.empty())
        return true;
    // Scrolled content turns up a few rows away in the previous frame; an
    // animation's new look turns up nowhere.
    const size_t needed = std::max<size_t>(kMinDistinctive, size_t(kExplained * changed.size()));
    for (int shift = 1; shift < rows; ++shift) {
        for (const int direction : {shift, -shift}) {
            size_t found = 0;
            for (const auto &[y, k] : changed) {
                const int from = y + direction;
                if (from >= 0 && from < rows && next.hashes[size_t(y) * kSegments + k] == prev.hashes[size_t(from) * kSegments + k])
                    ++found;
            }
            if (found >= needed)
                return false;
        }
    }
    return true;
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
    if (unmoved(sig)) {
        // Kept as the latest look of this place.
        m_previous = frame;
        m_previousSignature = std::move(sig);
        return Result::NoChange;
    }

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
    // Failing that, the best shift most of the scrolled columns move with.
    int bestMoving = 0;
    double bestMovingScore = 0.0;
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
    // How well each column of segments matches staying where it was: a column
    // that a shift fits worse than that did not scroll (a sticky sidebar,
    // buttons fixed to the window's edge, the still part of a view where only
    // a pane scrolls) and is left out of judging that shift.
    int stillCompared[kSegments] = {}, stillMatched[kSegments] = {};
    for (int i = skipTop; i < content - skipBottom; ++i) {
        const size_t row = size_t(first + i) * kSegments;
        for (int k = 0; k < kSegments; ++k) {
            if (prev.content[row + k]) {
                ++stillCompared[k];
                stillMatched[k] += prev.hashes[row + k] == next.hashes[row + k];
            }
        }
    }
    std::vector<std::pair<int, double>> scores; // of the shifts with enough content
    for (const int shift : shifts) {
        const int distance = std::abs(shift);
        int compared = 0, matched = 0;   // all overlapping segments
        int dCompared = 0, dMatched = 0; // segments with content
        // Content proves a shift only where it differs from what the previous
        // frame had at the place it moved to: a separator line, a scroll bar's
        // track or the inside of a coloured band match a few rows away without
        // anything having moved. The column and peak tests go by this alone.
        int pCompared = 0, pMatched = 0;
        int columnCompared[kSegments] = {}, columnMatched[kSegments] = {}; // proving content, per column
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
                    if (prev.hashes[a + k] != prev.hashes[b + k]) {
                        ++pCompared;
                        pMatched += same;
                        ++columnCompared[k];
                        columnMatched[k] += same;
                    }
                }
            }
        }
        if (compared < minOverlap * kSegments)
            continue;
        if (dCompared >= kMinDistinctive) {
            // The content of the columns that move with this shift, out of
            // that of the columns that did not rather stay put.
            int moving = 0, scrolled = 0, scrolledMatched = 0;
            for (int k = 0; k < kSegments; ++k) {
                if (columnCompared[k] == 0)
                    continue;
                const double fits = double(columnMatched[k]) / columnCompared[k];
                if (stillCompared[k] > 0 && double(stillMatched[k]) / stillCompared[k] > fits)
                    continue;
                scrolled += columnCompared[k];
                scrolledMatched += columnMatched[k];
                if (columnCompared[k] >= kColumnContent && fits >= kColumnMatch)
                    moving += columnCompared[k];
            }
            // Most of the view matches (also where a banner rotates or a
            // header turned sticky), or most of the content moves with this
            // shift beside columns that stayed put (a sticky sidebar).
            const double score = double(dMatched) / dCompared;
            const double proof = pCompared >= kMinDistinctive ? double(pMatched) / pCompared : 0.0;
            // For standing out, only the columns that scrolled count: a pane
            // that stays put (a folder tree beside a list) matches at no shift.
            if (scrolled >= kMinDistinctive)
                scores.emplace_back(shift, double(scrolledMatched) / scrolled);
            // Judged on all the content, so the shift most of it follows wins.
            // Ties keep the smaller shift: periodic content repeats at larger ones.
            if (double(matched) / compared >= kMinMatch && score >= kMinMatch) {
                if (score > bestInformativeScore + 1e-9) {
                    bestInformativeScore = score;
                    bestInformative = shift;
                }
            } else if (moving >= kMinDistinctive && double(moving) / scrolled >= kMinMoving
                       && proof > bestMovingScore + 1e-9) {
                bestMovingScore = proof;
                bestMoving = shift;
            }
        } else if (double(matched) / compared >= kMinMatch && !tracking
                   && (bestBlank == 0 || std::abs(shift - m_lastShift) < std::abs(bestBlank - m_lastShift))) {
            bestBlank = shift;
        }
    }
    if (bestInformative != 0)
        return bestInformative;
    if (bestMoving != 0)
        return bestMoving;
    if (!scores.empty()) {
        // Sidebars half stuck, animations and the like spoil every column
        // here and there; the true shift still stands out from all others.
        const auto peak = std::max_element(scores.begin(), scores.end(),
                                           [](const auto &a, const auto &b) { return a.second < b.second; });
        double others = 0.0;
        for (const auto &[shift, score] : scores) {
            if (std::abs(shift - peak->first) > kPeakWidth)
                others = std::max(others, score);
        }
        if (peak->second >= kMinPeak && peak->second >= kPeakMargin * others)
            return peak->first;
    }
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
