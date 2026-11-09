#pragma once

#include <QImage>
#include <QString>
#include <QStringList>

class QWidget;

// Save / copy shared by the result window and the annotation editor.
namespace ImageActions {

// Whether any pixel is not fully opaque (e.g. outside a freehand outline).
bool hasTransparency(const QImage &image);

// Asks for a file name and saves `image`. Returns the saved path, or an empty
// string if the user canceled or saving failed (a failure has been reported).
// `suggested` is the file offered (by default a new one in the screenshot
// folder).
QString saveAs(QWidget *parent, const QImage &image, const QString &suggested = QString());

// Whether `image` can be written to `path` as it is: its format is one Qt
// writes, and keeps transparency if the image has any.
bool canWrite(const QImage &image, const QString &path);

// Writes `image` to `path`, in the format its suffix names. On failure says
// so (with `title`) and returns false.
bool write(QWidget *parent, const QImage &image, const QString &path, const QString &title);

void copyToClipboard(const QImage &image);

// Asks for image files to open (in the folder last opened from).
QStringList chooseImages(QWidget *parent);

// Whether `path` names a file that looks like an image Qt can read.
bool isImageFile(const QString &path);

// Reads an image file, turned as its EXIF orientation says. On failure says
// so and returns a null image.
QImage load(QWidget *parent, const QString &path);

} // namespace ImageActions
