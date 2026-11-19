#include "SystemAudioCapture.h"

#include "AudioTrack.h"

#include <QCoreApplication>

#include <pipewire/pipewire.h>
#include <spa/param/audio/format-utils.h>
#include <spa/pod/builder.h>

#include <algorithm>

// A capture stream with stream.capture.sink set records the monitor of the
// default sink (and follows it when the default changes), which is what
// PulseAudio clients get from "<sink>.monitor". PipeWire converts to the
// format asked for.
struct SystemAudioCapture::Private {
    AudioTrack *track = nullptr;
    pw_thread_loop *loop = nullptr;
    pw_context *context = nullptr;
    pw_core *core = nullptr;
    pw_stream *stream = nullptr;
    spa_hook listener{};
    // Negotiated, on the PipeWire thread.
    int rate = AudioTrack::kRate;
    int channels = AudioTrack::kChannels;

    static void onParamChanged(void *data, uint32_t id, const spa_pod *param);
    static void onProcess(void *data);
    void close();
};

bool SystemAudioCapture::isSupported()
{
    return true;
}

SystemAudioCapture::SystemAudioCapture(AudioTrack *track)
    : d(std::make_unique<Private>())
{
    d->track = track;
    pw_init(nullptr, nullptr);
}

SystemAudioCapture::~SystemAudioCapture()
{
    stop();
}

void SystemAudioCapture::Private::onParamChanged(void *data, uint32_t id, const spa_pod *param)
{
    auto *self = static_cast<Private *>(data);
    if (!param || id != SPA_PARAM_Format)
        return;
    spa_audio_info_raw info{};
    if (spa_format_audio_raw_parse(param, &info) < 0)
        return;
    if (info.rate > 0)
        self->rate = int(info.rate);
    if (info.channels > 0)
        self->channels = int(info.channels);
}

void SystemAudioCapture::Private::onProcess(void *data)
{
    auto *self = static_cast<Private *>(data);
    pw_buffer *buffer = pw_stream_dequeue_buffer(self->stream);
    if (!buffer)
        return;
    const spa_data &plane = buffer->buffer->datas[0];
    if (plane.data && plane.chunk) {
        const uint32_t offset = std::min(plane.chunk->offset, plane.maxsize);
        const uint32_t size = std::min(plane.chunk->size, plane.maxsize - offset);
        const qsizetype frames = size / (sizeof(float) * self->channels);
        self->track->push(reinterpret_cast<const float *>(static_cast<const char *>(plane.data) + offset), frames,
                          self->channels, self->rate);
    }
    pw_stream_queue_buffer(self->stream, buffer);
}

bool SystemAudioCapture::start(QString *error)
{
    if (d->stream)
        return true;
    static const pw_stream_events events = [] {
        pw_stream_events e{};
        e.version = PW_VERSION_STREAM_EVENTS;
        e.param_changed = &Private::onParamChanged;
        e.process = &Private::onProcess;
        return e;
    }();

    d->loop = pw_thread_loop_new("glimpse-system-audio", nullptr);
    d->context = d->loop ? pw_context_new(pw_thread_loop_get_loop(d->loop), nullptr, 0) : nullptr;
    if (!d->context || pw_thread_loop_start(d->loop) < 0) {
        *error = QCoreApplication::translate("SystemAudioCapture", "PipeWire could not be started.");
        d->close();
        return false;
    }
    pw_thread_loop_lock(d->loop);
    d->core = pw_context_connect(d->context, nullptr, 0);
    if (!d->core) {
        pw_thread_loop_unlock(d->loop);
        *error = QCoreApplication::translate("SystemAudioCapture",
                                             "The sound played cannot be recorded: PipeWire is not running.");
        d->close();
        return false;
    }
    d->stream = pw_stream_new(d->core, "Glimpse",
                              pw_properties_new(PW_KEY_MEDIA_TYPE, "Audio", PW_KEY_MEDIA_CATEGORY, "Capture",
                                                PW_KEY_MEDIA_ROLE, "Production", "stream.capture.sink", "true",
                                                PW_KEY_NODE_NAME, "glimpse-system-audio", nullptr));
    pw_stream_add_listener(d->stream, &d->listener, &events, d.get());

    uint8_t buffer[1024];
    spa_pod_builder builder = SPA_POD_BUILDER_INIT(buffer, sizeof(buffer));
    spa_audio_info_raw info{};
    info.format = SPA_AUDIO_FORMAT_F32;
    info.rate = AudioTrack::kRate;
    info.channels = AudioTrack::kChannels;
    info.position[0] = SPA_AUDIO_CHANNEL_FL;
    info.position[1] = SPA_AUDIO_CHANNEL_FR;
    const spa_pod *params[1] = {spa_format_audio_raw_build(&builder, SPA_PARAM_EnumFormat, &info)};
    const int result = pw_stream_connect(d->stream, PW_DIRECTION_INPUT, PW_ID_ANY,
                                         pw_stream_flags(PW_STREAM_FLAG_AUTOCONNECT | PW_STREAM_FLAG_MAP_BUFFERS), params, 1);
    pw_thread_loop_unlock(d->loop);
    if (result < 0) {
        *error = QCoreApplication::translate("SystemAudioCapture", "The sound played could not be recorded.");
        d->close();
        return false;
    }
    return true;
}

void SystemAudioCapture::stop()
{
    d->close();
}

void SystemAudioCapture::Private::close()
{
    if (loop) {
        pw_thread_loop_lock(loop);
        if (stream) {
            pw_stream_disconnect(stream);
            pw_stream_destroy(stream);
            stream = nullptr;
        }
        if (core) {
            pw_core_disconnect(core);
            core = nullptr;
        }
        pw_thread_loop_unlock(loop);
        pw_thread_loop_stop(loop);
    }
    if (context) {
        pw_context_destroy(context);
        context = nullptr;
    }
    if (loop) {
        pw_thread_loop_destroy(loop);
        loop = nullptr;
    }
}
