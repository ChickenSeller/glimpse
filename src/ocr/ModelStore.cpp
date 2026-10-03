#include "ModelStore.h"

#include <QCoreApplication>
#include <QDir>
#include <QEventLoop>
#include <QFile>
#include <QMessageBox>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QProgressDialog>
#include <QSaveFile>
#include <QStandardPaths>

namespace ModelStore {

QString directory(const QString &engine)
{
    const QString path = QDir(QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation))
                             .filePath(QStringLiteral("ocr/") + engine);
    QDir().mkpath(path);
    return path;
}

bool download(const QList<ModelFile> &files, QWidget *parent)
{
    const QString title = QCoreApplication::translate("Ocr", "Downloading Recognition Models");
    QNetworkAccessManager network;
    QProgressDialog progress(parent);
    progress.setWindowTitle(title);
    progress.setWindowModality(Qt::ApplicationModal);
    progress.setMinimumDuration(0);
    progress.setAutoClose(false);
    progress.setAutoReset(false);

    for (qsizetype i = 0; i < files.size(); ++i) {
        const ModelFile &file = files.at(i);
        progress.setLabelText(QCoreApplication::translate("Ocr", "Downloading %1 (%2 of %3)...")
                                  .arg(file.name)
                                  .arg(i + 1)
                                  .arg(files.size()));
        progress.setRange(0, 0);
        progress.setValue(0);

        QDir().mkpath(QFileInfo(file.path).absolutePath());
        // QSaveFile only replaces the target once everything was written.
        QSaveFile out(file.path);
        if (!out.open(QIODevice::WriteOnly)) {
            QMessageBox::warning(parent, title,
                                 QCoreApplication::translate("Ocr", "Cannot write %1:\n%2")
                                     .arg(QDir::toNativeSeparators(file.path), out.errorString()));
            return false;
        }

        QNetworkRequest request(file.url);
        request.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::NoLessSafeRedirectPolicy);
        QNetworkReply *reply = network.get(request);
        QObject::connect(reply, &QNetworkReply::readyRead, reply, [reply, &out] { out.write(reply->readAll()); });
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

        out.write(reply->readAll());
        const bool canceled = progress.wasCanceled();
        const bool ok = reply->error() == QNetworkReply::NoError;
        const QString errorText = reply->errorString();
        reply->deleteLater();

        if (canceled) {
            out.cancelWriting();
            return false;
        }
        if (!ok || !out.commit()) {
            QMessageBox::warning(parent, title,
                                 QCoreApplication::translate("Ocr", "Could not download %1:\n%2")
                                     .arg(file.name, ok ? out.errorString() : errorText));
            return false;
        }
    }
    return true;
}

} // namespace ModelStore
