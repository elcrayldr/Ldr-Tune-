#pragma once
#include <vector>
#include <cmath>
#include <algorithm>
#include <cstdint>

namespace CrayDSP
{
/** Pitch shifter WSOLA causal para correccion vocal (+/-300 cents tipico). Latencia fija = grainSize. */
class WsolaPitchShifter
{
public:
    WsolaPitchShifter() = default;

    void prepare(double sampleRate, int maxBlockSize)
    {
        m_sampleRate = static_cast<float>(sampleRate);
        m_maxBlockSize = maxBlockSize;

        m_bufferSize = 2048;
        m_bufferMask = m_bufferSize - 1;

        m_inputRingBuffer.assign(static_cast<size_t>(m_bufferSize), 0.0f);
        m_outputAccumBuffer.assign(static_cast<size_t>(m_bufferSize), 0.0f);
        m_windowWeightBuffer.assign(static_cast<size_t>(m_bufferSize), 0.0f);
        m_hanningWindow.assign(static_cast<size_t>(kMaxGrain), 0.0f);

        setGrainSize(m_grainSize);
    }

    /** Cambia el tamano de grano (= latencia en muestras). Sin alocaciones; reinicia el estado. */
    void setGrainSize(int grain)
    {
        m_grainSize = std::clamp(grain, 32, kMaxGrain);
        m_hopSize = m_grainSize / 4;                     // 75 % de solape
        m_searchRange = std::max(16, m_grainSize / 4);

        for (int i = 0; i < m_grainSize; ++i)
            m_hanningWindow[static_cast<size_t>(i)] = 0.5f * (1.0f - std::cos(2.0f * 3.1415926535f * static_cast<float>(i) / static_cast<float>(m_grainSize)));

        reset();
    }

    void reset()
    {
        std::fill(m_inputRingBuffer.begin(), m_inputRingBuffer.end(), 0.0f);
        std::fill(m_outputAccumBuffer.begin(), m_outputAccumBuffer.end(), 0.0f);
        std::fill(m_windowWeightBuffer.begin(), m_windowWeightBuffer.end(), 0.0f);

        m_inWritePos = 0;
        m_outReadPos = 0;
        m_hopSampleCounter = 0;
        m_targetRatio = 1.0f;
        m_currentRatio = 1.0f;
        m_virtualSynthesisPos = 0.0f;
        m_lastBestOffset = 0;
    }

    void setShiftRatio(float ratio) { m_targetRatio = std::clamp(ratio, 0.5f, 2.0f); }

    int getLatencySamples() const { return m_grainSize; }

    float processSample(float inSample)
    {
        if (!std::isfinite(inSample))
            inSample = 0.0f;

        m_inputRingBuffer[static_cast<size_t>(m_inWritePos)] = inSample;
        m_inWritePos = (m_inWritePos + 1) & m_bufferMask;

        if (++m_hopSampleCounter >= m_hopSize)
        {
            m_hopSampleCounter = 0;
            m_currentRatio += (m_targetRatio - m_currentRatio) * 0.25f;
            synthesizeNextGrain();
        }

        const int rPos = m_outReadPos;
        float outSample = 0.0f;
        const float weight = m_windowWeightBuffer[static_cast<size_t>(rPos)];

        if (weight > 1e-4f)
        {
            outSample = m_outputAccumBuffer[static_cast<size_t>(rPos)] / weight;
        }
        else
        {
            const int delayIdx = (m_inWritePos - 1 - m_grainSize) & m_bufferMask;
            outSample = m_inputRingBuffer[static_cast<size_t>(delayIdx)];
        }

        m_outputAccumBuffer[static_cast<size_t>(rPos)] = 0.0f;
        m_windowWeightBuffer[static_cast<size_t>(rPos)] = 0.0f;

        m_outReadPos = (m_outReadPos + 1) & m_bufferMask;
        return outSample;
    }

private:
    void synthesizeNextGrain()
    {
        const float theoreticalAnalysisOffset = m_virtualSynthesisPos;
        m_virtualSynthesisPos += static_cast<float>(m_hopSize) * (m_currentRatio - 1.0f);

        const float maxAllowedDrift = static_cast<float>(m_grainSize);
        m_virtualSynthesisPos = std::clamp(m_virtualSynthesisPos, -maxAllowedDrift, maxAllowedDrift);

        const int nominalReadAnchor = (m_inWritePos - 1 - m_grainSize - static_cast<int>(theoreticalAnalysisOffset)) & m_bufferMask;

        int bestDelta = 0;
        float bestCorr = -1e9f;

        for (int delta = -m_searchRange; delta <= m_searchRange; delta += 2)
        {
            float corr = 0.0f;
            for (int k = 0; k < m_searchRange; k += 4)
            {
                const int idxRef = (nominalReadAnchor + m_lastBestOffset + k) & m_bufferMask;
                const int idxCand = (nominalReadAnchor + delta + k) & m_bufferMask;
                corr += m_inputRingBuffer[static_cast<size_t>(idxRef)] * m_inputRingBuffer[static_cast<size_t>(idxCand)];
            }

            if (corr > bestCorr)
            {
                bestCorr = corr;
                bestDelta = delta;
            }
        }

        m_lastBestOffset = bestDelta;
        const int finalReadPos = (nominalReadAnchor + bestDelta) & m_bufferMask;

        for (int i = 0; i < m_grainSize; ++i)
        {
            const int inIdx = (finalReadPos + i) & m_bufferMask;
            const int outIdx = (m_outReadPos + i) & m_bufferMask;

            const float win = m_hanningWindow[static_cast<size_t>(i)];
            m_outputAccumBuffer[static_cast<size_t>(outIdx)] += m_inputRingBuffer[static_cast<size_t>(inIdx)] * win;
            m_windowWeightBuffer[static_cast<size_t>(outIdx)] += win;
        }
    }

    float m_sampleRate = 48000.0f;
    int m_maxBlockSize = 512;
    static constexpr int kMaxGrain = 256;
    int m_grainSize = 64;
    int m_hopSize = 16;
    int m_searchRange = 16;

    int m_bufferSize = 2048;
    int m_bufferMask = 2047;

    std::vector<float> m_inputRingBuffer;
    std::vector<float> m_outputAccumBuffer;
    std::vector<float> m_windowWeightBuffer;
    std::vector<float> m_hanningWindow;

    int m_inWritePos = 0;
    int m_outReadPos = 0;
    int m_hopSampleCounter = 0;

    float m_targetRatio = 1.0f;
    float m_currentRatio = 1.0f;
    float m_virtualSynthesisPos = 0.0f;
    int m_lastBestOffset = 0;
};
} // namespace CrayDSP
