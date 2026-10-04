#include "KeyboardOverlay.h"

#include <QFont>
#include <QPainter>
#include <QPainterPath>

#include <algorithm>
#include <iterator>

namespace {

// Windows virtual-key codes (kept here so this file needs no windows.h).
enum : int {
    VkBack = 0x08, VkTab = 0x09, VkReturn = 0x0D, VkPause = 0x13, VkCapital = 0x14, VkEscape = 0x1B,
    VkSpace = 0x20, VkPrior = 0x21, VkNext = 0x22, VkEnd = 0x23, VkHome = 0x24,
    VkLeft = 0x25, VkUp = 0x26, VkRight = 0x27, VkDown = 0x28, VkSnapshot = 0x2C, VkInsert = 0x2D, VkDelete = 0x2E,
    VkLWin = 0x5B, VkRWin = 0x5C, VkApps = 0x5D, VkF1 = 0x70, VkScroll = 0x91,
    VkLShift = 0xA0, VkRShift = 0xA1, VkLControl = 0xA2, VkRControl = 0xA3, VkLMenu = 0xA4, VkRMenu = 0xA5,
    VkOem1 = 0xBA, VkOemPlus = 0xBB, VkOemComma = 0xBC, VkOemMinus = 0xBD, VkOemPeriod = 0xBE, VkOem2 = 0xBF,
    VkOem3 = 0xC0, VkOem4 = 0xDB, VkOem5 = 0xDC, VkOem6 = 0xDD, VkOem7 = 0xDE,
};

struct KeyCap {
    const char *label;
    int vk;
    qreal x, y, w; // in key units; every key is one unit high
};

constexpr qreal kWidth = 18.5;
constexpr qreal kHeight = 6.5;
constexpr qint64 kFadeMs = 300;

// A US layout, the most widely recognized one.
const KeyCap kKeys[] = {
    // Function row
    {"Esc", VkEscape, 0, 0, 1},
    {"F1", VkF1 + 0, 2, 0, 1}, {"F2", VkF1 + 1, 3, 0, 1}, {"F3", VkF1 + 2, 4, 0, 1}, {"F4", VkF1 + 3, 5, 0, 1},
    {"F5", VkF1 + 4, 6.5, 0, 1}, {"F6", VkF1 + 5, 7.5, 0, 1}, {"F7", VkF1 + 6, 8.5, 0, 1}, {"F8", VkF1 + 7, 9.5, 0, 1},
    {"F9", VkF1 + 8, 11, 0, 1}, {"F10", VkF1 + 9, 12, 0, 1}, {"F11", VkF1 + 10, 13, 0, 1}, {"F12", VkF1 + 11, 14, 0, 1},
    {"PrtSc", VkSnapshot, 15.5, 0, 1}, {"ScrLk", VkScroll, 16.5, 0, 1}, {"Pause", VkPause, 17.5, 0, 1},
    // Number row
    {"`", VkOem3, 0, 1.5, 1},
    {"1", '1', 1, 1.5, 1}, {"2", '2', 2, 1.5, 1}, {"3", '3', 3, 1.5, 1}, {"4", '4', 4, 1.5, 1}, {"5", '5', 5, 1.5, 1},
    {"6", '6', 6, 1.5, 1}, {"7", '7', 7, 1.5, 1}, {"8", '8', 8, 1.5, 1}, {"9", '9', 9, 1.5, 1}, {"0", '0', 10, 1.5, 1},
    {"-", VkOemMinus, 11, 1.5, 1}, {"=", VkOemPlus, 12, 1.5, 1}, {"Backspace", VkBack, 13, 1.5, 2},
    {"Ins", VkInsert, 15.5, 1.5, 1}, {"Home", VkHome, 16.5, 1.5, 1}, {"PgUp", VkPrior, 17.5, 1.5, 1},
    // Top letter row
    {"Tab", VkTab, 0, 2.5, 1.5},
    {"Q", 'Q', 1.5, 2.5, 1}, {"W", 'W', 2.5, 2.5, 1}, {"E", 'E', 3.5, 2.5, 1}, {"R", 'R', 4.5, 2.5, 1},
    {"T", 'T', 5.5, 2.5, 1}, {"Y", 'Y', 6.5, 2.5, 1}, {"U", 'U', 7.5, 2.5, 1}, {"I", 'I', 8.5, 2.5, 1},
    {"O", 'O', 9.5, 2.5, 1}, {"P", 'P', 10.5, 2.5, 1}, {"[", VkOem4, 11.5, 2.5, 1}, {"]", VkOem6, 12.5, 2.5, 1},
    {"\\", VkOem5, 13.5, 2.5, 1.5},
    {"Del", VkDelete, 15.5, 2.5, 1}, {"End", VkEnd, 16.5, 2.5, 1}, {"PgDn", VkNext, 17.5, 2.5, 1},
    // Home row
    {"Caps", VkCapital, 0, 3.5, 1.75},
    {"A", 'A', 1.75, 3.5, 1}, {"S", 'S', 2.75, 3.5, 1}, {"D", 'D', 3.75, 3.5, 1}, {"F", 'F', 4.75, 3.5, 1},
    {"G", 'G', 5.75, 3.5, 1}, {"H", 'H', 6.75, 3.5, 1}, {"J", 'J', 7.75, 3.5, 1}, {"K", 'K', 8.75, 3.5, 1},
    {"L", 'L', 9.75, 3.5, 1}, {";", VkOem1, 10.75, 3.5, 1}, {"'", VkOem7, 11.75, 3.5, 1},
    {"Enter", VkReturn, 12.75, 3.5, 2.25},
    // Bottom letter row
    {"Shift", VkLShift, 0, 4.5, 2.25},
    {"Z", 'Z', 2.25, 4.5, 1}, {"X", 'X', 3.25, 4.5, 1}, {"C", 'C', 4.25, 4.5, 1}, {"V", 'V', 5.25, 4.5, 1},
    {"B", 'B', 6.25, 4.5, 1}, {"N", 'N', 7.25, 4.5, 1}, {"M", 'M', 8.25, 4.5, 1}, {",", VkOemComma, 9.25, 4.5, 1},
    {".", VkOemPeriod, 10.25, 4.5, 1}, {"/", VkOem2, 11.25, 4.5, 1}, {"Shift", VkRShift, 12.25, 4.5, 2.75},
    {"↑", VkUp, 16.5, 4.5, 1},
    // Space row
    {"Ctrl", VkLControl, 0, 5.5, 1.25}, {"Win", VkLWin, 1.25, 5.5, 1.25}, {"Alt", VkLMenu, 2.5, 5.5, 1.25},
    {"", VkSpace, 3.75, 5.5, 6.25},
    {"Alt", VkRMenu, 10, 5.5, 1.25}, {"Win", VkRWin, 11.25, 5.5, 1.25}, {"Menu", VkApps, 12.5, 5.5, 1.25},
    {"Ctrl", VkRControl, 13.75, 5.5, 1.25},
    {"←", VkLeft, 15.5, 5.5, 1}, {"↓", VkDown, 16.5, 5.5, 1}, {"→", VkRight, 17.5, 5.5, 1},
};

const QColor kAccent(0x2d, 0x9c, 0xff);

} // namespace

