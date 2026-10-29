#include "PluginEditor.h"

namespace
{
    const char* const kNoteNames[12] = { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" };
}

CrayLdrRealtimeTuneAudioProcessorEditor::CrayLdrRealtimeTuneAudioProcessorEditor (CrayLdrRealtimeTuneAudioProcessor& p)
    : AudioProcessorEditor (&p), audioProcessor (p)
{
    setLookAndFeel (&customLookAndFeel);

    auto setupSlider = [this] (juce::Slider& sl, const juce::String& name)
    {
        sl.setName (name);
        sl.setSliderStyle (juce::Slider::RotaryVerticalDrag);
        sl.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 60, 16);
        addAndMakeVisible (sl);
    };

    setupSlider (retuneSpeedSlider, "RETUNE");
    setupSlider (humanizeSlider, "HUMANIZE");
    setupSlider (flexTuneSlider, "FLEX TUNE");
    setupSlider (attackSlider, "ATTACK");
    setupSlider (releaseSlider, "RELEASE");
    setupSlider (correctionAmountSlider, "AMOUNT");
    setupSlider (inputGainSlider, "IN GAIN");
    setupSlider (outputGainSlider, "OUT GAIN");
    setupSlider (mixSlider, "MIX");

    auto& apvts = p.parameters;
    retuneSpeedAttachment = std::make_unique<SliderAtt> (apvts, "retuneSpeed", retuneSpeedSlider);
    humanizeAttachment = std::make_unique<SliderAtt> (apvts, "humanize", humanizeSlider);
    flexTuneAttachment = std::make_unique<SliderAtt> (apvts, "flexTune", flexTuneSlider);
    attackAttachment = std::make_unique<SliderAtt> (apvts, "attack", attackSlider);
    releaseAttachment = std::make_unique<SliderAtt> (apvts, "release", releaseSlider);
    correctionAmountAttachment = std::make_unique<SliderAtt> (apvts, "correctionAmount", correctionAmountSlider);
    inputGainAttachment = std::make_unique<SliderAtt> (apvts, "inputGain", inputGainSlider);
    outputGainAttachment = std::make_unique<SliderAtt> (apvts, "outputGain", outputGainSlider);
    mixAttachment = std::make_unique<SliderAtt> (apvts, "mix", mixSlider);

    keyComboBox.addItemList (juce::StringArray { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" }, 1);
    scaleComboBox.addItemList (juce::StringArray { "Major", "Natural Minor", "Harmonic Minor", "Melodic Minor", "Chromatic",
                                                   "Pentatonic Major", "Pentatonic Minor", "Blues", "Custom" }, 1);
    trackingModeComboBox.addItemList (juce::StringArray { "FAST", "STABLE" }, 1);
    scaleModeComboBox.addItemList (juce::StringArray { "STRICT", "SAFE" }, 1);
    vocalRangeComboBox.addItemList (juce::StringArray { "LOW (65-350Hz)", "MID (100-600Hz)", "HIGH (150-1000Hz)" }, 1);

    for (auto* c : { &keyComboBox, &scaleComboBox, &trackingModeComboBox, &scaleModeComboBox, &vocalRangeComboBox })
        addAndMakeVisible (*c);

    modeComboBox.addItemList (juce::StringArray { "CLASSIC", "MODERN" }, 1);
    addAndMakeVisible (modeComboBox);
    modeAttachment = std::make_unique<ComboAtt> (apvts, "mode", modeComboBox);

    keyAttachment = std::make_unique<ComboAtt> (apvts, "key", keyComboBox);
    scaleAttachment = std::make_unique<ComboAtt> (apvts, "scale", scaleComboBox);
    trackingModeAttachment = std::make_unique<ComboAtt> (apvts, "trackingMode", trackingModeComboBox);
    scaleModeAttachment = std::make_unique<ComboAtt> (apvts, "scaleMode", scaleModeComboBox);
    vocalRangeAttachment = std::make_unique<ComboAtt> (apvts, "vocalRange", vocalRangeComboBox);

    addAndMakeVisible (formantPreserveButton);
    formantPreserveAttachment = std::make_unique<ButtonAtt> (apvts, "formantPreserve", formantPreserveButton);
    addAndMakeVisible (bypassButton);
    bypassAttachment = std::make_unique<ButtonAtt> (apvts, "bypass", bypassButton);

    for (int i = 0; i < 12; ++i)
    {
        auto& b = noteButtons[static_cast<size_t> (i)];
        b.setButtonText (kNoteNames[i]);
        b.setClickingTogglesState (true);
        b.setToggleState (audioProcessor.isCustomNoteEnabled (i), juce::dontSendNotification);
        b.onClick = [this, i]() { audioProcessor.setCustomNoteEnabled (i, noteButtons[static_cast<size_t> (i)].getToggleState()); };
        addAndMakeVisible (b);
    }

    setSize (780, 520);
    startTimerHz (30);
}

CrayLdrRealtimeTuneAudioProcessorEditor::~CrayLdrRealtimeTuneAudioProcessorEditor()
{
    stopTimer();
    setLookAndFeel (nullptr);
}

void CrayLdrRealtimeTuneAudioProcessorEditor::timerCallback()
{
    m_cachedStatus = audioProcessor.getVisualStatus();
    repaint();
}

void CrayLdrRealtimeTuneAudioProcessorEditor::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colour (0xff0a0c0f));

