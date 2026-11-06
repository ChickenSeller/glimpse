#pragma once

#include <QHash>
#include <QRect>
#include <QString>

class QPainter;

// The keyboard a recording shows: its physical arrangement and what its keys
// are labeled under the layout in use.
struct KeyboardLayout {
    // ANSI: US style. ISO: European, with a tall Enter and a key left of Z.
    // JIS: Japanese, with ¥, ろ and the conversion keys around the space bar.
    enum class Physical { Ansi, Iso, Jis };
    Physical physical = Physical::Ansi;
    // Character keys, by key id (see KeyboardOverlay): what they type.
    // Keys missing here keep their US labels.
    QHash<int, QString> labels;
    quintptr id = 0; // the platform's layout handle, to notice a change
};

// An on-video full-size keyboard (function row, main block, editing block,
// arrows and numeric keypad)
// whose held keys light up and fade out shortly after release. Keys are
// identified by where they are, not what they type: their scan code, with
// 0xE000 added for extended ones (0xE11D for Pause), so a key lights up in
// its place under any layout.
class KeyboardOverlay
{
public:
    // Its size in key units, margins included.
    static constexpr double kUnitsWide = 23.6;
    static constexpr double kUnitsHigh = 7.1;

    void setLayout(const KeyboardLayout &layout);
    void setKey(int key, bool pressed, qint64 nowMs);
    // The size the keyboard takes for a key unit of `unit` pixels.
    static QSize sizeFor(qreal unit);
    // Draws the keyboard into `area` (it keeps its aspect ratio, centered at
    // the bottom of `area`).
    void paint(QPainter &painter, const QRectF &area, qint64 nowMs) const;

private:
    struct State {
        bool pressed = false;
        qint64 releasedAt = -1;
    };
    QHash<int, State> m_keys;
    KeyboardLayout m_layout;
};
