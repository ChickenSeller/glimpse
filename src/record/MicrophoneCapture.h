#pragma once

#include <QAudioDevice>
#include <QAudioFormat>
#include <QObject>

#include <vector>

class AudioTrack;
class QAudioSource;
class QIODevice;

// The sound of a microphone (or any other input device) into an AudioTrack.
class MicrophoneCapture : public QObject
{
    Q_OBJECT

public:
    // An empty id: the system's default input.
    MicrophoneCapture(const QByteArray &deviceId, AudioTrack *track, QObject *parent = nullptr);
    ~MicrophoneCapture() override;

    bool start(QString *error);
    void stop();

private:
    void read();

    QAudioDevice m_device;
    AudioTrack *m_track;
    QAudioSource *m_source = nullptr;
    QIODevice *m_io = nullptr;
    QAudioFormat m_format;
    std::vector<float> m_samples;
};
