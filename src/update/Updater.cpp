#include "Updater.h"

#include <QApplication>
#include <QCryptographicHash>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QMessageBox>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QProcess>
#include <QProgressDialog>
#include <QPushButton>
#include <QSaveFile>
#include <QSettings>
#include <QStandardPaths>
#include <QTemporaryFile>
#include <QTimer>

namespace {

const QString kModeKey = QStringLiteral("updates/mode");
constexpr int kIdleRetryMs = 10000;

QString updateDirectory()
{
    const QString path = QDir(QStandardPaths::writableLocation(QStandardPaths::TempLocation)).filePath(QStringLiteral("glimpse-update"));
    QDir().mkpath(path);
    return path;
}

// Whether the folder of glimpse.exe can be written to (not e.g. Program Files).
bool appFolderWritable()
{
    QTemporaryFile probe(QDir(QCoreApplication::applicationDirPath()).filePath(QStringLiteral("update-check-XXXXXX")));
    return probe.open();
}

} // namespace

Updater::Updater(QObject *parent)
    : QObject(parent)
    , m_network(new QNetworkAccessManager(this))
{
}

Updater::~Updater() = default;

bool Updater::isSupported()
{
#ifdef Q_OS_WIN
    return true;
#else
    return false;
#endif
}

Updater::Mode Updater::mode()
{
    const int value = QSettings().value(kModeKey, int(Mode::Ask)).toInt();
    return value >= 0 && value <= int(Mode::Never) ? Mode(value) : Mode::Ask;
}

void Updater::setMode(Mode mode)
{
    QSettings().setValue(kModeKey, int(mode));
}

void Updater::setCanRestart(std::function<bool()> canRestart)
{
    m_canRestart = std::move(canRestart);
}

void Updater::checkAtStartup()
{
    if (!isSupported() || mode() == Mode::Never)
        return;
    requestManifest(false);
}

void Updater::checkNow()
{
    if (isSupported())
        requestManifest(true);
}

void Updater::requestManifest(bool interactive)
{
    QNetworkRequest request{QUrl(QStringLiteral(GLIMPSE_UPDATE_URL))};
    request.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::NoLessSafeRedirectPolicy);
    request.setAttribute(QNetworkRequest::CacheLoadControlAttribute, QNetworkRequest::AlwaysNetwork);
    request.setTransferTimeout(30000);
    QNetworkReply *reply = m_network->get(request);
    connect(reply, &QNetworkReply::finished, this, [this, reply, interactive] { onManifest(reply, interactive); });
}

bool Updater::parse(const QJsonObject &object, const QUrl &base, Release *release)
{
    release->version = QVersionNumber::fromString(object.value(QStringLiteral("version")).toString());
    release->url = base.resolved(QUrl(object.value(QStringLiteral("url")).toString()));
    release->sha256 = object.value(QStringLiteral("sha256")).toString().toLatin1().toLower();
    release->size = object.value(QStringLiteral("size")).toInteger();
    release->notes = object.value(QStringLiteral("notes")).toString();
    return !release->version.isNull() && release->url.isValid() && !release->sha256.isEmpty();
}

