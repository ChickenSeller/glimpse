#include "ScreenCastCapture.h"

#include <QDBusArgument>
#include <QDBusConnection>
#include <QDBusMessage>
#include <QDBusObjectPath>
#include <QDBusPendingCallWatcher>
#include <QDBusPendingReply>
#include <QDBusReply>
#include <QDBusUnixFileDescriptor>
#include <QDBusVariant>
#include <QRandomGenerator>
#include <QSettings>

#include <pipewire/pipewire.h>
#include <spa/param/video/format-utils.h>
#include <spa/pod/builder.h>

#include <fcntl.h>
#include <functional>
#include <unistd.h>

namespace {

const QString kService = QStringLiteral("org.freedesktop.portal.Desktop");
const QString kObjectPath = QStringLiteral("/org/freedesktop/portal/desktop");
const QString kScreenCast = QStringLiteral("org.freedesktop.portal.ScreenCast");
const QString kRequestInterface = QStringLiteral("org.freedesktop.portal.Request");

constexpr uint kSourceMonitor = 1;
constexpr uint kCursorEmbedded = 2;
constexpr uint kPersistUntilRevoked = 2;
const QString kRestoreTokenKey = QStringLiteral("recording/screenCastRestoreToken");

QString newToken()
{
    return QStringLiteral("glimpse%1").arg(QRandomGenerator::global()->generate());
}

// The object path the portal derives from our unique name and a token.
QString requestPath(const QString &token)
{
    const QString sender = QDBusConnection::sessionBus().baseService().mid(1).replace(QLatin1Char('.'), QLatin1Char('_'));
    return QStringLiteral("%1/request/%2/%3").arg(kObjectPath, sender, token);
}

// A uint property of the ScreenCast portal; 0 where unknown.
uint portalProperty(const QString &name)
{
    QDBusMessage get = QDBusMessage::createMethodCall(kService, kObjectPath, QStringLiteral("org.freedesktop.DBus.Properties"),
                                                      QStringLiteral("Get"));
    get << kScreenCast << name;
    const QDBusMessage reply = QDBusConnection::sessionBus().call(get);
    if (reply.type() != QDBusMessage::ReplyMessage || reply.arguments().isEmpty())
        return 0;
    return reply.arguments().first().value<QDBusVariant>().variant().toUInt();
}

QImage::Format imageFormat(spa_video_format format)
{
    // Byte order in memory; QImage's 32-bit formats are words, little-endian here.
    switch (format) {
    case SPA_VIDEO_FORMAT_BGRx: return QImage::Format_RGB32;
    case SPA_VIDEO_FORMAT_BGRA: return QImage::Format_ARGB32;
    case SPA_VIDEO_FORMAT_RGBx: return QImage::Format_RGBX8888;
    case SPA_VIDEO_FORMAT_RGBA: return QImage::Format_RGBA8888;
    default: return QImage::Format_Invalid;
    }
}

} // namespace

// Receives one Request's Response signal (QtDBus connects signals to slots
// by name only).
class PortalRequest : public QObject
{
    Q_OBJECT

public:
    PortalRequest(const QString &path, std::function<void(uint, const QVariantMap &)> onResponse, QObject *parent)
        : QObject(parent)
        , m_onResponse(std::move(onResponse))
    {
        watch(path);
    }

    ~PortalRequest() override { unwatch(); }

    void watch(const QString &path)
    {
        unwatch();
        m_path = path;
        QDBusConnection::sessionBus().connect(kService, m_path, kRequestInterface, QStringLiteral("Response"), this,
                                              SLOT(onResponse(uint,QVariantMap)));
    }

    QString path() const { return m_path; }

private slots:
    void onResponse(uint response, const QVariantMap &results)
    {
        unwatch();
        m_onResponse(response, results);
        deleteLater();
    }

private:
    void unwatch()
    {
        if (m_path.isEmpty())
            return;
        QDBusConnection::sessionBus().disconnect(kService, m_path, kRequestInterface, QStringLiteral("Response"), this,
                                                 SLOT(onResponse(uint,QVariantMap)));
        m_path.clear();
    }

    std::function<void(uint, const QVariantMap &)> m_onResponse;
    QString m_path;
};

ScreenCastCapture::ScreenCastCapture(QObject *parent)
    : QObject(parent)
{
    pw_init(nullptr, nullptr);
}

