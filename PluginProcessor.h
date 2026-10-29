#pragma once
#include <array>
#include <atomic>
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_dsp/juce_dsp.h>
#include "YinPitchDetector.h"
#include "ScaleQuantizer.h"
#include "WsolaPitchShifter.h"

class CrayLdrRealtimeTuneAudioProcessor : public juce::AudioProcessor
{
public:
    CrayLdrRealtimeTuneAudioProcessor();
    ~CrayLdrRealtimeTuneAudioProcessor() override;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;

    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;

    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override;

    const juce::String getName() const override;

    bool acceptsMidi() const override;
    bool producesMidi() const override;
    bool isMidiEffect() const override;
    double getTailLengthSeconds() const override;

    int getNumPrograms() override;
    int getCurrentProgram() override;
    void setCurrentProgram (int index) override;
    const juce::String getProgramName (int index) override;
    void changeProgramName (int index, const juce::String& newName) override;

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    struct VisualStatus
    {
        float detectedFreqHz = 130.81f;
        int detectedNoteIndex = 0;
        int targetNoteIndex = 0;
        float centsDeviation = 0.0f;
        float confidence = 0.0f;
        bool isVoiced = false;
        bool isAllowed = true;
    };

    VisualStatus getVisualStatus() const noexcept
    {
        VisualStatus s;
        s.detectedFreqHz = m_visualDetectedFreq.load (std::memory_order_relaxed);
        s.detectedNoteIndex = m_visualDetectedNote.load (std::memory_order_relaxed);
        s.targetNoteIndex = m_visualTargetNote.load (std::memory_order_relaxed);
        s.centsDeviation = m_visualCentsDeviation.load (std::memory_order_relaxed);
        s.confidence = m_visualConfidence.load (std::memory_order_relaxed);
        s.isVoiced = m_visualIsVoiced.load (std::memory_order_relaxed);
        s.isAllowed = m_visualIsAllowed.load (std::memory_order_relaxed);
        return s;
    }

    juce::AudioProcessorValueTreeState parameters;

    /** Llamado desde la GUI: se aplica al cuantizador en el siguiente bloque de audio. */
    void setCustomNoteEnabled (int noteIndex, bool enabled)
    {
        if (noteIndex >= 0 && noteIndex < 12)
            m_customNotes[static_cast<size_t> (noteIndex)].store (enabled, std::memory_order_relaxed);
    }

    bool isCustomNoteEnabled (int noteIndex) const
    {
        return m_customNotes[static_cast<size_t> (noteIndex)].load (std::memory_order_relaxed);
    }

private:
    juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();
    void applyMode (int modeIdx);
    int m_currentMode = 0;

    CrayDSP::YinPitchDetector m_pitchDetector;
    CrayDSP::ScaleQuantizer m_quantizer;
    CrayDSP::WsolaPitchShifter m_pitchShifterL;
    CrayDSP::WsolaPitchShifter m_pitchShifterR;

    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> m_inputGainSmoothed;
    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> m_outputGainSmoothed;
    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> m_mixSmoothed;
    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> m_bypassSmoothed;

    float m_envelopeShiftCents = 0.0f;
    float m_humanizeFilteredCents = 0.0f;

    std::array<std::atomic<bool>, 12> m_customNotes {};

    std::atomic<float> m_visualDetectedFreq { 130.81f };
    std::atomic<int> m_visualDetectedNote { 0 };
    std::atomic<int> m_visualTargetNote { 0 };
    std::atomic<float> m_visualCentsDeviation { 0.0f };
    std::atomic<float> m_visualConfidence { 0.0f };
    std::atomic<bool> m_visualIsVoiced { false };
    std::atomic<bool> m_visualIsAllowed { true };

    double m_currentSampleRate = 48000.0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (CrayLdrRealtimeTuneAudioProcessor)
};
