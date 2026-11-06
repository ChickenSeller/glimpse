#include "ModelStore.h"

#include "app/SettingsStore.h"

#include <QClipboard>
#include <QCoreApplication>
#include <QCryptographicHash>
#include <QDialog>
#include <QDir>
#include <QEventLoop>
#include <QFile>
#include <QFileDialog>
#include <QGuiApplication>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QMessageBox>
#include <QPushButton>
#include <QTreeWidget>
#include <QVBoxLayout>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QProgressDialog>
#include <QSaveFile>
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

// Reads `path` through `each` in chunks, with a progress dialog for large
// files. Empty on success, else the error; "canceled" sets `*canceled`.
template<typename Each>
QString readChunks(const QString &path, QWidget *parent, const QString &label, bool *canceled, Each each)
{
    *canceled = false;
    QFile in(path);
    if (!in.open(QIODevice::ReadOnly))
        return in.errorString();
    QProgressDialog progress(label, QCoreApplication::translate("Ocr", "Cancel"), 0, int(std::max<qint64>(1, in.size() / (1 << 20))), parent);
    progress.setWindowModality(Qt::ApplicationModal);
    progress.setMinimumDuration(500);
    progress.setAutoClose(false);
    qint64 done = 0;
    while (!in.atEnd()) {
        const QByteArray chunk = in.read(4 << 20);
        if (chunk.isEmpty() && in.error() != QFile::NoError)
            return in.errorString();
        if (const QString error = each(chunk); !error.isEmpty())
            return error;
        done += chunk.size();
        progress.setValue(int(done / (1 << 20)));
        if (progress.wasCanceled()) {
            *canceled = true;
            return QCoreApplication::translate("Ocr", "Canceled.");
        }
    }
    return {};
}

QString sha256Of(const QString &path, QWidget *parent, bool *canceled)
{
    QCryptographicHash hash(QCryptographicHash::Sha256);
    const QString error = readChunks(path, parent, QCoreApplication::translate("Ocr", "Checking %1...").arg(QFileInfo(path).fileName()), canceled,
                                     [&hash](const QByteArray &chunk) {
                                         hash.addData(chunk);
                                         return QString();
                                     });
    return error.isEmpty() ? QString::fromLatin1(hash.result().toHex()) : QString();
}

// The names a file of `file` may have: where it goes, and as downloaded.
bool nameFits(const QString &name, const ModelFile &file)
{
    return name.compare(QFileInfo(file.path).fileName(), Qt::CaseInsensitive) == 0
           || name.compare(QFileInfo(file.url.path()).fileName(), Qt::CaseInsensitive) == 0;
}

