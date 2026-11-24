#pragma once

#include <QImage>

#include <vector>

// Builds one tall image from successive same-sized frames of a scrolling view.
//
// Each new frame is compared with the previous one: rows that stay put at the
// top and bottom (sticky headers, footers, toolbars) are detected once and
// kept only once; in between, the vertical shift is found by matching row
// hashes, and only the newly revealed rows are appended.
//
// Frames scrolled down by a program line up with the previous frame added.
// When the user scrolls (Tracking), frames may also go back up: each is lined
// up with the frame before it in either direction, which keeps track of where
// the view is, and rows are appended once it passes the lowest point so far.
// The last rows appended can be taken back (undoLast()); Tracking then picks
// up again from there once the view is back within reach of it.
//
// Horizontally, the same goes for columns, from left to right: each frame is
// transposed (columns become rows) on its way in, and the result on its way
// out; "rows", "height", "down" and "header" below then mean columns, width,
// right and the still columns at the left.
class ScrollStitcher
{
public:
    enum class Scrolling { Down, Tracking };
    enum class Direction { Vertical, Horizontal };
    enum class Result {
        Added,     // new rows were appended
        Moved,     // Tracking: lined up, but showed nothing new
        NoChange,  // identical to the previous frame: the end was reached
        NoOverlap, // scrolled too far to line up; the frame was ignored
        Gap,       // Tracking: lined up, but the rows right after the stitched
                   // part are out of view (after undoLast()); scroll back up
    };

    explicit ScrollStitcher(Scrolling scrolling = Scrolling::Down, Direction direction = Direction::Vertical)
        : m_scrolling(scrolling)
        , m_horizontal(direction == Direction::Horizontal)
    {
    }

    Result addFrame(const QImage &frame);

    // From program scrolling (Down) to the user's (Tracking), mid-capture.
    void setScrolling(Scrolling scrolling) { m_scrolling = scrolling; }
    Scrolling scrolling() const { return m_scrolling; }
    // Takes back the rows the last added frame appended; false if none are
    // left to take back (the first frame stays).
    bool undoLast();
    bool canUndo() const { return !m_segments.empty(); }

    bool isEmpty() const { return m_frames == 0; }
    // Height of the stitched image so far, in pixels.
    int height() const;
    // The shift (pixels) found for the last added frame.
    int lastShift() const { return m_lastShift; }
    // Rows that actually scroll (frame height minus header and footer).
    int contentHeight() const;

    QImage image() const;
    // The stitched image so far, scaled down to fit `maxSize`, at a cost that
    // depends on `maxSize` only.
    QImage preview(const QSize &maxSize) const;

private:
    // Each row is compared in kSegments horizontal segments, so a change in
    // part of a row (a rotating banner, an animation) only costs that part,
    // and a narrow pane that scrolls beside still ones has segments of its own.
    struct Signature {
        std::vector<size_t> hashes; // row * kSegments + segment
        std::vector<char> content;  // segment is not a single solid colour
        bool operator==(const Signature &other) const { return hashes == other.hashes; }
    };
    static constexpr int kSegments = 32;

    // Columns as rows (and back again), keeping the device pixel ratio.
    static QImage transposed(const QImage &image);
    static Signature signature(const QImage &image);
    bool rowsEqual(const Signature &a, int rowA, const Signature &b, int rowB) const;
    // The same row in two frames, but for a little that animates (a progress
    // bar, a caret): still a row that did not scroll.
    bool rowStill(const Signature &next, int row) const;
    // Whether `next` shows the view where the previous frame did, give or
    // take what animates in it.
    bool unmoved(const Signature &next) const;
    // Share of the previous frame's content segments that `next` has `shift`
    // rows up (down for a negative shift).
    double contentMatch(const Signature &next, int shift) const;
    // skipTop / skipBottom: rows at the edges of the scrolling part that the
    // new frame shows unchanged in place (an element that turned sticky).
    // Negative when Tracking found the view scrolled up; 0 if none lines up.
    int findShift(const Signature &next, int skipTop, int skipBottom) const;
    void appendRows(const QImage &source, int first, int count);
    QImage footerImage() const;
    // preview() as the rows are kept (transposed, horizontally).
    QImage previewRows(const QSize &maxSize) const;
    // Row `y` of image(), without building it.
    const uchar *rowAt(int y, const QImage &footer) const;

    Scrolling m_scrolling;
    bool m_horizontal;
    int m_frames = 0;
    QImage m_previous;
    Signature m_previousSignature;
    int m_header = -1; // static rows at the top, -1 until known
    int m_footer = -1; // static rows at the bottom
    int m_lastShift = 0;
    int m_position = 0;    // Tracking: how far the previous frame is scrolled
    int m_maxPosition = 0; // Tracking: the furthest it has been

    QImage m_body; // grows by doubling; rows [0, m_bodyHeight) are valid
    int m_bodyHeight = 0;
    QImage m_footerRows;
    std::vector<int> m_segments; // rows appended by each frame after the first
};
