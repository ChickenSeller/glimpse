#pragma once

#include <QImage>
#include <QObject>
#include <QString>

struct pw_thread_loop;
struct pw_context;
struct pw_core;
struct pw_stream;
struct spa_hook;

// Wayland screen capture through org.freedesktop.portal.ScreenCast and
// PipeWire, with the pointer drawn into the frames by the compositor.
// QScreenCapture uses the same portal but asks for no pointer, and the
// pointer position is not available on Wayland to paint it in ourselves.
//
// The portal lets the user pick the monitor. Frames come in physical pixels,
// only when the screen changes.
class ScreenCastCapture : public QObject
{
    Q_OBJECT

public:
    explicit ScreenCastCapture(QObject *parent = nullptr);
    ~ScreenCastCapture() override;

    // Asks the portal (which may show its dialog), then streams.
    void start();
    void stop();

signals:
    // A whole monitor; emitted from the PipeWire thread.
    void frameReady(const QImage &image);
    void failed(const QString &message);

private:
    // One portal call that answers through an org.freedesktop.portal.Request.
    void request(const QString &method, const QList<QVariant> &arguments, QVariantMap options,
                 void (ScreenCastCapture::*onSuccess)(const QVariantMap &results));
    void onSessionCreated(const QVariantMap &results);
    void onSourcesSelected(const QVariantMap &results);
    void onStarted(const QVariantMap &results);
    void connectStream(int fd, quint32 node);
    void closeSession();

    static void onStreamStateChanged(void *data, int old, int state, const char *error);
    static void onStreamParamChanged(void *data, quint32 id, const struct spa_pod *param);
    static void onStreamProcess(void *data);

    QString m_session;
    bool m_stopped = false;

    pw_thread_loop *m_loop = nullptr;
    pw_context *m_context = nullptr;
    pw_core *m_core = nullptr;
    pw_stream *m_stream = nullptr;
    spa_hook *m_streamListener = nullptr;
    // Negotiated, on the PipeWire thread.
    QImage::Format m_format = QImage::Format_Invalid;
    int m_width = 0;
    int m_height = 0;
};
