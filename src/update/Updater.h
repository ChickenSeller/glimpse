#pragma once

#include <QJsonObject>
#include <QObject>
#include <QPointer>
#include <QUrl>
#include <QVersionNumber>

#include <functional>

class QNetworkAccessManager;
class QNetworkReply;
class QProgressDialog;
class QSaveFile;

// Keeps Glimpse up to date from the homepage's update.json (which the web
// server reads live from the GitLab releases, see deploy/nginx/):
//
//   {"latest":   {"version", "url", "sha256", "size", "notes"} or null,
//    "required": the same, or null}
//
// "required" is the version every older Glimpse must move to (set on the
// web server). What happens depends on Settings > General:
// install the latest automatically, install a required version
// automatically, ask about the latest, or never check.
//
// Installing replaces the files of the folder glimpse.exe runs from (the
// unpacked Windows package) once Glimpse has quit, then starts the new
// version: resources/update.ps1 does that. Windows only.
class Updater : public QObject
{
    Q_OBJECT

public:
    enum class Mode {
        Automatic,    // install the latest version without asking
        RequiredOnly, // install a required version without asking, nothing else
        Ask,          // offer the latest version
        Never,
    };

    explicit Updater(QObject *parent = nullptr);
    ~Updater() override;

    static bool isSupported();
    static Mode mode();
    static void setMode(Mode mode);

    // Whether Glimpse may quit now for an automatic install (no capture or
    // recording in progress); asked again later while it may not.
    void setCanRestart(std::function<bool()> canRestart);

    // Checks once, as the settings say.
    void checkAtStartup();

signals:
    // For the tray: an automatic update started or failed.
    void notify(const QString &title, const QString &message);

private:
    struct Release {
        QVersionNumber version;
        QUrl url;
        QByteArray sha256;
        qint64 size = 0;
        QString notes;
    };

    void onManifest(QNetworkReply *reply);
    static bool parse(const QJsonObject &object, const QUrl &base, Release *release);
    void ask(const Release &release);
    void download(const Release &release, bool interactive);
    void onDownloaded(const Release &release, QNetworkReply *reply, bool interactive);
    void installWhenIdle(const QString &zipPath);
    bool install(const QString &zipPath, QString *error);

    QNetworkAccessManager *m_network;
    std::function<bool()> m_canRestart;
    QSaveFile *m_file = nullptr;
    QPointer<QProgressDialog> m_progress;
};