// The files still needed, each to download or to choose from disk.
class ObtainDialog : public QDialog
{
public:
    ObtainDialog(const QList<ModelFile> &files, const QString &title, const QString &message, QWidget *parent)
        : QDialog(parent)
        , m_files(files)
        , m_ready(files.size(), false)
    {
        setWindowTitle(title);
        auto *layout = new QVBoxLayout(this);
        auto *text = new QLabel(message);
        text->setWordWrap(true);
        layout->addWidget(text);

        m_list = new QTreeWidget;
        m_list->setRootIsDecorated(false);
        m_list->setSelectionMode(QAbstractItemView::NoSelection);
        m_list->setHeaderLabels({QCoreApplication::translate("Ocr", "File"), QCoreApplication::translate("Ocr", "Status"), QString()});
        for (qsizetype i = 0; i < m_files.size(); ++i) {
            auto *item = new QTreeWidgetItem(m_list, {m_files.at(i).name});
            item->setToolTip(0, m_files.at(i).url.toString());
            auto *choose = new QPushButton(QCoreApplication::translate("Ocr", "Choose..."));
            choose->setToolTip(QCoreApplication::translate("Ocr", "Use a copy of this file already on this computer"));
            connect(choose, &QPushButton::clicked, this, [this, i] { chooseFor(i); });
            m_list->setItemWidget(item, 2, choose);
        }
        m_list->header()->setSectionResizeMode(0, QHeaderView::Stretch);
        m_list->header()->setSectionResizeMode(1, QHeaderView::ResizeToContents);
        m_list->header()->setSectionResizeMode(2, QHeaderView::ResizeToContents);
        m_list->header()->setStretchLastSection(false);
        layout->addWidget(m_list);

        auto *hint = new QLabel(QCoreApplication::translate("Ocr", "Downloading slowly? Copy the links, download the files in a browser or a "
                                   "download manager, then choose them here. Downloading can also be canceled "
                                   "halfway to come back here."));
        hint->setWordWrap(true);
        layout->addWidget(hint);

        auto *buttons = new QHBoxLayout;
        auto *copy = new QPushButton(QCoreApplication::translate("Ocr", "Copy &Links"));
        connect(copy, &QPushButton::clicked, this, &ObtainDialog::copyLinks);
        auto *chooseAll = new QPushButton(QCoreApplication::translate("Ocr", "C&hoose Files..."));
        chooseAll->setToolTip(QCoreApplication::translate("Ocr", "Choose several files at once; each goes to the row its name belongs to"));
        connect(chooseAll, &QPushButton::clicked, this, &ObtainDialog::chooseFiles);
        m_download = new QPushButton(QCoreApplication::translate("Ocr", "&Download"));
        m_download->setDefault(true);
        connect(m_download, &QPushButton::clicked, this, &ObtainDialog::downloadRest);
        auto *cancel = new QPushButton(QCoreApplication::translate("Ocr", "Cancel"));
        connect(cancel, &QPushButton::clicked, this, &QDialog::reject);
        buttons->addWidget(copy);
        buttons->addWidget(chooseAll);
        buttons->addStretch();
        buttons->addWidget(m_download);
        buttons->addWidget(cancel);
        layout->addLayout(buttons);

        updateRows();
        resize(std::max(560, sizeHint().width()), sizeHint().height());
    }

private:
    QList<ModelFile> rest() const
    {
        QList<ModelFile> files;
        for (qsizetype i = 0; i < m_files.size(); ++i) {
            if (!m_ready.at(i))
                files << m_files.at(i);
        }
        return files;
    }

    void updateRows()
    {
        for (qsizetype i = 0; i < m_files.size(); ++i) {
            QTreeWidgetItem *item = m_list->topLevelItem(int(i));
            item->setText(1, m_ready.at(i) ? QCoreApplication::translate("Ocr", "Ready") : QCoreApplication::translate("Ocr", "Missing"));
            m_list->itemWidget(item, 2)->setEnabled(!m_ready.at(i));
        }
        if (rest().isEmpty())
            accept();
    }

    void copyLinks()
    {
        QStringList links;
        for (const ModelFile &file : rest())
            links << sources(file.url).constFirst().toString();
        QGuiApplication::clipboard()->setText(links.join(QLatin1Char('\n')) + QLatin1Char('\n'));
    }

    // Copies `source` in for row `i`; false (and says why) if it does not fit.
    bool take(qsizetype i, const QString &source)
    {
        const QString error = install(source, m_files.at(i), this);
        if (!error.isEmpty()) {
            QMessageBox::warning(this, windowTitle(),
                                 QCoreApplication::translate("Ocr", "Could not use %1 for %2:\n%3")
                                     .arg(QDir::toNativeSeparators(source), m_files.at(i).name, error));
            return false;
        }
        m_ready[i] = true;
        return true;
    }

    void chooseFor(qsizetype i)
    {
        const QString source = QFileDialog::getOpenFileName(this, QCoreApplication::translate("Ocr", "Choose %1").arg(m_files.at(i).name), m_folder);
        if (source.isEmpty())
            return;
        m_folder = QFileInfo(source).absolutePath();
        take(i, source);
        updateRows();
    }

