#pragma once

#include <QColor>
#include <QIcon>

// Recolors a monochrome icon (e.g. a black Material Symbols SVG) to `color`,
// keeping it scalable. Disabled mode is drawn translucent.
QIcon tintedIcon(const QIcon &source, const QColor &color);
