#pragma once
#include <array>
#include <memory>
#include <juce_gui_basics/juce_gui_basics.h>
#include "PluginProcessor.h"
#include "CrayCustomLookAndFeel.h"

class CrayLdrRealtimeTuneAudioProcessorEditor : public juce::AudioProcessorEditor,
                                                private juce::Timer
{
public:
    explicit CrayLdrRealtimeTuneAudioProcessorEditor (CrayLdrRealtimeTuneAudioProcessor&);
    ~CrayLdrRealtimeTuneAudioProcessorEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    using SliderAtt = juce::AudioProcessorValueTreeState::SliderAttachment;
    using ComboAtt  = juce::AudioProcessorValueTreeState::ComboBoxAttachment;
    using ButtonAtt = juce::AudioProcessorValueTreeState::ButtonAttachment;

    void timerCallback() override;

    CrayLdrRealtimeTuneAudioProcessor& audioProcessor;
    CrayCustomLookAndFeel customLookAndFeel;

    juce::Slider retuneSpeedSlider, humanizeSlider, flexTuneSlider, attackSlider, releaseSlider,
                 correctionAmountSlider, inputGainSlider, outputGainSlider, mixSlider;

    std::unique_ptr<SliderAtt> retuneSpeedAttachment, humanizeAttachment, flexTuneAttachment, attackAttachment,
                               releaseAttachment, correctionAmountAttachment, inputGainAttachment,
                               outputGainAttachment, mixAttachment;

    juce::ComboBox keyComboBox, scaleComboBox, modeComboBox, trackingModeComboBox, scaleModeComboBox, vocalRangeComboBox;
    juce::ToggleButton formantPreserveButton { "FORMANT PRESERVE" };
    juce::ToggleButton bypassButton { "BYPASS" };

    std::unique_ptr<ComboAtt> modeAttachment, keyAttachment, scaleAttachment, trackingModeAttachment, scaleModeAttachment, vocalRangeAttachment;
    std::unique_ptr<ButtonAtt> formantPreserveAttachment, bypassAttachment;

    std::array<juce::TextButton, 12> noteButtons;

    CrayLdrRealtimeTuneAudioProcessor::VisualStatus m_cachedStatus;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (CrayLdrRealtimeTuneAudioProcessorEditor)
};
