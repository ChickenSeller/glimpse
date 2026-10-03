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

bool hasTransparency(const QImage &image)
{
    if (!image.hasAlphaChannel())
        return false;
    const QImage argb = image.convertToFormat(QImage::Format_ARGB32);
    for (int y = 0; y < argb.height(); ++y) {
        const auto *line = reinterpret_cast<const QRgb *>(argb.constScanLine(y));
        for (int x = 0; x < argb.width(); ++x) {
            if (qAlpha(line[x]) < 255)
                return true;
        }
    }
    return false;
}

QString saveAs(QWidget *parent, const QImage &image)
{
    const QString dir = QStandardPaths::writableLocation(QStandardPaths::PicturesLocation);
    const QString name = QStringLiteral("Glimpse_%1.png")
                             .arg(QDateTime::currentDateTime().toString(QStringLiteral("yyyyMMdd_HHmmss")));
    // JPEG and BMP cannot keep transparency, so such images are PNG only.
    const bool pngOnly = hasTransparency(image);
    const QString filter = pngOnly
        ? QCoreApplication::translate("EditorWindow", "PNG Image (*.png)")
        : QCoreApplication::translate("EditorWindow", "PNG Image (*.png);;JPEG Image (*.jpg *.jpeg);;BMP Image (*.bmp)");
    QString path = QFileDialog::getSaveFileName(
        parent, QCoreApplication::translate("EditorWindow", "Save Capture"), QDir(dir).filePath(name), filter);
    if (path.isEmpty())
        return {};
    const QFileInfo info(path);
    if (info.suffix().isEmpty())
        path += QStringLiteral(".png");
    else if (pngOnly && info.suffix().compare(QLatin1String("png"), Qt::CaseInsensitive) != 0)
        path = info.dir().filePath(info.completeBaseName() + QStringLiteral(".png"));

    if (!image.save(path, pngOnly ? "PNG" : nullptr)) {
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