    void chooseFiles()
    {
        const QStringList sources = QFileDialog::getOpenFileNames(this, QCoreApplication::translate("Ocr", "Choose Files"), m_folder);
        if (sources.isEmpty())
            return;
        m_folder = QFileInfo(sources.constFirst()).absolutePath();
        QStringList unmatched;
        for (const QString &source : sources) {
            const QString name = QFileInfo(source).fileName();
            QList<qsizetype> fits;
            for (qsizetype i = 0; i < m_files.size(); ++i) {
                if (!m_ready.at(i) && nameFits(name, m_files.at(i)))
                    fits << i;
            }
            if (fits.size() > 1) {
                // The same name in several rows (two language directions):
                // the checksum tells them apart where it is known.
                bool canceled = false;
                const QString sha = sha256Of(source, this, &canceled);
                if (canceled)
                    return;
                QList<qsizetype> same;
                for (qsizetype i : std::as_const(fits)) {
                    if (!m_files.at(i).sha256.isEmpty()
                        && sha.compare(QString::fromLatin1(m_files.at(i).sha256), Qt::CaseInsensitive) == 0)
                        same << i;
                }
                fits = same;
            }
            if (fits.size() != 1)
                unmatched << name;
            else
                take(fits.constFirst(), source);
        }
        updateRows();
        if (!unmatched.isEmpty() && !rest().isEmpty()) {
            QMessageBox::information(this, windowTitle(),
                                     QCoreApplication::translate("Ocr", "Could not tell which file these are; choose them on their own rows:\n\n%1")
                                         .arg(unmatched.join(QLatin1Char('\n'))));
        }
    }

    void downloadRest()
    {
        // On failure or Cancel the dialog stays, for choosing files instead.
        const QList<ModelFile> files = rest();
        if (download(files, this)) {
            m_ready.fill(true);
            updateRows();
        }
    }

    QList<ModelFile> m_files;
    QList<bool> m_ready;
    QTreeWidget *m_list = nullptr;
    QPushButton *m_download = nullptr;
    QString m_folder;
};

} // namespace

bool obtain(const QList<ModelFile> &files, const QString &title, const QString &message, QWidget *parent)
{
    if (files.isEmpty())
        return true;
    ObtainDialog dialog(files, title, message, parent);
    return dialog.exec() == QDialog::Accepted;
}

QString install(const QString &source, const ModelFile &file, QWidget *parent)
{
    const QFileInfo from(source);
    if (from.canonicalFilePath() == QFileInfo(file.path).canonicalFilePath()) {
        // Already in place (chosen from where it goes): only check it.
        bool canceled = false;
        const QString sha = sha256Of(source, parent, &canceled);
        if (sha.isEmpty())
            return QCoreApplication::translate("Ocr", "Could not read the file.");
        if (!file.sha256.isEmpty() && sha.compare(QString::fromLatin1(file.sha256), Qt::CaseInsensitive) != 0)
            return QCoreApplication::translate("Ocr", "The file is not the expected one (checksum mismatch).");
        return {};
    }
    QDir().mkpath(QFileInfo(file.path).absolutePath());
    QSaveFile out(file.path);
    if (!out.open(QIODevice::WriteOnly))
        return out.errorString();
    QCryptographicHash hash(QCryptographicHash::Sha256);
    bool canceled = false;
    QString error = readChunks(source, parent, QCoreApplication::translate("Ocr", "Copying %1...").arg(file.name), &canceled,
                               [&out, &hash](const QByteArray &chunk) {
                                   hash.addData(chunk);
                                   return out.write(chunk) == chunk.size() ? QString() : out.errorString();
                               });
    if (error.isEmpty() && !file.sha256.isEmpty() && hash.result().toHex() != file.sha256.toLower())
        error = QCoreApplication::translate("Ocr", "The file is not the expected one (checksum mismatch).");
    if (!error.isEmpty()) {
        out.cancelWriting();
        return error;
    }
    return out.commit() ? QString() : out.errorString();
}

QString directory(const QString &engine)
{
    const QString path = QDir(QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation))
                             .filePath(QStringLiteral("ocr/") + engine);
    QDir().mkpath(path);
    return path;
}

SourceOrder sourceOrder()
{
    const int order = SettingsStore::open().value(kOrderKey, int(SourceOrder::MirrorFirst)).toInt();
    return order >= 0 && order <= int(SourceOrder::OfficialOnly) ? SourceOrder(order) : SourceOrder::MirrorFirst;
}

void setSourceOrder(SourceOrder order)
{
    SettingsStore::open().setValue(kOrderKey, int(order));
}

QString defaultMirror()
{
    return QStringLiteral(GLIMPSE_DOWNLOAD_MIRROR);
}

QString mirror()
{
    return SettingsStore::open().value(kMirrorKey, defaultMirror()).toString().trimmed();
}

void setMirror(const QString &base)
{
    // Only a deviation is stored, so the default can move with new versions.
    QSettings settings = SettingsStore::open();
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