    g.setColour (juce::Colour (0xfffacc15));
    g.fillRect (0, 0, getWidth(), 3);

    g.setFont (juce::Font (18.0f, juce::Font::bold));
    g.setColour (juce::Colours::white);
    g.drawText ("CRAY LDR", 18, 12, 100, 24, juce::Justification::left);

    g.setColour (juce::Colour (0xfffacc15));
    g.drawText ("REALTIME TUNE", 112, 12, 160, 24, juce::Justification::left);

    g.setFont (juce::Font (11.0f));
    g.setColour (juce::Colour (0xff71717a));
    g.drawText (juce::String::fromUTF8 ("VST3 x64 \xE2\x80\xA2 Low-Latency Vocal Engine"), 280, 16, 260, 20, juce::Justification::left);

    const auto hudRect = juce::Rectangle<int> (18, 48, getWidth() - 36, 120);
    g.setColour (juce::Colour (0xff12151b));
    g.fillRoundedRectangle (hudRect.toFloat(), 8.0f);
    g.setColour (juce::Colour (0xff27272a));
    g.drawRoundedRectangle (hudRect.toFloat(), 8.0f, 1.0f);

    // Nota detectada
    g.setColour (juce::Colour (0xff1c1f26));
    g.fillRoundedRectangle (34.0f, 60.0f, 72.0f, 72.0f, 6.0f);
    g.setFont (juce::Font (26.0f, juce::Font::bold));
    g.setColour (m_cachedStatus.isVoiced ? juce::Colours::white : juce::Colour (0xff52525b));
    g.drawText (kNoteNames[m_cachedStatus.detectedNoteIndex], 34, 64, 72, 40, juce::Justification::centred);
    g.setFont (juce::Font (10.0f));
    g.setColour (juce::Colour (0xffa1a1aa));
    g.drawText (juce::String (m_cachedStatus.detectedFreqHz, 1) + " Hz", 34, 104, 72, 18, juce::Justification::centred);

    // Nota objetivo
    const float tx = static_cast<float> (getWidth() - 106);
    g.setColour (juce::Colour (0xff282109));
    g.fillRoundedRectangle (tx, 60.0f, 72.0f, 72.0f, 6.0f);
    g.setColour (juce::Colour (0xfffacc15));
    g.drawRoundedRectangle (tx, 60.0f, 72.0f, 72.0f, 6.0f, 1.5f);
    g.setFont (juce::Font (26.0f, juce::Font::bold));
    g.drawText (kNoteNames[m_cachedStatus.targetNoteIndex], getWidth() - 106, 64, 72, 40, juce::Justification::centred);
    g.setFont (juce::Font (10.0f, juce::Font::bold));
    g.drawText ("TARGET", getWidth() - 106, 104, 72, 18, juce::Justification::centred);