void Updater::onManifest(QNetworkReply *reply, bool interactive)
{
    reply->deleteLater();
    // At start, being offline or a page without update information is not
    // worth a word; asked for, it is.
    if (reply->error() != QNetworkReply::NoError) {
        if (interactive)
            QMessageBox::warning(nullptr, tr("Glimpse Update"),
                                 tr("Could not check for updates:\n%1").arg(reply->errorString()));
        return;
    }
    const QJsonObject manifest = QJsonDocument::fromJson(reply->readAll()).object();
    const QVersionNumber current = QVersionNumber::fromString(QStringLiteral(GLIMPSE_VERSION));

    Release latest;
    Release required;
    const bool haveLatest = parse(manifest.value(QStringLiteral("latest")).toObject(), reply->url(), &latest)
                            && latest.version > current;
    const bool haveRequired = parse(manifest.value(QStringLiteral("required")).toObject(), reply->url(), &required)
                              && required.version > current;

    if (interactive) {
        if (haveLatest)
            ask(latest);
        else
            QMessageBox::information(nullptr, tr("Glimpse Update"),
                                     tr("Glimpse %1 is the latest version.").arg(QStringLiteral(GLIMPSE_VERSION)));
        return;
    }

    switch (mode()) {
    case Mode::Automatic:
        if (haveLatest)
            download(latest, false);
        break;
    case Mode::RequiredOnly:
        if (haveRequired)
            download(required, false);
        break;
    case Mode::Ask:
        if (haveLatest)
            ask(latest);
        break;
    case Mode::Never:
        break;
    }
}

void Updater::ask(const Release &release)
{
    QMessageBox box;
    box.setIcon(QMessageBox::Information);
    box.setWindowTitle(tr("Glimpse Update"));
    box.setText(tr("Glimpse %1 is available; you have %2.\nUpdate now? Glimpse restarts when it is done.")
                    .arg(release.version.toString(), QStringLiteral(GLIMPSE_VERSION)));
    box.setDetailedText(release.notes);
    QPushButton *update = box.addButton(tr("Update"), QMessageBox::AcceptRole);
    box.addButton(tr("Later"), QMessageBox::RejectRole);
    box.setDefaultButton(update);
    box.setWindowFlag(Qt::WindowStaysOnTopHint);
    box.exec();
    if (box.clickedButton() == update)
        download(release, true);
}

void Updater::download(const Release &release, bool interactive)
{
    if (m_file)
        return; // one at a time
    if (!appFolderWritable()) {
        const QString message = tr("Glimpse %1 is available, but the folder Glimpse runs from cannot be written to. "
                                   "Download it from the homepage and unpack it yourself.")
                                    .arg(release.version.toString());
        if (interactive)
            QMessageBox::warning(nullptr, tr("Glimpse Update"), message);
        else
            emit notify(tr("Glimpse Update"), message);
        return;
    }

    const QString path = QDir(updateDirectory()).filePath(release.url.fileName());
    m_file = new QSaveFile(path, this);
    if (!m_file->open(QIODevice::WriteOnly)) {
        delete m_file;
        m_file = nullptr;
        return;
    }

    QNetworkRequest request(release.url);
    request.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::NoLessSafeRedirectPolicy);
    request.setTransferTimeout(60000);
    QNetworkReply *reply = m_network->get(request);
    connect(reply, &QNetworkReply::readyRead, this, [this, reply] { m_file->write(reply->readAll()); });
    connect(reply, &QNetworkReply::finished, this, [this, release, reply, interactive] {
        onDownloaded(release, reply, interactive);
    });

    if (interactive) {
        m_progress = new QProgressDialog(tr("Downloading Glimpse %1...").arg(release.version.toString()), tr("Cancel"), 0,
                                         0);
        m_progress->setWindowTitle(tr("Glimpse Update"));
        m_progress->setAttribute(Qt::WA_DeleteOnClose);
        m_progress->setMinimumDuration(0);
        connect(reply, &QNetworkReply::downloadProgress, m_progress, [this](qint64 received, qint64 total) {
            if (total > 0) {
                m_progress->setMaximum(int(total / 1024));
                m_progress->setValue(int(received / 1024));
            }
        });
        connect(m_progress, &QProgressDialog::canceled, reply, &QNetworkReply::abort);
        m_progress->show();
    } else {
        emit notify(tr("Glimpse Update"),
                    tr("Downloading Glimpse %1; Glimpse restarts once it is installed.").arg(release.version.toString()));
    }
}

