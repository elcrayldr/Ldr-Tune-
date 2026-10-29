#pragma once
#include <vector>
#include <cmath>
#include <algorithm>
#include <cstdint>

namespace CrayDSP
{
/**
 * Low-latency dual-read-head pitch shifter for monophonic vocal correction.
 * Two continuously moving delay taps are crossfaded to hide the delay reset.
 * This is not WSOLA time-stretching; the read-head slope changes the pitch.
 */
class WsolaPitchShifter
{
public:
    WsolaPitchShifter() = default;

    void prepare(double sampleRate, int maxBlockSize)
    {
        m_sampleRate = static_cast<float>(sampleRate);
        m_maxBlockSize = maxBlockSize;
        m_bufferSize = 4096;
        m_bufferMask = m_bufferSize - 1;
        m_inputRingBuffer.assign(static_cast<size_t>(m_bufferSize), 0.0f);
        setGrainSize(m_grainSize);
    }

    /** Grain controls the delay excursion and approximate fixed latency. */
    void setGrainSize(int grain)
    {
        m_grainSize = std::clamp(grain, 32, kMaxGrain);
        m_minDelay = 4.0f;
        m_maxDelay = static_cast<float>(m_grainSize - 4);
        m_delayRange = std::max(8.0f, m_maxDelay - m_minDelay);
        reset();
    }

    void reset()
    {
        std::fill(m_inputRingBuffer.begin(), m_inputRingBuffer.end(), 0.0f);
        m_writePos = 0;
        m_phase = 0.0f;
        m_targetRatio = 1.0f;
        m_currentRatio = 1.0f;
        m_activeMix = 0.0f;
    }

    void setShiftRatio(float ratio)
    {
        if (!std::isfinite(ratio))
            ratio = 1.0f;
        m_targetRatio = std::clamp(ratio, 0.5f, 2.0f);
    }

    // The unity path uses a fixed centre tap; report that actual delay to the host.
    int getLatencySamples() const { return m_grainSize / 2; }

    float processSample(float inSample)
    {
        if (!std::isfinite(inSample))
            inSample = 0.0f;

        m_inputRingBuffer[static_cast<size_t>(m_writePos)] = inSample;
        m_writePos = (m_writePos + 1) & m_bufferMask;

        // Smooth pitch-ratio changes to avoid abrupt read-head velocity changes.
        m_currentRatio += (m_targetRatio - m_currentRatio) * 0.02f;
        const float deviation = std::abs(m_currentRatio - 1.0f);
        const float activeTarget = deviation > 0.0015f ? 1.0f : 0.0f;
        const float mixCoeff = 1.0f - std::exp(-1.0f / (0.005f * std::max(1.0f, m_sampleRate)));
        m_activeMix += (activeTarget - m_activeMix) * mixCoeff;

        const float centreDelay = 0.5f * (m_minDelay + m_maxDelay);
        const float dryAligned = readDelay(centreDelay);

        if (m_activeMix < 0.0001f)
        {
            // Keep the pitch-shifter phase deterministic when correction is inactive.
            m_phase = 0.0f;
            return dryAligned;
        }

        const bool shiftUp = m_currentRatio > 1.0f;
        const float delay1 = shiftUp
            ? (m_maxDelay - m_phase * m_delayRange)
            : (m_minDelay + m_phase * m_delayRange);

        float phase2 = m_phase + 0.5f;
        if (phase2 >= 1.0f)
            phase2 -= 1.0f;

        const float delay2 = shiftUp
            ? (m_maxDelay - phase2 * m_delayRange)
            : (m_minDelay + phase2 * m_delayRange);

        // Hann crossfade: one head fades out exactly as the other fades in.
        const float w1 = 0.5f - 0.5f * std::cos(6.28318530718f * m_phase);
        const float w2 = 1.0f - w1;
        const float shifted = readDelay(delay1) * w1 + readDelay(delay2) * w2;

        // At very small corrections, blend smoothly to the fixed-delay path.
        const float out = dryAligned + m_activeMix * (shifted - dryAligned);

        const float phaseStep = std::abs(1.0f - m_currentRatio) / m_delayRange;
        m_phase += phaseStep;
        if (m_phase >= 1.0f)
            m_phase -= std::floor(m_phase);

        return std::isfinite(out) ? out : dryAligned;
    }

private:
    float readDelay(float delaySamples) const
    {
        // Linear interpolation keeps the reader fractional and avoids discontinuities.
        const float readPos = static_cast<float>(m_writePos) - delaySamples;
        const float wrapped = readPos < 0.0f ? readPos + static_cast<float>(m_bufferSize) : readPos;
        const int index0 = static_cast<int>(wrapped) & m_bufferMask;
        const int index1 = (index0 + 1) & m_bufferMask;
        const float fraction = wrapped - std::floor(wrapped);
        const float a = m_inputRingBuffer[static_cast<size_t>(index0)];
        const float b = m_inputRingBuffer[static_cast<size_t>(index1)];
        return a + fraction * (b - a);
    }

    float m_sampleRate = 48000.0f;
    int m_maxBlockSize = 512;
    static constexpr int kMaxGrain = 256;
    int m_grainSize = 64;

    int m_bufferSize = 4096;
    int m_bufferMask = 4095;
    std::vector<float> m_inputRingBuffer;

    float m_minDelay = 4.0f;
    float m_maxDelay = 60.0f;
    float m_delayRange = 56.0f;
    int m_writePos = 0;
    float m_phase = 0.0f;
    float m_targetRatio = 1.0f;
    float m_currentRatio = 1.0f;
    float m_activeMix = 0.0f;
};
} // namespace CrayDSP