void KeyboardOverlay::setKey(int virtualKey, bool pressed, qint64 nowMs)
{
    State &state = m_keys[virtualKey];
    if (pressed == state.pressed)
        return; // auto-repeat
    state.pressed = pressed;
    state.releasedAt = pressed ? -1 : nowMs;
}

QSize KeyboardOverlay::sizeFor(qreal unit)
{
    const qreal margin = unit * 0.3;
    return QSizeF(kWidth * unit + 2 * margin, kHeight * unit + 2 * margin).toSize();
}

void KeyboardOverlay::paint(QPainter &p, const QRectF &area, qint64 nowMs) const
{
    // The largest unit that fits the area.
    const qreal unit = std::min(area.width() / (kWidth + 0.6), area.height() / (kHeight + 0.6));
    if (unit < 4)
        return;
    const QSizeF size = sizeFor(unit);
    const QRectF board(area.left() + (area.width() - size.width()) / 2, area.bottom() - size.height(),
                       size.width(), size.height());
    const qreal margin = unit * 0.3;

    p.save();
    p.setRenderHint(QPainter::Antialiasing);
    p.setPen(Qt::NoPen);
    p.setBrush(QColor(0, 0, 0, 170));
    p.drawRoundedRect(board, unit * 0.3, unit * 0.3);

    QFont font = p.font();
    font.setPixelSize(std::max(6, qRound(unit * 0.3)));
    QFont bold = font;
    bold.setBold(true);
    const qreal gap = unit * 0.08;

    for (const KeyCap &key : kKeys) {
        const QRectF cap(board.left() + margin + key.x * unit + gap, board.top() + margin + key.y * unit + gap,
                         key.w * unit - 2 * gap, unit - 2 * gap);
        // How lit the key is: 1 while held, fading to 0 after release.
        qreal lit = 0;
        const auto it = m_keys.constFind(key.vk);
        if (it != m_keys.constEnd()) {
            if (it->pressed)
                lit = 1;
            else if (it->releasedAt >= 0 && nowMs - it->releasedAt < kFadeMs)
                lit = 1.0 - qreal(nowMs - it->releasedAt) / kFadeMs;
        }

        QColor fill = kAccent;
        fill.setAlphaF(0.12 + 0.78 * lit);
        if (lit == 0)
            fill = QColor(255, 255, 255, 30);
        p.setBrush(fill);
        p.setPen(QPen(QColor(255, 255, 255, lit > 0 ? 220 : 70), std::max(1.0, unit * 0.03)));
        p.drawRoundedRect(cap, unit * 0.12, unit * 0.12);

        p.setFont(lit > 0 ? bold : font);
        p.setPen(QColor(255, 255, 255, lit > 0 ? 255 : 200));
        p.drawText(cap, Qt::AlignCenter, QString::fromUtf8(key.label));
    }
    p.restore();
}