void Updater::onDownloaded(const Release &release, QNetworkReply *reply, bool interactive)
{
    reply->deleteLater();
    m_file->write(reply->readAll());
    if (m_progress)
        m_progress->close();

    QString error;
    if (reply->error() == QNetworkReply::OperationCanceledError) {
        m_file->cancelWriting();
    } else if (reply->error() != QNetworkReply::NoError) {
        error = reply->errorString();
        m_file->cancelWriting();
    } else if (!m_file->commit()) {
        error = m_file->errorString();
    } else {
        // Only the package published with the release is installed.
        QFile check(m_file->fileName());
        QCryptographicHash hash(QCryptographicHash::Sha256);
        if (!check.open(QIODevice::ReadOnly) || !hash.addData(&check) || hash.result().toHex() != release.sha256)
            error = tr("The download is damaged (checksum mismatch).");
        check.close();
    }
    const QString zipPath = m_file->fileName();
    delete m_file;
    m_file = nullptr;

    if (!error.isEmpty()) {
        QFile::remove(zipPath);
        const QString message = tr("Could not download Glimpse %1:\n%2").arg(release.version.toString(), error);
        if (interactive)
            QMessageBox::warning(nullptr, tr("Glimpse Update"), message);
        else
            emit notify(tr("Glimpse Update"), message);
        return;
    }
    if (reply->error() == QNetworkReply::OperationCanceledError)
        return;

    if (interactive) {
        if (!install(zipPath, &error))
            QMessageBox::warning(nullptr, tr("Glimpse Update"), error);
    } else {
        installWhenIdle(zipPath);
    }
}

void Updater::installWhenIdle(const QString &zipPath)
{
    // Not in the middle of a capture or a recording.
    if (m_canRestart && !m_canRestart()) {
        QTimer::singleShot(kIdleRetryMs, this, [this, zipPath] { installWhenIdle(zipPath); });
        return;
    }
    QString error;
    if (!install(zipPath, &error))
        emit notify(tr("Glimpse Update"), error);
}

bool Updater::install(const QString &zipPath, QString *error)
{
    // The script is copied out of the resources: PowerShell needs a file.
    const QString script = QDir(updateDirectory()).filePath(QStringLiteral("update.ps1"));
    QFile::remove(script);
    if (!QFile::copy(QStringLiteral(":/update.ps1"), script)) {
        *error = tr("Could not prepare the update.");
        return false;
    }
    QFile::setPermissions(script, QFile::ReadOwner | QFile::WriteOwner);

    const QStringList arguments = {
        QStringLiteral("-NoProfile"), QStringLiteral("-ExecutionPolicy"), QStringLiteral("Bypass"),
        QStringLiteral("-WindowStyle"), QStringLiteral("Hidden"), QStringLiteral("-File"), QDir::toNativeSeparators(script),
        QStringLiteral("-Zip"), QDir::toNativeSeparators(zipPath),
        QStringLiteral("-Target"), QDir::toNativeSeparators(QCoreApplication::applicationDirPath()),
        QStringLiteral("-ProcessId"), QString::number(QCoreApplication::applicationPid()),
        QStringLiteral("-Exe"), QDir::toNativeSeparators(QCoreApplication::applicationFilePath()),
    };
    // By its full path: PATH does not always include PowerShell's folder.
    const QString windows = qEnvironmentVariable("SystemRoot", QStringLiteral("C:\\Windows"));
    QString powershell = QDir(windows).filePath(QStringLiteral("System32/WindowsPowerShell/v1.0/powershell.exe"));
    if (!QFileInfo::exists(powershell))
        powershell = QStringLiteral("powershell.exe"); // let Windows look for it after all
    QProcess process;
    process.setProgram(QDir::toNativeSeparators(powershell));
    process.setArguments(arguments);
    if (!process.startDetached()) {
        *error = tr("Could not start the update.") + QLatin1Char('\n') + process.errorString();
        return false;
    }
    // The script waits for Glimpse to exit, replaces its files and starts it again.
    QTimer::singleShot(0, qApp, &QCoreApplication::quit);
    return true;
}
