#include "ImageActions.h"

#include <QClipboard>
#include <QCoreApplication>
#include <QDateTime>
#include <QDir>
#include <QFileDialog>
#include <QFileInfo>
#include <QGuiApplication>
#include <QMessageBox>
#include <QStandardPaths>

// The strings keep the "EditorWindow" context they had before this file
// existed, so their translations still apply.
namespace ImageActions {

QString saveAs(QWidget *parent, const QImage &image)
{
    const QString dir = QStandardPaths::writableLocation(QStandardPaths::PicturesLocation);
    const QString name = QStringLiteral("Glimpse_%1.png")
                             .arg(QDateTime::currentDateTime().toString(QStringLiteral("yyyyMMdd_HHmmss")));
    QString path = QFileDialog::getSaveFileName(
        parent, QCoreApplication::translate("EditorWindow", "Save Capture"), QDir(dir).filePath(name),
        QCoreApplication::translate("EditorWindow", "PNG Image (*.png);;JPEG Image (*.jpg *.jpeg);;BMP Image (*.bmp)"));
    if (path.isEmpty())
        return {};
    if (QFileInfo(path).suffix().isEmpty())
        path += QStringLiteral(".png");

    if (!image.save(path)) {
        QMessageBox::warning(parent, QCoreApplication::translate("EditorWindow", "Save Capture"),
                             QCoreApplication::translate("EditorWindow", "Could not save the image to\n%1")
                                 .arg(QDir::toNativeSeparators(path)));
        return {};
    }
    return path;
}

void copyToClipboard(const QImage &image)
{
    QGuiApplication::clipboard()->setImage(image);
}

} // namespace ImageActions
