#include "KeyboardOverlay.h"

#include <QFont>
#include <QList>
#include <QPainter>
#include <QPainterPath>

#include <algorithm>

namespace {

struct KeyCap {
    const char *label; // fixed, or for character keys the US one, used when the layout gives none
    int id;            // scan code; 0xE000 + scan code for extended keys
    qreal x, y, w;     // in key units; every key is one unit high
    bool character = false; // labeled by the layout in use
    bool unlabeled = false; // the lower part of a tall Enter
};

constexpr qreal kWidth = 18.5;
constexpr qreal kHeight = 6.5;
constexpr qint64 kFadeMs = 300;

KeyCap chr(const char *label, int id, qreal x, qreal y, qreal w = 1)
{
    return {label, id, x, y, w, true};
}

KeyCap fixed(const char *label, int id, qreal x, qreal y, qreal w = 1)
{
    return {label, id, x, y, w};
}

// Function row, editing block and arrows: the same on every keyboard.
void addCommon(QList<KeyCap> &keys)
{
    keys << fixed("Esc", 0x01, 0, 0);
    const char *f[] = {"F1", "F2", "F3", "F4", "F5", "F6", "F7", "F8", "F9", "F10", "F11", "F12"};
    const int fScan[] = {0x3B, 0x3C, 0x3D, 0x3E, 0x3F, 0x40, 0x41, 0x42, 0x43, 0x44, 0x57, 0x58};
    const qreal fX[] = {2, 3, 4, 5, 6.5, 7.5, 8.5, 9.5, 11, 12, 13, 14};
    for (int i = 0; i < 12; ++i)
        keys << fixed(f[i], fScan[i], fX[i], 0);
    keys << fixed("PrtSc", 0xE037, 15.5, 0) << fixed("ScrLk", 0x46, 16.5, 0) << fixed("Pause", 0xE11D, 17.5, 0);
    keys << fixed("Ins", 0xE052, 15.5, 1.5) << fixed("Home", 0xE047, 16.5, 1.5) << fixed("PgUp", 0xE049, 17.5, 1.5);
    keys << fixed("Del", 0xE053, 15.5, 2.5) << fixed("End", 0xE04F, 16.5, 2.5) << fixed("PgDn", 0xE051, 17.5, 2.5);
    keys << fixed("↑", 0xE048, 16.5, 4.5);
    keys << fixed("←", 0xE04B, 15.5, 5.5) << fixed("↓", 0xE050, 16.5, 5.5) << fixed("→", 0xE04D, 17.5, 5.5);
}

// Number row from 1 to =, letters and the punctuation among them: the keys
// all three arrangements share, at the same places.
void addShared(QList<KeyCap> &keys)
{
    const char *digits[] = {"1", "2", "3", "4", "5", "6", "7", "8", "9", "0", "-", "="};
    for (int i = 0; i < 12; ++i)
        keys << chr(digits[i], 0x02 + i, 1 + i, 1.5);
    const char *top[] = {"Q", "W", "E", "R", "T", "Y", "U", "I", "O", "P", "[", "]"};
    for (int i = 0; i < 12; ++i)
        keys << chr(top[i], 0x10 + i, 1.5 + i, 2.5);
    const char *home[] = {"A", "S", "D", "F", "G", "H", "J", "K", "L", ";", "'"};
    for (int i = 0; i < 11; ++i)
        keys << chr(home[i], 0x1E + i, 1.75 + i, 3.5);
    const char *bottom[] = {"Z", "X", "C", "V", "B", "N", "M", ",", ".", "/"};
    for (int i = 0; i < 10; ++i)
        keys << chr(bottom[i], 0x2C + i, 2.25 + i, 4.5);
    keys << fixed("Tab", 0x0F, 0, 2.5, 1.5) << fixed("Caps", 0x3A, 0, 3.5, 1.75);
}

QList<KeyCap> ansiKeys()
{
    QList<KeyCap> keys;
    addCommon(keys);
    addShared(keys);
    keys << chr("`", 0x29, 0, 1.5) << fixed("Backspace", 0x0E, 13, 1.5, 2);
    keys << chr("\\", 0x2B, 13.5, 2.5, 1.5);
    keys << fixed("Enter", 0x1C, 12.75, 3.5, 2.25);
    keys << fixed("Shift", 0x2A, 0, 4.5, 2.25) << fixed("Shift", 0x36, 12.25, 4.5, 2.75);
    keys << fixed("Ctrl", 0x1D, 0, 5.5, 1.25) << fixed("Win", 0xE05B, 1.25, 5.5, 1.25) << fixed("Alt", 0x38, 2.5, 5.5, 1.25)
         << fixed("", 0x39, 3.75, 5.5, 6.25) << fixed("Alt", 0xE038, 10, 5.5, 1.25) << fixed("Win", 0xE05C, 11.25, 5.5, 1.25)
         << fixed("Menu", 0xE05D, 12.5, 5.5, 1.25) << fixed("Ctrl", 0xE01D, 13.75, 5.5, 1.25);
    return keys;
}

QList<KeyCap> isoKeys()
{
    QList<KeyCap> keys;
    addCommon(keys);
    addShared(keys);
    keys << chr("`", 0x29, 0, 1.5) << fixed("Backspace", 0x0E, 13, 1.5, 2);
    // The tall Enter: its upper part beside ], its lower one beside the key
    // that takes \'s place.
    keys << fixed("Enter", 0x1C, 13.5, 2.5, 1.5) << KeyCap{"", 0x1C, 13.75, 3.5, 1.25, false, true};
    keys << chr("#", 0x2B, 12.75, 3.5);
    keys << fixed("Shift", 0x2A, 0, 4.5, 1.25) << chr("\\", 0x56, 1.25, 4.5) << fixed("Shift", 0x36, 12.25, 4.5, 2.75);
    keys << fixed("Ctrl", 0x1D, 0, 5.5, 1.25) << fixed("Win", 0xE05B, 1.25, 5.5, 1.25) << fixed("Alt", 0x38, 2.5, 5.5, 1.25)
         << fixed("", 0x39, 3.75, 5.5, 6.25) << fixed("AltGr", 0xE038, 10, 5.5, 1.25) << fixed("Win", 0xE05C, 11.25, 5.5, 1.25)
         << fixed("Menu", 0xE05D, 12.5, 5.5, 1.25) << fixed("Ctrl", 0xE01D, 13.75, 5.5, 1.25);
    return keys;
}

QList<KeyCap> jisKeys()
{
    QList<KeyCap> keys;
    addCommon(keys);
    addShared(keys);
    keys << fixed("半/全", 0x29, 0, 1.5) << fixed("¥", 0x7D, 13, 1.5) << fixed("Back", 0x0E, 14, 1.5);
    keys << fixed("Enter", 0x1C, 13.5, 2.5, 1.5) << KeyCap{"", 0x1C, 13.75, 3.5, 1.25, false, true};
    keys << chr("]", 0x2B, 12.75, 3.5);
    keys << fixed("Shift", 0x2A, 0, 4.5, 2.25) << chr("\\", 0x73, 12.25, 4.5) << fixed("Shift", 0x36, 13.25, 4.5, 1.75);
    keys << fixed("Ctrl", 0x1D, 0, 5.5, 1.25) << fixed("Win", 0xE05B, 1.25, 5.5, 1.25) << fixed("Alt", 0x38, 2.5, 5.5, 1.25)
         << fixed("無変換", 0x7B, 3.75, 5.5, 1.25) << fixed("", 0x39, 5, 5.5, 3.5) << fixed("変換", 0x79, 8.5, 5.5, 1.25)
         << fixed("かな", 0x70, 9.75, 5.5, 1.25) << fixed("Alt", 0xE038, 11, 5.5) << fixed("Win", 0xE05C, 12, 5.5)
         << fixed("Menu", 0xE05D, 13, 5.5) << fixed("Ctrl", 0xE01D, 14, 5.5);
    return keys;
}

const QList<KeyCap> &keysFor(KeyboardLayout::Physical physical)
{
    static const QList<KeyCap> ansi = ansiKeys();
    static const QList<KeyCap> iso = isoKeys();
    static const QList<KeyCap> jis = jisKeys();
    switch (physical) {
    case KeyboardLayout::Physical::Iso: return iso;
    case KeyboardLayout::Physical::Jis: return jis;
    default: return ansi;
    }
}

const QColor kAccent(0x2d, 0x9c, 0xff);

} // namespace

void KeyboardOverlay::setLayout(const KeyboardLayout &layout)
{
    m_layout = layout;
}

void KeyboardOverlay::setKey(int key, bool pressed, qint64 nowMs)
{
    State &state = m_keys[key];
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

    for (const KeyCap &key : keysFor(m_layout.physical)) {
        const QRectF cap(board.left() + margin + key.x * unit + gap, board.top() + margin + key.y * unit + gap,
                         key.w * unit - 2 * gap, unit - 2 * gap);
        // How lit the key is: 1 while held, fading to 0 after release.
        qreal lit = 0;
        const auto it = m_keys.constFind(key.id);
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

        if (key.unlabeled)
            continue;
        QString label = QString::fromUtf8(key.label);
        if (key.character) {
            const auto own = m_layout.labels.constFind(key.id);
            if (own != m_layout.labels.constEnd() && !own->isEmpty())
                label = *own;
        }
        p.setFont(lit > 0 ? bold : font);
        p.setPen(QColor(255, 255, 255, lit > 0 ? 255 : 200));
        p.drawText(cap, Qt::AlignCenter, label);
    }
    p.restore();
}
