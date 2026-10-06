#include "UsageStats.h"

#include <QCoreApplication>
#include <QGuiApplication>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLocale>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QRandomGenerator>
#include <QScreen>
#include <QSettings>
#include <QSysInfo>
#include <QUrl>
#include <QUrlQuery>
#include <QUuid>

namespace {

const QString kEnabledKey = QStringLiteral("usage/enabled");
const QString kIdKey = QStringLiteral("usage/installId");
// Kept even with statistics off, so that turning them on later does not
// report an update that happened long ago.
const QString kLastVersionKey = QStringLiteral("usage/lastVersion");

QString installId()
{
    QSettings settings;
    QString id = settings.value(kIdKey).toString();
    if (id.size() != 32) {
        id = QUuid::createUuid().toString(QUuid::Id128);
        settings.setValue(kIdKey, id);
    }
    return id;
}

// Matomo reads the operating system from the user agent.
QString userAgent()
{
#if defined(Q_OS_WIN)
    const QString system = QStringLiteral("Windows NT %1; Win64; x64").arg(QSysInfo::kernelVersion().section(QLatin1Char('.'), 0, 1));
#elif defined(Q_OS_LINUX)
    const QString system = QStringLiteral("X11; Linux %1").arg(QSysInfo::currentCpuArchitecture());
#else
    const QString system = QSysInfo::prettyProductName();
#endif
    return QStringLiteral("Glimpse/%1 (%2)").arg(QStringLiteral(GLIMPSE_VERSION), system);
}

QString trackingQuery(const QString &id, const QString &category, const QString &action, const QString &name)
{
    QUrlQuery query;
    query.addQueryItem(QStringLiteral("idsite"), QStringLiteral(GLIMPSE_MATOMO_SITE_ID));
    query.addQueryItem(QStringLiteral("rec"), QStringLiteral("1"));
    query.addQueryItem(QStringLiteral("apiv"), QStringLiteral("1"));
    query.addQueryItem(QStringLiteral("send_image"), QStringLiteral("0"));
    query.addQueryItem(QStringLiteral("rand"), QString::number(QRandomGenerator::global()->generate()));
    query.addQueryItem(QStringLiteral("_id"), id.left(16));
    query.addQueryItem(QStringLiteral("uid"), id);
    query.addQueryItem(QStringLiteral("url"),
                       QStringLiteral(GLIMPSE_HOMEPAGE_URL "app/%1").arg(QStringLiteral(GLIMPSE_VERSION)));
    query.addQueryItem(QStringLiteral("ua"), userAgent());
    query.addQueryItem(QStringLiteral("lang"), QLocale::system().name().replace(QLatin1Char('_'), QLatin1Char('-')));
    if (const QScreen *screen = QGuiApplication::primaryScreen()) {
        const QSize size = screen->size() * screen->devicePixelRatio();
        query.addQueryItem(QStringLiteral("res"), QStringLiteral("%1x%2").arg(size.width()).arg(size.height()));
    }
    query.addQueryItem(QStringLiteral("e_c"), category);
    query.addQueryItem(QStringLiteral("e_a"), action);
    if (!name.isEmpty())
        query.addQueryItem(QStringLiteral("e_n"), name);
    return QLatin1Char('?') + query.toString(QUrl::FullyEncoded);
}

// One request for all of them (Matomo's bulk tracking); failures are ignored.
void send(const QList<QStringList> &events)
{
    if (events.isEmpty() || !UsageStats::isAvailable() || !UsageStats::isEnabled())
        return;
    static QNetworkAccessManager *network = new QNetworkAccessManager(qApp);
    const QString id = installId();
    QJsonArray requests;
    for (const QStringList &event : events)
        requests.append(trackingQuery(id, event.value(0), event.value(1), event.value(2)));

    QNetworkRequest request{QUrl(QStringLiteral(GLIMPSE_MATOMO_URL))};
    request.setHeader(QNetworkRequest::ContentTypeHeader, QStringLiteral("application/json"));
    request.setHeader(QNetworkRequest::UserAgentHeader, userAgent());
    request.setTransferTimeout(30000);
    QNetworkReply *reply =
        network->post(request, QJsonDocument(QJsonObject{{QStringLiteral("requests"), requests}}).toJson(QJsonDocument::Compact));
    QObject::connect(reply, &QNetworkReply::finished, reply, &QObject::deleteLater);
}

} // namespace

namespace UsageStats {

bool isAvailable()
{
    return !QStringLiteral(GLIMPSE_MATOMO_SITE_ID).isEmpty() && !QStringLiteral(GLIMPSE_MATOMO_URL).isEmpty();
}

bool isEnabled()
{
    return QSettings().value(kEnabledKey, true).toBool();
}

void setEnabled(bool enabled)
{
    QSettings settings;
    settings.setValue(kEnabledKey, enabled);
    // Turned off, the install is forgotten; turned on again, it is a new one.
    if (!enabled)
        settings.remove(kIdKey);
}

void reportStart(const QString &updateMode)
{
    const QString version = QStringLiteral(GLIMPSE_VERSION);
    QSettings settings;
    QString last = settings.value(kLastVersionKey).toString();
    // Glimpse before 0.4.0 did not keep its version, but it did keep settings.
    const bool firstRun = last.isEmpty() && settings.allKeys().isEmpty();
    if (last.isEmpty() && !firstRun)
        last = QStringLiteral("0.3");
    settings.setValue(kLastVersionKey, version);

    QList<QStringList> events;
    events.append({QStringLiteral("App"), QStringLiteral("Start"), version});
    events.append({QStringLiteral("Update"), QStringLiteral("Mode"), updateMode});
    if (firstRun)
        events.append({QStringLiteral("App"), QStringLiteral("First run"), version});
    else if (last != version)
        events.append({QStringLiteral("Update"), QStringLiteral("Updated"), QStringLiteral("%1 -> %2").arg(last, version)});
    send(events);
}

void report(const QString &category, const QString &action, const QString &name)
{
    send({{category, action, name}});
}

} // namespace UsageStats
