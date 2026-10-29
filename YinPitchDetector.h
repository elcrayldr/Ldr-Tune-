#pragma once
#include <vector>
#include <cmath>
#include <algorithm>
#include <cstdint>

namespace CrayDSP
{
/** Detector de tono fundamental (F0) basado en YIN. Sin alocaciones tras prepare(). */
class YinPitchDetector
{
public:
    enum class RangePreset { Low, Mid, High };   // 65-350 / 100-600 / 150-1000 Hz
    enum class TrackingMode { Fast, Stable };

    YinPitchDetector() = default;

    struct PitchResult
    {
        float frequencyHz = 0.0f;
        float confidence = 0.0f;
        bool isVoiced = false;
        int periodSamples = 0;
    };

    void prepare(double sampleRate, int maxBlockSize)
    {
        m_sampleRate = static_cast<float>(sampleRate);
        m_maxBlockSize = maxBlockSize;

        m_bufferSize = 2048;
        m_bufferMask = m_bufferSize - 1;
        m_circularBuffer.assign(static_cast<size_t>(m_bufferSize), 0.0f);

        m_maxPossiblePeriod = 1024;
        m_diffBuffer.assign(static_cast<size_t>(m_maxPossiblePeriod), 0.0f);
        m_cmndBuffer.assign(static_cast<size_t>(m_maxPossiblePeriod), 0.0f);

        setRangePreset(m_rangePreset);
        setTrackingMode(m_trackingMode);

        m_writePos = 0;
        m_hopCounter = 0;
        m_lastPitch = 130.81f;
        m_lastConfidence = 0.0f;
        m_isVoiced = false;
        m_cachedResult = { 130.81f, 0.0f, false, 367 };
    }

    void setRangePreset(RangePreset preset)
    {
        m_rangePreset = preset;
        switch (preset)
        {
            case RangePreset::Low:  m_minFreq = 65.0f;  m_maxFreq = 350.0f;  break;
            case RangePreset::Mid:  m_minFreq = 100.0f; m_maxFreq = 600.0f;  break;
            case RangePreset::High: m_minFreq = 150.0f; m_maxFreq = 1000.0f; break;
        }

        m_minPeriod = std::clamp(static_cast<int>(m_sampleRate / m_maxFreq), 4, m_maxPossiblePeriod - 2);
        m_maxPeriod = std::clamp(static_cast<int>(m_sampleRate / m_minFreq), m_minPeriod + 4, m_maxPossiblePeriod - 1);
    }

    void setTrackingMode(TrackingMode mode)
    {
        m_trackingMode = mode;
        m_voicingThreshold = (mode == TrackingMode::Fast) ? 0.20f : 0.12f;
        m_hopSize = (mode == TrackingMode::Fast) ? 32 : 64;
    }

    PitchResult processSample(float sample)
    {
        if (!std::isfinite(sample))
            sample = 0.0f;

        m_circularBuffer[static_cast<size_t>(m_writePos)] = sample;
        m_writePos = (m_writePos + 1) & m_bufferMask;

        if (++m_hopCounter >= m_hopSize)
        {
            m_hopCounter = 0;
            m_cachedResult = computeYin();
        }

        return m_cachedResult;
    }

