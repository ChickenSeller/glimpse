#pragma once

#include <QString>

#include <memory>

class AudioTrack;

// What the computer plays (the default output device), into an AudioTrack.
// Windows: WASAPI loopback. Linux: a PipeWire stream on the default sink's
// monitor. The sound comes on a thread of its own.
class SystemAudioCapture
{
public:
    static bool isSupported();

    explicit SystemAudioCapture(AudioTrack *track);
    ~SystemAudioCapture();

    bool start(QString *error);
    void stop();

private:
    struct Private;
    std::unique_ptr<Private> d;
};
