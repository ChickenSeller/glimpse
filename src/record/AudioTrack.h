#pragma once

#include <QElapsedTimer>
#include <QMutex>

#include <vector>

// The sound of one source (the microphone, or what the computer plays) laid
// out on the recording's clock: 48 kHz stereo floats, where frame n was heard
// n / 48000 s after the clock started.
//
// Sources only deliver what they have, when they have it (system sound not
// at all while nothing plays), so each chunk goes where its time says, not
// after the previous one: gaps become silence and the sources stay in step
// with each other and with the video. Pushed from any thread.
class AudioTrack
{
public:
    static constexpr int kRate = 48000;
    static constexpr int kChannels = 2;

    // Interleaved samples as the device gave them, at its rate and with its
    // channels, the first of them heard `ageUs` ago, or (-1) the last of them
    // heard just now. Dropped until start().
    void push(const float *samples, qsizetype frames, int channels, int rate, qint64 ageUs = -1);
    // Starts the track at `frame` of `clock` (which then runs, and outlives
    // the track); anything earlier is dropped.
    void start(const QElapsedTimer *clock, qint64 frame);
    // Adds frames [from, to) to `out` ((to - from) stereo frames) and drops
    // everything before `to`; what has not arrived stays silent.
    void mixInto(float *out, qint64 from, qint64 to);

    // The clock's current frame.
    static qint64 frameAt(const QElapsedTimer &clock);

private:
    // To stereo at the device's rate, then to kRate.
    void toStereo(const float *samples, qsizetype frames, int channels);
    void resample(int rate);

    QMutex m_mutex;
    const QElapsedTimer *m_clock = nullptr;
    std::vector<float> m_data; // stereo, from frame m_start on
    qint64 m_start = -1;       // -1: not started, chunks are dropped

    // Working buffers and the resampler's state, kept between chunks.
    std::vector<float> m_stereo;
    std::vector<float> m_resampled;
    int m_rate = 0;
    double m_phase = 0.0;
    float m_previous[2] = {0.0f, 0.0f};
};