ScreenCastCapture::~ScreenCastCapture()
{
    stop();
}

void ScreenCastCapture::start()
{
    m_stopped = false;
    if (!QDBusConnection::sessionBus().isConnected()) {
        emit failed(tr("The session D-Bus is not available."));
        return;
    }
    request(QStringLiteral("CreateSession"), {}, {{QStringLiteral("session_handle_token"), newToken()}},
            &ScreenCastCapture::onSessionCreated);
}

void ScreenCastCapture::request(const QString &method, const QList<QVariant> &arguments, QVariantMap options,
                                void (ScreenCastCapture::*onSuccess)(const QVariantMap &results))
{
    const QString token = newToken();
    options.insert(QStringLiteral("handle_token"), token);
    // Subscribed before the call, so the response cannot be missed.
    auto *pending = new PortalRequest(requestPath(token), [this, method, onSuccess](uint response, const QVariantMap &results) {
        if (m_stopped)
            return;
        if (response == 1) {
            emit failed(tr("Screen recording was not allowed."));
            return;
        }
        if (response != 0) {
            emit failed(tr("The screen cast portal refused the request (%1).").arg(method));
            return;
        }
        (this->*onSuccess)(results);
    }, this);

    QDBusMessage message = QDBusMessage::createMethodCall(kService, kObjectPath, kScreenCast, method);
    message.setArguments(QList<QVariant>(arguments) << options);
    auto *watcher = new QDBusPendingCallWatcher(QDBusConnection::sessionBus().asyncCall(message), this);
    connect(watcher, &QDBusPendingCallWatcher::finished, this, [this, pending, method](QDBusPendingCallWatcher *call) {
        call->deleteLater();
        const QDBusPendingReply<QDBusObjectPath> reply = *call;
        if (reply.isError()) {
            pending->deleteLater();
            if (!m_stopped)
                emit failed(tr("Screen cast portal error: %1").arg(reply.error().message()));
            return;
        }
        // Very old portals ignore handle_token and pick their own path.
        if (reply.value().path() != pending->path())
            pending->watch(reply.value().path());
    });
}

void ScreenCastCapture::onSessionCreated(const QVariantMap &results)
{
    m_session = results.value(QStringLiteral("session_handle")).toString();
    if (m_session.isEmpty()) {
        emit failed(tr("The screen cast portal did not create a session."));
        return;
    }
    QVariantMap options{
        {QStringLiteral("types"), kSourceMonitor},
        {QStringLiteral("multiple"), false},
    };
    // Embedded where offered; otherwise the recording simply has no pointer.
    if (portalProperty(QStringLiteral("AvailableCursorModes")) & kCursorEmbedded)
        options.insert(QStringLiteral("cursor_mode"), kCursorEmbedded);
    // Remembered until revoked (version 4+): the next recording reuses the
    // screen picked last time instead of asking again.
    if (portalProperty(QStringLiteral("version")) >= 4) {
        options.insert(QStringLiteral("persist_mode"), kPersistUntilRevoked);
        const QString token = QSettings().value(kRestoreTokenKey).toString();
        if (!token.isEmpty())
            options.insert(QStringLiteral("restore_token"), token);
    }
    request(QStringLiteral("SelectSources"), {QVariant::fromValue(QDBusObjectPath(m_session))}, options,
            &ScreenCastCapture::onSourcesSelected);
}

void ScreenCastCapture::onSourcesSelected(const QVariantMap &)
{
    request(QStringLiteral("Start"), {QVariant::fromValue(QDBusObjectPath(m_session)), QString()}, {},
            &ScreenCastCapture::onStarted);
}

