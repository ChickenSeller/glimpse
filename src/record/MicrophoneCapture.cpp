#include "MicrophoneCapture.h"

#include "AudioTrack.h"

#include <QAudioSource>
#include <QIODevice>
#include <QMediaDevices>

MicrophoneCapture::MicrophoneCapture(const QByteArray &deviceId, AudioTrack *track, QObject *parent)
    : QObject(parent)
    , m_track(track)
{
    // The chosen device where it is still there, else the default one.
    const QList<QAudioDevice> inputs = QMediaDevices::audioInputs();
    for (const QAudioDevice &device : inputs) {
        if (!deviceId.isEmpty() && device.id() == deviceId)
            m_device = device;
    }
    if (m_device.isNull())
        m_device = QMediaDevices::defaultAudioInput();
}

MicrophoneCapture::~MicrophoneCapture()
{
    stop();
}

bool MicrophoneCapture::start(QString *error)
{
    if (m_device.isNull()) {
        *error = tr("No microphone was found.");
        return false;
    }
    // The device's own rate and channels, as floats where it can.
    m_format = m_device.preferredFormat();
    QAudioFormat floats = m_format;
    floats.setSampleFormat(QAudioFormat::Float);
    if (m_device.isFormatSupported(floats))
        m_format = floats;

    m_source = new QAudioSource(m_device, m_format, this);
    m_source->setBufferSize(m_format.bytesForDuration(200000));
    m_io = m_source->start();
    if (!m_io || m_source->error() != QtAudio::NoError) {
        *error = tr("The microphone \"%1\" could not be opened. On Windows, check that apps may use the "
                    "microphone (Settings, Privacy & security, Microphone).")
                     .arg(m_device.description());
        stop();
        return false;
    }
    connect(m_io, &QIODevice::readyRead, this, &MicrophoneCapture::read);
    return true;
}

void MicrophoneCapture::stop()
{
    if (!m_source)
        return;
    if (m_io)
        read(); // what is left in the buffer
    m_source->stop();
    delete m_source;
    m_source = nullptr;
    m_io = nullptr;
}

void MicrophoneCapture::read()
{
    if (!m_io)
        return;
    const QByteArray bytes = m_io->readAll();
    const int channels = m_format.channelCount();
    const int frameBytes = m_format.bytesPerFrame();
    if (bytes.isEmpty() || channels <= 0 || frameBytes <= 0)
        return;
    const qsizetype frames = bytes.size() / frameBytes;
    const qsizetype count = frames * channels;
    m_samples.resize(size_t(count));
    const char *data = bytes.constData();
    for (qsizetype i = 0; i < count; ++i)
        m_samples[size_t(i)] = m_format.normalizedSampleValue(data + i * m_format.bytesPerSample());
    m_track->push(m_samples.data(), frames, channels, m_format.sampleRate());
}
