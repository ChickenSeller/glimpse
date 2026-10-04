#include "ModelStore.h"

#include <QCoreApplication>
#include <QCryptographicHash>
#include <QDir>
#include <QEventLoop>
#include <QFile>
#include <QMessageBox>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QProgressDialog>
#include <QSaveFile>
#include <QSettings>
#include <QStandardPaths>

namespace ModelStore {

namespace {

const QString kOrderKey = QStringLiteral("downloads/order");
const QString kMirrorKey = QStringLiteral("downloads/mirror");
// No answer from a source for this long moves on to the next one; a slow
// but steady download may take as long as it needs.
constexpr int kStallTimeoutMs = 30000;

// One attempt: `url` into `out`, from its start, checked against `sha256`
// when known. Empty on success, else the error.
QString fetch(QNetworkAccessManager &network, const QUrl &url, const QByteArray &sha256, QSaveFile &out,
              QProgressDialog &progress)
{
    out.seek(0);
    out.resize(0);
    QCryptographicHash hash(QCryptographicHash::Sha256);
    QNetworkRequest request(url);
    request.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::NoLessSafeRedirectPolicy);
    request.setTransferTimeout(kStallTimeoutMs);
    QNetworkReply *reply = network.get(request);
    const auto take = [reply, &out, &hash] {
        const QByteArray data = reply->readAll();
        out.write(data);
        hash.addData(data);
    };
    QObject::connect(reply, &QNetworkReply::readyRead, reply, take);
    QObject::connect(reply, &QNetworkReply::downloadProgress, &progress, [&progress](qint64 received, qint64 total) {
        if (total > 0) {
            progress.setRange(0, int(total / 1024));
            progress.setValue(int(received / 1024));
        }
    });
    QObject::connect(&progress, &QProgressDialog::canceled, reply, &QNetworkReply::abort);

    QEventLoop loop;
    QObject::connect(reply, &QNetworkReply::finished, &loop, &QEventLoop::quit);
    loop.exec();

    take();
    QString error = reply->error() == QNetworkReply::NoError ? QString() : reply->errorString();
    reply->deleteLater();
    if (error.isEmpty() && !sha256.isEmpty() && hash.result().toHex() != sha256.toLower())
        error = QCoreApplication::translate("Ocr", "The file is not the expected one (checksum mismatch).");
    return error;
}

} // namespace

QString directory(const QString &engine)
{
    const QString path = QDir(QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation))
                             .filePath(QStringLiteral("ocr/") + engine);
    QDir().mkpath(path);
    return path;
}

SourceOrder sourceOrder()
{
    const int order = QSettings().value(kOrderKey, int(SourceOrder::MirrorFirst)).toInt();
    return order >= 0 && order <= int(SourceOrder::OfficialOnly) ? SourceOrder(order) : SourceOrder::MirrorFirst;
}

void setSourceOrder(SourceOrder order)
{
    QSettings().setValue(kOrderKey, int(order));
}

QString defaultMirror()
{
    return QStringLiteral(GLIMPSE_DOWNLOAD_MIRROR);
}

QString mirror()
{
    return QSettings().value(kMirrorKey, defaultMirror()).toString().trimmed();
}

void setMirror(const QString &base)
{
    // Only a deviation is stored, so the default can move with new versions.
    QSettings settings;
    const QString value = base.trimmed();
    if (value.isEmpty() || value == defaultMirror())
        settings.remove(kMirrorKey);
    else
        settings.setValue(kMirrorKey, value);
}

QUrl mirrorUrl(const QUrl &official, const QString &base)
{
    QString root = base.trimmed();
    while (root.endsWith(QLatin1Char('/')))
        root.chop(1);
    return QUrl(root + QLatin1Char('/') + official.host() + official.path(QUrl::FullyEncoded), QUrl::StrictMode);
}

QList<QUrl> sources(const QUrl &official)
{
    const QString base = mirror();
    const QUrl mirrored = base.isEmpty() ? QUrl() : mirrorUrl(official, base);
    if (!mirrored.isValid())
        return {official};
    switch (sourceOrder()) {
    case SourceOrder::MirrorFirst: return {mirrored, official};
    case SourceOrder::OfficialFirst: return {official, mirrored};
    case SourceOrder::OfficialOnly: break;
    }
    return {official};
}

bool download(const QList<ModelFile> &files, QWidget *parent)
{
    const QString title = QCoreApplication::translate("Ocr", "Downloading Models");
    QNetworkAccessManager network;
    QProgressDialog progress(parent);
    progress.setWindowTitle(title);
    progress.setWindowModality(Qt::ApplicationModal);
    progress.setMinimumDuration(0);
    progress.setAutoClose(false);
    progress.setAutoReset(false);

    for (qsizetype i = 0; i < files.size(); ++i) {
        const ModelFile &file = files.at(i);
        QDir().mkpath(QFileInfo(file.path).absolutePath());
        // QSaveFile only replaces the target once everything was written.
        QSaveFile out(file.path);
        if (!out.open(QIODevice::WriteOnly)) {
            QMessageBox::warning(parent, title,
                                 QCoreApplication::translate("Ocr", "Cannot write %1:\n%2")
                                     .arg(QDir::toNativeSeparators(file.path), out.errorString()));
            return false;
        }

        // The next source when one fails (unreachable, missing the file...).
        QStringList errors;
        bool done = false;
        const QList<QUrl> urls = sources(file.url);
        for (const QUrl &url : urls) {
            progress.setLabelText(QCoreApplication::translate("Ocr", "Downloading %1 (%2 of %3) from %4...")
                                      .arg(file.name)
                                      .arg(i + 1)
                                      .arg(files.size())
                                      .arg(url.host()));
            progress.setRange(0, 0);
            progress.setValue(0);
            const QString error = fetch(network, url, file.sha256, out, progress);
            if (progress.wasCanceled()) {
                out.cancelWriting();
                return false;
            }
            if (error.isEmpty()) {
                done = true;
                break;
            }
            errors << QStringLiteral("%1: %2").arg(url.host(), error);
        }

        if (!done || !out.commit()) {
            QMessageBox::warning(parent, title,
                                 QCoreApplication::translate("Ocr", "Could not download %1:\n%2")
                                     .arg(file.name, done ? out.errorString() : errors.join(QLatin1Char('\n'))));
            return false;
        }
    }
    return true;
}

} // namespace ModelStore
