#pragma once

#include <QImage>
#include <QString>

class QWidget;

// Save / copy shared by the result window and the annotation editor.
namespace ImageActions {

// Whether any pixel is not fully opaque (e.g. outside a freehand outline).
bool hasTransparency(const QImage &image);

// Asks for a file name and saves `image`. Returns the saved path, or an empty
// string if the user canceled or saving failed (a failure has been reported).
QString saveAs(QWidget *parent, const QImage &image);

void copyToClipboard(const QImage &image);

} // namespace ImageActions