void ScreenCastCapture::onStarted(const QVariantMap &results)
{
    // A token works once; each start hands out the next one.
    const QString token = results.value(QStringLiteral("restore_token")).toString();
    if (!token.isEmpty())
        QSettings().setValue(kRestoreTokenKey, token);

    // streams: a(ua{sv}), the PipeWire node of each picked source.
    quint32 node = 0;
    bool found = false;
    const QDBusArgument streams = results.value(QStringLiteral("streams")).value<QDBusArgument>();
    streams.beginArray();
    while (!streams.atEnd()) {
        quint32 id = 0;
        QVariantMap properties;
        streams.beginStructure();
        streams >> id >> properties;
        streams.endStructure();
        if (!found) {
            node = id;
            found = true;
        }
    }
    streams.endArray();
    if (!found) {
        emit failed(tr("No screen was picked."));
        return;
    }

    QDBusMessage open = QDBusMessage::createMethodCall(kService, kObjectPath, kScreenCast,
                                                       QStringLiteral("OpenPipeWireRemote"));
    open << QVariant::fromValue(QDBusObjectPath(m_session)) << QVariantMap();
    const QDBusReply<QDBusUnixFileDescriptor> fd = QDBusConnection::sessionBus().call(open);
    if (!fd.isValid() || !fd.value().isValid()) {
        emit failed(tr("Could not connect to PipeWire: %1").arg(fd.error().message()));
        return;
    }
    // The descriptor object closes its own copy.
    connectStream(fcntl(fd.value().fileDescriptor(), F_DUPFD_CLOEXEC, 3), node);
}

void ScreenCastCapture::connectStream(int fd, quint32 node)
{
    static const pw_stream_events events = [] {
        pw_stream_events e{};
        e.version = PW_VERSION_STREAM_EVENTS;
        e.state_changed = [](void *data, pw_stream_state old, pw_stream_state state, const char *error) {
            onStreamStateChanged(data, old, state, error);
        };
        e.param_changed = &ScreenCastCapture::onStreamParamChanged;
        e.process = &ScreenCastCapture::onStreamProcess;
        return e;
    }();

    m_loop = pw_thread_loop_new("glimpse-screencast", nullptr);
    m_context = pw_context_new(pw_thread_loop_get_loop(m_loop), nullptr, 0);
    if (!m_loop || !m_context || pw_thread_loop_start(m_loop) < 0) {
        close(fd);
        emit failed(tr("Could not start PipeWire."));
        return;
    }

    pw_thread_loop_lock(m_loop);
    m_core = pw_context_connect_fd(m_context, fd, nullptr, 0);
    if (!m_core) {
        pw_thread_loop_unlock(m_loop);
        emit failed(tr("Could not connect to PipeWire."));
        return;
    }
    m_stream = pw_stream_new(m_core, "Glimpse",
                             pw_properties_new(PW_KEY_MEDIA_TYPE, "Video", PW_KEY_MEDIA_CATEGORY, "Capture",
                                               PW_KEY_MEDIA_ROLE, "Screen", nullptr));
    m_streamListener = new spa_hook{};
    pw_stream_add_listener(m_stream, m_streamListener, &events, this);

    // Shared-memory buffers in a 32-bit RGB format; without modifiers on
    // offer the compositor does not send DMA-BUFs.
    uint8_t buffer[1024];
    spa_pod_builder builder = SPA_POD_BUILDER_INIT(buffer, sizeof(buffer));
    const spa_rectangle defSize = SPA_RECTANGLE(1920, 1080);
    const spa_rectangle minSize = SPA_RECTANGLE(1, 1);
    const spa_rectangle maxSize = SPA_RECTANGLE(16384, 16384);
    const spa_fraction defRate = SPA_FRACTION(0, 1);
    const spa_fraction minRate = SPA_FRACTION(0, 1);
    const spa_fraction maxRate = SPA_FRACTION(1000, 1);
    const spa_pod *params[1];
    params[0] = static_cast<const spa_pod *>(spa_pod_builder_add_object(
        &builder, SPA_TYPE_OBJECT_Format, SPA_PARAM_EnumFormat,
        SPA_FORMAT_mediaType, SPA_POD_Id(SPA_MEDIA_TYPE_video),
        SPA_FORMAT_mediaSubtype, SPA_POD_Id(SPA_MEDIA_SUBTYPE_raw),
        SPA_FORMAT_VIDEO_format, SPA_POD_CHOICE_ENUM_Id(5, SPA_VIDEO_FORMAT_BGRx, SPA_VIDEO_FORMAT_BGRx,
                                                        SPA_VIDEO_FORMAT_BGRA, SPA_VIDEO_FORMAT_RGBx, SPA_VIDEO_FORMAT_RGBA),
        SPA_FORMAT_VIDEO_size, SPA_POD_CHOICE_RANGE_Rectangle(&defSize, &minSize, &maxSize),
        SPA_FORMAT_VIDEO_framerate, SPA_POD_CHOICE_RANGE_Fraction(&defRate, &minRate, &maxRate)));
    const int result = pw_stream_connect(m_stream, PW_DIRECTION_INPUT, node,
                                         pw_stream_flags(PW_STREAM_FLAG_AUTOCONNECT | PW_STREAM_FLAG_MAP_BUFFERS),
                                         params, 1);
    pw_thread_loop_unlock(m_loop);
    if (result < 0)
        emit failed(tr("Could not connect to the screen stream."));
}

