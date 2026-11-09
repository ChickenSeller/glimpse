#include "ImageActions.h"

#include "app/AppSettings.h"
#include "app/SettingsStore.h"

#include <QClipboard>
#include <QCoreApplication>
#include <QDateTime>
#include <QDir>
#include <QFileDialog>
#include <QFileInfo>
#include <QGuiApplication>
#include <QImageReader>
#include <QImageWriter>
#include <QMessageBox>
#include <QSaveFile>

// The strings keep the "EditorWindow" context they had before this file
// existed, so their translations still apply.
namespace ImageActions {

namespace {

const QString kOpenFolderKey = QStringLiteral("editor/openFolder");

// The format a file name asks for, e.g. "jpg"; empty without a suffix.
QByteArray formatOf(const QString &path)
{
    return QFileInfo(path).suffix().toLower().toLatin1();
}

bool keepsTransparency(const QByteArray &format)
{
    return format != "jpg" && format != "jpeg" && format != "bmp";
}

} // namespace

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

QString saveAs(QWidget *parent, const QImage &image, const QString &suggested)
{
    const QString title = QCoreApplication::translate("EditorWindow", "Save Capture");
    QString offered = suggested;
    if (offered.isEmpty()) {
        // The folder chosen in Settings > General, created if it is not there yet.
        const QString dir = AppSettings::screenshotFolder();
        QDir().mkpath(dir);
        offered = QDir(dir).filePath(QStringLiteral("Glimpse_%1.png")
                                         .arg(QDateTime::currentDateTime().toString(QStringLiteral("yyyyMMdd_HHmmss"))));
    }
    // JPEG and BMP cannot keep transparency, so such images are PNG only.
    const bool pngOnly = hasTransparency(image);
    const QString png = QCoreApplication::translate("EditorWindow", "PNG Image (*.png)");
    const QString jpeg = QCoreApplication::translate("EditorWindow", "JPEG Image (*.jpg *.jpeg)");
    const QString bmp = QCoreApplication::translate("EditorWindow", "BMP Image (*.bmp)");
    QString filter = pngOnly ? png : png + QStringLiteral(";;") + jpeg + QStringLiteral(";;") + bmp;
    QString selected = png;
    const QByteArray offeredFormat = formatOf(offered);
    if (pngOnly && offeredFormat != "png") {
        const QFileInfo info(offered);
        offered = info.dir().filePath(info.completeBaseName() + QStringLiteral(".png"));
    } else if (offeredFormat == "jpg" || offeredFormat == "jpeg") {
        selected = jpeg;
    } else if (offeredFormat == "bmp") {
        selected = bmp;
    }

    QString path = QFileDialog::getSaveFileName(parent, title, offered, filter, &selected);
    if (path.isEmpty())
        return {};
    const QFileInfo info(path);
    if (info.suffix().isEmpty())
        path += QStringLiteral(".png");
    else if (pngOnly && info.suffix().compare(QLatin1String("png"), Qt::CaseInsensitive) != 0)
        path = info.dir().filePath(info.completeBaseName() + QStringLiteral(".png"));
    return write(parent, image, path, title) ? path : QString();
}

bool canWrite(const QImage &image, const QString &path)
{
    const QByteArray format = formatOf(path);
    if (!QImageWriter::supportedImageFormats().contains(format))
        return false;
    return keepsTransparency(format) || !hasTransparency(image);
}

bool write(QWidget *parent, const QImage &image, const QString &path, const QString &title)
{
    // Written next to the file and swapped in at the end: a failed save
    // leaves the old file as it was.
    QSaveFile file(path);
    QImageWriter writer(&file, formatOf(path));
    const QByteArray format = writer.format();
    if (format == "jpg" || format == "jpeg")
        writer.setQuality(95);
    QString error;
    if (!file.open(QIODevice::WriteOnly))
        error = file.errorString();
    else if (!writer.write(image))
        error = writer.errorString();
    else if (!file.commit())
        error = file.errorString();
    if (error.isEmpty())
        return true;
    file.cancelWriting();
    QMessageBox::warning(parent, title,
                         QCoreApplication::translate("EditorWindow", "Could not save the image to\n%1")
                                 .arg(QDir::toNativeSeparators(path))
                             + QStringLiteral("\n\n") + error);
    return false;
}

void copyToClipboard(const QImage &image)
{
    QGuiApplication::clipboard()->setImage(image);
}

QStringList chooseImages(QWidget *parent)
{
    QStringList patterns;
    const QList<QByteArray> formats = QImageReader::supportedImageFormats();
    for (const QByteArray &format : formats)
        patterns << QStringLiteral("*.") + QString::fromLatin1(format);
    const QString filter = QCoreApplication::translate("ImageActions", "Images (%1)").arg(patterns.join(QLatin1Char(' ')))
                           + QStringLiteral(";;") + QCoreApplication::translate("ImageActions", "All Files (*)");

    QSettings settings = SettingsStore::open();
    QString folder = settings.value(kOpenFolderKey).toString();
    if (folder.isEmpty() || !QFileInfo(folder).isDir())
        folder = AppSettings::screenshotFolder();
    const QStringList files = QFileDialog::getOpenFileNames(
        parent, QCoreApplication::translate("ImageActions", "Open Images"), folder, filter);
    if (!files.isEmpty())
        settings.setValue(kOpenFolderKey, QFileInfo(files.constFirst()).absolutePath());
    return files;
}

bool isImageFile(const QString &path)
{
    const QFileInfo info(path);
    return info.isFile() && !QImageReader::imageFormat(path).isEmpty();
}

QImage load(QWidget *parent, const QString &path)
{
    QImageReader reader(path);
    reader.setAutoTransform(true);
    // Qt refuses images over 256 MB by default; a long scrolling capture can
    // be larger, and the file was chosen on purpose.
    QImageReader::setAllocationLimit(0);
    const QImage image = reader.read();
    if (image.isNull()) {
        QMessageBox::warning(parent, QCoreApplication::translate("ImageActions", "Open Image"),
                             QCoreApplication::translate("ImageActions", "Could not open\n%1\n\n%2")
                                 .arg(QDir::toNativeSeparators(path), reader.errorString()));
    }
    return image;
}

} // namespace ImageActions
