#pragma once
#include <cmath>
#include <array>
#include <vector>
#include <string>
#include <algorithm>

namespace CrayDSP
{
class ScaleQuantizer
{
public:
    enum class ScaleType { Major, NaturalMinor, HarmonicMinor, MelodicMinor, Chromatic, PentatonicMajor, PentatonicMinor, Blues, Custom };
    enum class ScaleMode { Strict, Safe };

    ScaleQuantizer() { updateAllowedNotesMask(); }

    void setA4Reference(float a4Hz) { m_a4 = std::clamp(a4Hz, 415.0f, 466.0f); }

    void setKey(int keyIndex)
    {
        const int k = ((keyIndex % 12) + 12) % 12;
        if (k != m_keyIndex) { m_keyIndex = k; updateAllowedNotesMask(); }
    }

    void setScaleType(ScaleType type)
    {
        if (type != m_scaleType) { m_scaleType = type; updateAllowedNotesMask(); }
    }

    void setScaleMode(ScaleMode mode)
    {
        m_scaleMode = mode;
        m_hysteresisCents = (mode == ScaleMode::Strict) ? 12.0f : 24.0f;
    }

    void setCustomNoteState(int noteIndex, bool enabled)
    {
        if (noteIndex >= 0 && noteIndex < 12)
        {
            m_customMask[static_cast<size_t>(noteIndex)] = enabled;
            if (m_scaleType == ScaleType::Custom)
                m_allowedNotes = m_customMask;
        }
    }

    void setFlexTune(float flex) { m_flexTune = std::clamp(flex, 0.0f, 100.0f); }
    void setHysteresisCents(float cents) { m_hysteresisCents = std::clamp(cents, 0.0f, 50.0f); }

    struct QuantizeResult
    {
        int detectedNoteIndex = 0;
        int detectedMidi = 60;
        int targetMidi = 60;
        int targetNoteIndex = 0;
        float inputFrequencyHz = 440.0f;
        float targetFrequencyHz = 440.0f;
        float centsDeviation = 0.0f;
        float targetCentsShift = 0.0f;
        bool isAllowedNote = true;
    };

    QuantizeResult quantize(float inputFrequencyHz, float confidence, bool isVoiced)
    {
        QuantizeResult out;
        out.inputFrequencyHz = inputFrequencyHz;

        if (!isVoiced || inputFrequencyHz < 20.0f || !std::isfinite(inputFrequencyHz))
        {
            out.targetFrequencyHz = inputFrequencyHz;
            return out;
        }

        const float fractionalMidi = 69.0f + 12.0f * std::log2(std::max(1.0f, inputFrequencyHz) / m_a4);
        const int nearestChromaticMidi = static_cast<int>(std::round(fractionalMidi));
        out.detectedMidi = nearestChromaticMidi;
        out.detectedNoteIndex = ((nearestChromaticMidi % 12) + 12) % 12;
        out.centsDeviation = (fractionalMidi - static_cast<float>(nearestChromaticMidi)) * 100.0f;

        int bestAllowedMidi = nearestChromaticMidi;
        float minCentsDist = 1e6f;

        for (int offset = -7; offset <= 7; ++offset)
        {
            const int candidateMidi = nearestChromaticMidi + offset;
            const int noteClass = ((candidateMidi % 12) + 12) % 12;

            if (m_allowedNotes[static_cast<size_t>(noteClass)])
            {
                const float dist = std::abs((fractionalMidi - static_cast<float>(candidateMidi)) * 100.0f);
                if (dist < minCentsDist)
                {
                    minCentsDist = dist;
                    bestAllowedMidi = candidateMidi;
                }
            }
        }

        // Histeresis anti-vibracion en la frontera entre notas
        if (m_lastTargetMidi >= 0 && bestAllowedMidi != m_lastTargetMidi)
        {
            const int prevClass = ((m_lastTargetMidi % 12) + 12) % 12;
            if (m_allowedNotes[static_cast<size_t>(prevClass)])
            {
                const float distToPrev = std::abs((fractionalMidi - static_cast<float>(m_lastTargetMidi)) * 100.0f);
                if (distToPrev < minCentsDist + m_hysteresisCents)
                    bestAllowedMidi = m_lastTargetMidi;
            }
        }

        m_lastTargetMidi = bestAllowedMidi;
        out.targetMidi = bestAllowedMidi;
        out.targetNoteIndex = ((bestAllowedMidi % 12) + 12) % 12;
        out.targetFrequencyHz = m_a4 * std::pow(2.0f, (static_cast<float>(bestAllowedMidi) - 69.0f) / 12.0f);
        out.isAllowedNote = m_allowedNotes[static_cast<size_t>(out.detectedNoteIndex)];

        const float deviationToTarget = (fractionalMidi - static_cast<float>(bestAllowedMidi)) * 100.0f;
        float desiredShift = -deviationToTarget;

        if (m_flexTune > 0.0f)
        {
            const float deadbandCents = m_flexTune * 0.25f;
            if (std::abs(deviationToTarget) < deadbandCents)
                desiredShift = 0.0f;
            else if (desiredShift > 0.0f)
                desiredShift -= deadbandCents;
            else
                desiredShift += deadbandCents;
        }

        if (m_scaleMode == ScaleMode::Safe)
        {
            const float confidenceFactor = std::clamp((confidence - 0.35f) / 0.5f, 0.0f, 1.0f);
            desiredShift *= confidenceFactor;
        }

        out.targetCentsShift = desiredShift;
        return out;
    }

    void reset() { m_lastTargetMidi = -1; }

    const std::array<bool, 12>& getAllowedNotes() const { return m_allowedNotes; }

private:
    void updateAllowedNotesMask()
    {
        std::vector<int> intervals;
        switch (m_scaleType)
        {
            case ScaleType::Major:           intervals = { 0, 2, 4, 5, 7, 9, 11 }; break;
            case ScaleType::NaturalMinor:    intervals = { 0, 2, 3, 5, 7, 8, 10 }; break;
            case ScaleType::HarmonicMinor:   intervals = { 0, 2, 3, 5, 7, 8, 11 }; break;
            case ScaleType::MelodicMinor:    intervals = { 0, 2, 3, 5, 7, 9, 11 }; break;
            case ScaleType::Chromatic:       intervals = { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11 }; break;
            case ScaleType::PentatonicMajor: intervals = { 0, 2, 4, 7, 9 }; break;
            case ScaleType::PentatonicMinor: intervals = { 0, 3, 5, 7, 10 }; break;
            case ScaleType::Blues:           intervals = { 0, 3, 5, 6, 7, 10 }; break;
            case ScaleType::Custom:          m_allowedNotes = m_customMask; return;
        }

        m_allowedNotes.fill(false);
        for (int interval : intervals)
            m_allowedNotes[static_cast<size_t>((m_keyIndex + interval) % 12)] = true;
    }

    float m_a4 = 440.0f;
    int m_keyIndex = 0;
    ScaleType m_scaleType = ScaleType::NaturalMinor;
    ScaleMode m_scaleMode = ScaleMode::Strict;

    std::array<bool, 12> m_allowedNotes { false };
    std::array<bool, 12> m_customMask { true, false, true, true, false, true, false, true, true, false, true, false };

    float m_hysteresisCents = 12.0f;
    float m_flexTune = 0.0f;
    int m_lastTargetMidi = -1;
};
} // namespace CrayDSP