void ScreenCastCapture::onStreamStateChanged(void *data, int, int state, const char *error)
{
    auto *self = static_cast<ScreenCastCapture *>(data);
    if (state == PW_STREAM_STATE_ERROR)
        emit self->failed(tr("Screen stream error: %1").arg(QString::fromUtf8(error)));
}

void ScreenCastCapture::onStreamParamChanged(void *data, quint32 id, const spa_pod *param)
{
    auto *self = static_cast<ScreenCastCapture *>(data);
    if (!param || id != SPA_PARAM_Format)
        return;
    spa_video_info_raw info{};
    if (spa_format_video_raw_parse(param, &info) < 0)
        return;
    self->m_format = imageFormat(info.format);
    self->m_width = int(info.size.width);
    self->m_height = int(info.size.height);

    uint8_t buffer[256];
    spa_pod_builder builder = SPA_POD_BUILDER_INIT(buffer, sizeof(buffer));
    const spa_pod *params[1];
    params[0] = static_cast<const spa_pod *>(spa_pod_builder_add_object(
        &builder, SPA_TYPE_OBJECT_ParamBuffers, SPA_PARAM_Buffers,
        SPA_PARAM_BUFFERS_dataType, SPA_POD_Int((1 << SPA_DATA_MemPtr) | (1 << SPA_DATA_MemFd))));
    pw_stream_update_params(self->m_stream, params, 1);
}

void ScreenCastCapture::onStreamProcess(void *data)
{
    auto *self = static_cast<ScreenCastCapture *>(data);
    // Only the newest buffer matters.
    pw_buffer *newest = nullptr;
    while (pw_buffer *b = pw_stream_dequeue_buffer(self->m_stream)) {
        if (newest)
            pw_stream_queue_buffer(self->m_stream, newest);
        newest = b;
    }
    if (!newest)
        return;
    const spa_data &plane = newest->buffer->datas[0];
    const bool usable = plane.data && plane.chunk && plane.chunk->size > 0 && self->m_format != QImage::Format_Invalid
                        && !(plane.chunk->flags & SPA_CHUNK_FLAG_CORRUPTED);
    if (usable) {
        const int stride = plane.chunk->stride > 0 ? plane.chunk->stride : self->m_width * 4;
        const auto *bits = static_cast<const uchar *>(plane.data) + plane.chunk->offset;
        // Copied: the buffer goes back to the compositor right away.
        emit self->frameReady(QImage(bits, self->m_width, self->m_height, stride, self->m_format).copy());
    }
    pw_stream_queue_buffer(self->m_stream, newest);
}

void ScreenCastCapture::stop()
{
    m_stopped = true;
    if (m_loop) {
        pw_thread_loop_lock(m_loop);
        if (m_stream) {
            pw_stream_disconnect(m_stream);
            pw_stream_destroy(m_stream);
            m_stream = nullptr;
        }
        if (m_core) {
            pw_core_disconnect(m_core);
            m_core = nullptr;
        }
        pw_thread_loop_unlock(m_loop);
        pw_thread_loop_stop(m_loop);
    }
    delete m_streamListener;
    m_streamListener = nullptr;
    if (m_context) {
        pw_context_destroy(m_context);
        m_context = nullptr;
    }
    if (m_loop) {
        pw_thread_loop_destroy(m_loop);
        m_loop = nullptr;
    }
    closeSession();
}

void ScreenCastCapture::closeSession()
{
    if (m_session.isEmpty())
        return;
    QDBusConnection::sessionBus().asyncCall(QDBusMessage::createMethodCall(
        kService, m_session, QStringLiteral("org.freedesktop.portal.Session"), QStringLiteral("Close")));
    m_session.clear();
}

#include "ScreenCastCapture.moc"
