#include "SystemAudioCapture.h"

#include "AudioTrack.h"

#include <QCoreApplication>

#include <atomic>
#include <future>
#include <thread>
#include <vector>

#include <windows.h>
#include <audioclient.h>
#include <mmdeviceapi.h>

namespace {

template<typename T>
void release(T *&p)
{
    if (p) {
        p->Release();
        p = nullptr;
    }
}

// The mix format's samples as floats; the sub-formats differ in Data1 only
// (KSDATAFORMAT_SUBTYPE_PCM: 1, _IEEE_FLOAT: 3).
struct SampleReader {
    bool isFloat = false;
    int bits = 0; // per sample, as stored
    int validBits = 0;

    bool init(const WAVEFORMATEX *format)
    {
        WORD tag = format->wFormatTag;
        bits = format->wBitsPerSample;
        validBits = bits;
        if (tag == WAVE_FORMAT_EXTENSIBLE && format->cbSize >= 22) {
            const auto *ext = reinterpret_cast<const WAVEFORMATEXTENSIBLE *>(format);
            tag = WORD(ext->SubFormat.Data1);
            if (ext->Samples.wValidBitsPerSample)
                validBits = ext->Samples.wValidBitsPerSample;
        }
        isFloat = tag == WAVE_FORMAT_IEEE_FLOAT && bits == 32;
        return isFloat || (tag == WAVE_FORMAT_PCM && (bits == 16 || bits == 24 || bits == 32));
    }

    void read(const BYTE *in, size_t count, float *out) const
    {
        if (isFloat) {
            memcpy(out, in, count * sizeof(float));
            return;
        }
        for (size_t i = 0; i < count; ++i) {
            switch (bits) {
            case 16: out[i] = reinterpret_cast<const qint16 *>(in)[i] / 32768.0f; break;
            case 24: {
                const BYTE *s = in + i * 3;
                const qint32 v = qint32(quint32(s[0]) << 8 | quint32(s[1]) << 16 | quint32(s[2]) << 24);
                out[i] = float(v / 2147483648.0);
                break;
            }
            default: out[i] = float(reinterpret_cast<const qint32 *>(in)[i] / 2147483648.0); break;
            }
        }
    }
};

} // namespace

struct SystemAudioCapture::Private {
    AudioTrack *track = nullptr;
    std::thread thread;
    std::atomic<bool> stopping{false};

    void run(std::promise<QString> started);
};

bool SystemAudioCapture::isSupported()
{
    return true;
}

SystemAudioCapture::SystemAudioCapture(AudioTrack *track)
    : d(std::make_unique<Private>())
{
    d->track = track;
}

SystemAudioCapture::~SystemAudioCapture()
{
    stop();
}

bool SystemAudioCapture::start(QString *error)
{
    if (d->thread.joinable())
        return true;
    d->stopping = false;
    std::promise<QString> started;
    std::future<QString> result = started.get_future();
    d->thread = std::thread(&Private::run, d.get(), std::move(started));
    const QString message = result.get();
    if (message.isEmpty())
        return true;
    d->thread.join();
    *error = message;
    return false;
}

void SystemAudioCapture::stop()
{
    d->stopping = true;
    if (d->thread.joinable())
        d->thread.join();
}

void SystemAudioCapture::Private::run(std::promise<QString> started)
{
    // Everything COM lives on this thread.
    const HRESULT init = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
    IMMDeviceEnumerator *enumerator = nullptr;
    IMMDevice *device = nullptr;
    IAudioClient *client = nullptr;
    IAudioCaptureClient *capture = nullptr;
    WAVEFORMATEX *format = nullptr;
    SampleReader reader;
    const auto cleanUp = [&] {
        if (client)
            client->Stop();
        release(capture);
        release(client);
        release(device);
        release(enumerator);
        if (format)
            CoTaskMemFree(format);
        if (SUCCEEDED(init))
            CoUninitialize();
    };

    QString failure;
    if (FAILED(CoCreateInstance(__uuidof(MMDeviceEnumerator), nullptr, CLSCTX_ALL, __uuidof(IMMDeviceEnumerator),
                                reinterpret_cast<void **>(&enumerator)))
        || FAILED(enumerator->GetDefaultAudioEndpoint(eRender, eConsole, &device))) {
        failure = QCoreApplication::translate("SystemAudioCapture", "No sound output device was found.");
    } else if (FAILED(device->Activate(__uuidof(IAudioClient), CLSCTX_ALL, nullptr, reinterpret_cast<void **>(&client)))
               || FAILED(client->GetMixFormat(&format))) {
        failure = QCoreApplication::translate("SystemAudioCapture", "The sound output device could not be opened.");
    } else if (!reader.init(format)) {
        failure = QCoreApplication::translate("SystemAudioCapture", "The sound output device uses a sample format Glimpse cannot read.");
    } else if (FAILED(client->Initialize(AUDCLNT_SHAREMODE_SHARED, AUDCLNT_STREAMFLAGS_LOOPBACK, 2000000 /* 200 ms */,
                                         0, format, nullptr))
               || FAILED(client->GetService(__uuidof(IAudioCaptureClient), reinterpret_cast<void **>(&capture)))
               || FAILED(client->Start())) {
        failure = QCoreApplication::translate("SystemAudioCapture", "The sound played could not be recorded.");
    }
    if (!failure.isEmpty()) {
        cleanUp();
        started.set_value(failure);
        return;
    }
    started.set_value(QString());

    const int channels = format->nChannels;
    const int rate = int(format->nSamplesPerSec);
    std::vector<float> samples;
    LARGE_INTEGER frequency;
    QueryPerformanceFrequency(&frequency);
    // Packets come every 10 ms or so while something plays, none in silence
    // (the track fills that in).
    while (!stopping) {
        Sleep(10);
        UINT32 packet = 0;
        while (!stopping && SUCCEEDED(capture->GetNextPacketSize(&packet)) && packet > 0) {
            BYTE *data = nullptr;
            UINT32 frames = 0;
            DWORD flags = 0;
            // When the packet's first frame played, in 100 ns units of the
            // performance counter.
            UINT64 played = 0;
            if (FAILED(capture->GetBuffer(&data, &frames, &flags, nullptr, &played)))
                break;
            LARGE_INTEGER counter;
            QueryPerformanceCounter(&counter);
            const qint64 nowHns = qint64(counter.QuadPart / frequency.QuadPart * 10000000
                                         + counter.QuadPart % frequency.QuadPart * 10000000 / frequency.QuadPart);
            // (Without a sensible one: heard just now.)
            const qint64 ageHns = nowHns - qint64(played);
            const qint64 ageUs = played && ageHns >= 0 && ageHns < 10000000 ? ageHns / 10 : -1;
            samples.resize(size_t(frames) * channels);
            if (flags & AUDCLNT_BUFFERFLAGS_SILENT)
                std::fill(samples.begin(), samples.end(), 0.0f);
            else
                reader.read(data, samples.size(), samples.data());
            capture->ReleaseBuffer(frames);
            track->push(samples.data(), frames, channels, rate, ageUs);
        }
        // The device went away (unplugged, switched off): the rest is silent.
        if (FAILED(capture->GetNextPacketSize(&packet)))
            break;
    }
    cleanUp();
}