    // Medidor de cents
    const int meterX = 120;
    const int meterW = getWidth() - 240;
    const int meterY = 88;
    g.setColour (juce::Colour (0xff0d0f13));
    g.fillRect (meterX, meterY, meterW, 14);

    g.setColour (juce::Colour (0xff3f3f46));
    g.drawVerticalLine (meterX + meterW / 2, (float) meterY, (float) (meterY + 14));

    const float normShift = juce::jlimit (0.0f, 1.0f, (m_cachedStatus.centsDeviation + 50.0f) / 100.0f);
    const int needleX = meterX + static_cast<int> (normShift * (float) meterW);
    g.setColour (juce::Colour (0xfffacc15));
    g.fillRect (needleX - 2, meterY - 2, 4, 18);

    g.setFont (juce::Font (11.0f, juce::Font::bold));
    const juce::String centsText = juce::String (m_cachedStatus.centsDeviation > 0 ? "+" : "")
                                 + juce::String (static_cast<int> (m_cachedStatus.centsDeviation)) + " CENTS";
    g.drawText (centsText, meterX, meterY - 22, meterW, 18, juce::Justification::centred);

    // Etiquetas de los knobs
    g.setFont (juce::Font (10.0f, juce::Font::bold));
    g.setColour (juce::Colour (0xffa1a1aa));
    for (auto* s : { &retuneSpeedSlider, &humanizeSlider, &flexTuneSlider, &attackSlider, &releaseSlider,
                     &correctionAmountSlider, &inputGainSlider, &outputGainSlider, &mixSlider })
        g.drawText (s->getName(), s->getX(), s->getY() - 14, s->getWidth(), 14, juce::Justification::centred);
}

void CrayLdrRealtimeTuneAudioProcessorEditor::resized()
{
    modeComboBox.setBounds (getWidth() - 130, 14, 110, 24);
    keyComboBox.setBounds (20, 180, 70, 24);
    scaleComboBox.setBounds (95, 180, 130, 24);
    trackingModeComboBox.setBounds (235, 180, 90, 24);
    scaleModeComboBox.setBounds (330, 180, 90, 24);
    vocalRangeComboBox.setBounds (425, 180, 140, 24);
    formantPreserveButton.setBounds (575, 180, 110, 24);
    bypassButton.setBounds (getWidth() - 85, 180, 65, 24);

    const int btnW = (getWidth() - 40) / 12;
    for (int i = 0; i < 12; ++i)
        noteButtons[static_cast<size_t> (i)].setBounds (20 + i * btnW, 215, btnW - 4, 30);

    const int sliderY = 280;
    const int colW = (getWidth() - 40) / 6;

    retuneSpeedSlider.setBounds (20 + 0 * colW, sliderY, colW - 10, 85);
    humanizeSlider.setBounds (20 + 1 * colW, sliderY, colW - 10, 85);
    flexTuneSlider.setBounds (20 + 2 * colW, sliderY, colW - 10, 85);
    attackSlider.setBounds (20 + 3 * colW, sliderY, colW - 10, 85);
    releaseSlider.setBounds (20 + 4 * colW, sliderY, colW - 10, 85);
    correctionAmountSlider.setBounds (20 + 5 * colW, sliderY, colW - 10, 85);

    const int row2Y = 400;
    inputGainSlider.setBounds (20 + 1 * colW, row2Y, colW - 10, 85);
    mixSlider.setBounds (20 + 2 * colW, row2Y, colW - 10, 85);
    outputGainSlider.setBounds (20 + 3 * colW, row2Y, colW - 10, 85);
}
