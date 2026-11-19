#include "AudioTrack.h"

#include <QMutexLocker>

#include <algorithm>
#include <cmath>

namespace {

// A chunk that starts within this of where the previous one ended follows
// it directly: devices deliver in bursts, a little early or late.
constexpr qint64 kJitterFrames = AudioTrack::kRate * 40 / 1000;

} // namespace

qint64 AudioTrack::frameAt(const QElapsedTimer &clock)
{
    return clock.nsecsElapsed() * kRate / 1000000000;
}

void AudioTrack::toStereo(const float *samples, qsizetype frames, int channels)
{
    m_stereo.resize(size_t(frames) * 2);
    float *out = m_stereo.data();
    constexpr float kHalf = 0.70710678f; // -3 dB for what is shared by both sides
    for (qsizetype i = 0; i < frames; ++i) {
        const float *in = samples + i * channels;
        float left = in[0];
        float right = channels > 1 ? in[1] : in[0];
        // Surround (Windows' order: front left and right, center, LFE, back
        // left and right, side left and right) folded into the two sides.
        if (channels >= 3) {
            left += kHalf * in[2];
            right += kHalf * in[2];
        }
        if (channels >= 6) {
            left += kHalf * in[4];
            right += kHalf * in[5];
        }
        if (channels >= 8) {
            left += kHalf * in[6];
            right += kHalf * in[7];
        }
        out[2 * i] = left;
        out[2 * i + 1] = right;
    }
}

void AudioTrack::resample(int rate)
{
    if (rate == kRate) {
        m_resampled.swap(m_stereo);
        return;
    }
    if (rate != m_rate) {
        m_rate = rate;
        m_phase = 0.0;
        m_previous[0] = m_previous[1] = 0.0f;
    }
    // Linear, between the frames of the chunk with the last frame of the one
    // before in front (position 0): plenty for speech and a demo's sound.
    const qsizetype frames = qsizetype(m_stereo.size() / 2);
    const double step = double(rate) / kRate;
    const auto at = [&](qsizetype i, int c) { return i == 0 ? m_previous[c] : m_stereo[size_t(i - 1) * 2 + c]; };
    m_resampled.clear();
    double pos = m_phase;
    while (pos < double(frames)) {
        const qsizetype i = qsizetype(pos);
        const float f = float(pos - double(i));
        for (int c = 0; c < 2; ++c)
            m_resampled.push_back(at(i, c) * (1.0f - f) + at(i + 1, c) * f);
        pos += step;
    }
    m_phase = pos - double(frames);
    if (frames > 0) {
        m_previous[0] = m_stereo[size_t(frames - 1) * 2];
        m_previous[1] = m_stereo[size_t(frames - 1) * 2 + 1];
    }
}

void AudioTrack::push(const float *samples, qsizetype frames, int channels, int rate, qint64 ageUs)
{
    if (frames <= 0 || channels <= 0 || rate <= 0)
        return;
    QMutexLocker locker(&m_mutex);
    if (m_start < 0)
        return;
    const qint64 now = frameAt(*m_clock);
    toStereo(samples, frames, channels);
    resample(rate);
    const qint64 count = qint64(m_resampled.size() / 2);
    if (count == 0)
        return;

    const float *data = m_resampled.data();
    qint64 n = count;
    const qint64 start = ageUs >= 0 ? now - ageUs * kRate / 1000000 : now - count;
    const qint64 have = m_start + qint64(m_data.size() / 2); // where the track ends
    if (std::abs(start - have) <= kJitterFrames) {
        // Right after the previous chunk.
    } else if (start > have) {
        m_data.resize(size_t(start - m_start) * 2, 0.0f); // a gap: silence
    } else {
        // Overlaps what is there (the device's clock runs a little fast,
        // or the time before the track's start was mixed already): only the
        // rest is new.
        const qint64 skip = std::min(n, have - start);
        data += skip * 2;
        n -= skip;
    }
    if (n > 0)
        m_data.insert(m_data.end(), data, data + n * 2);
}

void AudioTrack::start(const QElapsedTimer *clock, qint64 frame)
{
    QMutexLocker locker(&m_mutex);
    m_clock = clock;
    m_data.clear();
    m_start = frame;
}

void AudioTrack::mixInto(float *out, qint64 from, qint64 to)
{
    QMutexLocker locker(&m_mutex);
    if (m_start < 0 || to <= from)
        return;
    const qint64 end = m_start + qint64(m_data.size() / 2);
    const qint64 a = std::max(from, m_start);
    const qint64 b = std::min(to, end);
    for (qint64 f = a; f < b; ++f) {
        out[(f - from) * 2] += m_data[size_t(f - m_start) * 2];
        out[(f - from) * 2 + 1] += m_data[size_t(f - m_start) * 2 + 1];
    }
    // Drop what is mixed (and anything older).
    const qint64 drop = std::clamp(to - m_start, qint64(0), end - m_start);
    m_data.erase(m_data.begin(), m_data.begin() + drop * 2);
    // Past what never came, too: a chunk for that time is too late now.
    m_start = std::max(m_start, to);
}
