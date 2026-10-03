#pragma once

#include <QImage>

#include <vector>

// Builds one tall image from successive same-sized frames of a scrolling view.
//
// Each new frame is compared with the previous one: rows that stay put at the
// top and bottom (sticky headers, footers, toolbars) are detected once and
// kept only once; in between, the vertical shift is found by matching row
// hashes, and only the newly revealed rows are appended.
class ScrollStitcher
{
public:
    enum class Result {
        Added,     // new rows were appended
        NoChange,  // identical to the previous frame: the end was reached
        NoOverlap, // scrolled too far to line up; the frame was ignored
    };

    Result addFrame(const QImage &frame);

    bool isEmpty() const { return m_frames == 0; }
    // Height of the stitched image so far, in pixels.
    int height() const;
    // The shift (pixels) found for the last added frame.
    int lastShift() const { return m_lastShift; }
    // Rows that actually scroll (frame height minus header and footer).
    int contentHeight() const;

    QImage image() const;

private:
    // Each row is compared in kSegments horizontal segments, so a change in
    // part of a row (a rotating banner, an animation) only costs that part.
    struct Signature {
        std::vector<size_t> hashes; // row * kSegments + segment
        std::vector<char> content;  // segment is not a single solid colour
        bool operator==(const Signature &other) const { return hashes == other.hashes; }
    };
    static constexpr int kSegments = 8;

    static Signature signature(const QImage &image);
    bool rowsEqual(const Signature &a, int rowA, const Signature &b, int rowB) const;
    // skipTop / skipBottom: rows at the edges of the scrolling part that the
    // new frame shows unchanged in place (an element that turned sticky).
    int findShift(const Signature &next, int skipTop, int skipBottom) const;
    void appendRows(const QImage &source, int first, int count);

    int m_frames = 0;
    QImage m_previous;
    Signature m_previousSignature;
    int m_header = -1; // static rows at the top, -1 until known
    int m_footer = -1; // static rows at the bottom
    int m_lastShift = 0;

    QImage m_body; // grows by doubling; rows [0, m_bodyHeight) are valid
    int m_bodyHeight = 0;
    QImage m_footerRows;
};
