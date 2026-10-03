#pragma once

#include <QColor>
#include <QHash>
#include <QIcon>

class QAction;

// Recolors a monochrome icon (e.g. a black Material Symbols SVG) to `color`,
// keeping it scalable. Disabled mode is drawn translucent.
QIcon tintedIcon(const QIcon &source, const QColor &color);

// Remembers actions' original icons (as set in a .ui) and recolors them; call
// apply() again when the palette changes.
class ActionIconTinter
{
public:
    void add(QAction *action);
    void apply(const QColor &color) const;

private:
    QHash<QAction *, QIcon> m_icons;
};