    void reset()
    {
        std::fill(m_circularBuffer.begin(), m_circularBuffer.end(), 0.0f);
        m_writePos = 0;
        m_hopCounter = 0;
        m_lastPitch = 130.81f;
        m_lastConfidence = 0.0f;
        m_isVoiced = false;
    }

private:
    PitchResult computeYin()
    {
        PitchResult res;
        const int W = m_maxPeriod;
        const int minP = m_minPeriod;
        const int maxP = m_maxPeriod;

        float energySum = 0.0f;
        for (int j = 0; j < W; ++j)
        {
            const int idx = (m_writePos - 1 - j) & m_bufferMask;
            const float v = m_circularBuffer[static_cast<size_t>(idx)];
            energySum += v * v;
        }
        const float rms = std::sqrt(energySum / static_cast<float>(W));
        if (rms < 0.005f)
        {
            res.isVoiced = false;
            res.confidence = 0.0f;
            res.frequencyHz = m_lastPitch;
            res.periodSamples = std::max(4, static_cast<int>(m_sampleRate / m_lastPitch));
            return res;
        }

        // 1. Funcion de diferencia
        for (int tau = 0; tau < maxP; ++tau)
        {
            float sum = 0.0f;
            for (int j = 0; j < W; ++j)
            {
                const int idx1 = (m_writePos - 1 - j) & m_bufferMask;
                const int idx2 = (idx1 - tau) & m_bufferMask;
                const float diff = m_circularBuffer[static_cast<size_t>(idx1)] - m_circularBuffer[static_cast<size_t>(idx2)];
                sum += diff * diff;
            }
            m_diffBuffer[static_cast<size_t>(tau)] = sum;
        }

        // 2. CMNDF
        m_cmndBuffer[0] = 1.0f;
        float runningSum = 0.0f;
        for (int tau = 1; tau < maxP; ++tau)
        {
            runningSum += m_diffBuffer[static_cast<size_t>(tau)];
            if (runningSum > 1e-7f)
                m_cmndBuffer[static_cast<size_t>(tau)] = (m_diffBuffer[static_cast<size_t>(tau)] * static_cast<float>(tau)) / runningSum;
            else
                m_cmndBuffer[static_cast<size_t>(tau)] = 1.0f;
        }

        // 3. Primer minimo bajo el umbral
        int tauFound = -1;
        for (int tau = minP; tau < maxP; ++tau)
        {
            if (m_cmndBuffer[static_cast<size_t>(tau)] < m_voicingThreshold)
            {
                while (tau + 1 < maxP && m_cmndBuffer[static_cast<size_t>(tau + 1)] < m_cmndBuffer[static_cast<size_t>(tau)])
                    tau++;
                tauFound = tau;
                break;
            }
        }

        if (tauFound == -1)
        {
            int globalMinTau = minP;
            float minVal = m_cmndBuffer[static_cast<size_t>(minP)];
            for (int tau = minP + 1; tau < maxP; ++tau)
            {
                if (m_cmndBuffer[static_cast<size_t>(tau)] < minVal)
                {
                    minVal = m_cmndBuffer[static_cast<size_t>(tau)];
                    globalMinTau = tau;
                }
            }

            if (m_trackingMode == TrackingMode::Fast && minVal < 0.32f)
            {
                tauFound = globalMinTau;
            }
            else
            {
                res.isVoiced = false;
                res.confidence = 0.0f;
                res.frequencyHz = m_lastPitch;
                res.periodSamples = std::max(4, static_cast<int>(m_sampleRate / m_lastPitch));
                return res;
            }
        }

        // 4. Prevencion de saltos de octava
        if (tauFound / 2 >= minP)
        {
            const int halfTau = tauFound / 2;
            if (m_cmndBuffer[static_cast<size_t>(halfTau)] < m_voicingThreshold * 1.35f)
                tauFound = halfTau;
        }

        // 5. Interpolacion parabolica
        float betterTau = static_cast<float>(tauFound);
        if (tauFound > 0 && tauFound + 1 < maxP)
        {
            const float s0 = m_cmndBuffer[static_cast<size_t>(tauFound - 1)];
            const float s1 = m_cmndBuffer[static_cast<size_t>(tauFound)];
            const float s2 = m_cmndBuffer[static_cast<size_t>(tauFound + 1)];
            const float denom = 2.0f * (2.0f * s1 - s0 - s2);
            if (std::abs(denom) > 1e-6f)
            {
                const float delta = (s2 - s0) / denom;
                if (std::abs(delta) < 1.0f)
                    betterTau += delta;
            }
        }

        const float detectedFreq = m_sampleRate / std::max(1.0f, betterTau);
        const float confidence = std::clamp(1.0f - m_cmndBuffer[static_cast<size_t>(tauFound)], 0.0f, 1.0f);

        if (detectedFreq < m_minFreq || detectedFreq > m_maxFreq)
        {
            res.isVoiced = false;
            res.confidence = 0.0f;
            res.frequencyHz = m_lastPitch;
            res.periodSamples = std::max(4, static_cast<int>(betterTau + 0.5f));
            return res;
        }

        res.frequencyHz = detectedFreq;
        res.confidence = confidence;
        res.isVoiced = (confidence > (m_trackingMode == TrackingMode::Fast ? 0.35f : 0.45f));
        res.periodSamples = std::max(4, static_cast<int>(betterTau + 0.5f));

        if (res.isVoiced)
        {
            m_lastPitch = res.frequencyHz;
            m_lastConfidence = res.confidence;
            m_isVoiced = true;
        }
        else
        {
            m_isVoiced = false;
        }

        return res;
    }

    float m_sampleRate = 48000.0f;
    int m_maxBlockSize = 512;
    int m_bufferSize = 2048;
    int m_bufferMask = 2047;
    int m_writePos = 0;

    RangePreset m_rangePreset = RangePreset::Mid;
    TrackingMode m_trackingMode = TrackingMode::Fast;

    float m_minFreq = 100.0f;
    float m_maxFreq = 600.0f;
    int m_minPeriod = 80;
    int m_maxPeriod = 480;
    int m_maxPossiblePeriod = 1024;

    std::vector<float> m_circularBuffer;
    std::vector<float> m_diffBuffer;
    std::vector<float> m_cmndBuffer;

    int m_hopSize = 32;
    int m_hopCounter = 0;

    float m_voicingThreshold = 0.20f;
    float m_lastPitch = 130.81f;
    float m_lastConfidence = 0.0f;
    bool m_isVoiced = false;
    PitchResult m_cachedResult;
};
} // namespace CrayDSP
