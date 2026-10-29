#include "PluginProcessor.h"
#include "PluginEditor.h"

CrayLdrRealtimeTuneAudioProcessor::CrayLdrRealtimeTuneAudioProcessor()
     : AudioProcessor (BusesProperties()
                       .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
                       .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
       parameters (*this, nullptr, juce::Identifier ("CrayLdrParams"), createParameterLayout())
{
    // Escala "Custom" por defecto: C menor natural
    const bool defaults[12] = { true, false, true, true, false, true, false, true, true, false, true, false };
    for (size_t i = 0; i < 12; ++i)
        m_customNotes[i].store (defaults[i]);

    setLatencySamples (112);
}

CrayLdrRealtimeTuneAudioProcessor::~CrayLdrRealtimeTuneAudioProcessor() {}

juce::AudioProcessorValueTreeState::ParameterLayout CrayLdrRealtimeTuneAudioProcessor::createParameterLayout()
{
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> params;

    auto addFloat = [&params](const char* id, const char* name, float lo, float hi, float step, float def)
    {
        params.push_back (std::make_unique<juce::AudioParameterFloat> (
            juce::ParameterID { id, 1 }, name, juce::NormalisableRange<float> (lo, hi, step), def));
    };

    addFloat ("retuneSpeed", "Retune Speed", 0.0f, 100.0f, 0.1f, 0.0f);
    addFloat ("humanize", "Humanize", 0.0f, 100.0f, 0.5f, 10.0f);
    addFloat ("flexTune", "Flex Tune", 0.0f, 100.0f, 0.5f, 0.0f);
    addFloat ("attack", "Attack", 0.0f, 100.0f, 0.5f, 0.0f);
    addFloat ("release", "Release", 0.0f, 100.0f, 0.5f, 5.0f);
    addFloat ("correctionAmount", "Correction Amount", 0.0f, 100.0f, 1.0f, 100.0f);

    params.push_back (std::make_unique<juce::AudioParameterBool> (juce::ParameterID { "formantPreserve", 1 }, "Formant Preserve", true));
    params.push_back (std::make_unique<juce::AudioParameterChoice> (juce::ParameterID { "trackingMode", 1 }, "Tracking Mode", juce::StringArray { "FAST", "STABLE" }, 0));
    params.push_back (std::make_unique<juce::AudioParameterChoice> (juce::ParameterID { "scaleMode", 1 }, "Scale Mode", juce::StringArray { "STRICT", "SAFE" }, 0));
    params.push_back (std::make_unique<juce::AudioParameterChoice> (juce::ParameterID { "vocalRange", 1 }, "Vocal Range", juce::StringArray { "LOW", "MID", "HIGH" }, 1));

    params.push_back (std::make_unique<juce::AudioParameterChoice> (juce::ParameterID { "key", 1 }, "Key",
        juce::StringArray { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" }, 0));
    params.push_back (std::make_unique<juce::AudioParameterChoice> (juce::ParameterID { "scale", 1 }, "Scale",
        juce::StringArray { "Major", "Natural Minor", "Harmonic Minor", "Melodic Minor", "Chromatic", "Pentatonic Major", "Pentatonic Minor", "Blues", "Custom" }, 1));

    params.push_back (std::make_unique<juce::AudioParameterChoice> (juce::ParameterID { "mode", 1 }, "Mode", juce::StringArray { "CLASSIC", "MODERN" }, 0));

    addFloat ("inputGain", "Input Gain", -24.0f, 12.0f, 0.1f, 0.0f);
    addFloat ("outputGain", "Output Gain", -24.0f, 12.0f, 0.1f, 0.0f);
    addFloat ("mix", "Mix", 0.0f, 100.0f, 1.0f, 100.0f);
    params.push_back (std::make_unique<juce::AudioParameterBool> (juce::ParameterID { "bypass", 1 }, "Bypass", false));

    return { params.begin(), params.end() };
}

const juce::String CrayLdrRealtimeTuneAudioProcessor::getName() const { return JucePlugin_Name; }
bool CrayLdrRealtimeTuneAudioProcessor::acceptsMidi() const { return false; }
bool CrayLdrRealtimeTuneAudioProcessor::producesMidi() const { return false; }
bool CrayLdrRealtimeTuneAudioProcessor::isMidiEffect() const { return false; }
double CrayLdrRealtimeTuneAudioProcessor::getTailLengthSeconds() const { return 0.0; }
int CrayLdrRealtimeTuneAudioProcessor::getNumPrograms() { return 1; }
int CrayLdrRealtimeTuneAudioProcessor::getCurrentProgram() { return 0; }
void CrayLdrRealtimeTuneAudioProcessor::setCurrentProgram (int) {}
const juce::String CrayLdrRealtimeTuneAudioProcessor::getProgramName (int) { return {}; }
void CrayLdrRealtimeTuneAudioProcessor::changeProgramName (int, const juce::String&) {}

bool CrayLdrRealtimeTuneAudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    if (layouts.getMainOutputChannelSet() != juce::AudioChannelSet::mono()
     && layouts.getMainOutputChannelSet() != juce::AudioChannelSet::stereo())
        return false;

    return layouts.getMainOutputChannelSet() == layouts.getMainInputChannelSet();
}

void CrayLdrRealtimeTuneAudioProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    m_currentSampleRate = sampleRate;

    m_pitchDetector.prepare (sampleRate, samplesPerBlock);
    m_pitchShifterL.prepare (sampleRate, samplesPerBlock);
    m_pitchShifterR.prepare (sampleRate, samplesPerBlock);
    m_quantizer.reset();

    const double smoothTimeSec = 0.02;
    m_inputGainSmoothed.reset (sampleRate, smoothTimeSec);
    m_outputGainSmoothed.reset (sampleRate, smoothTimeSec);
    m_mixSmoothed.reset (sampleRate, smoothTimeSec);
    m_bypassSmoothed.reset (sampleRate, smoothTimeSec);

    // Arrancar en el valor real de los parametros (evita rampa desde 0 al iniciar)
    m_inputGainSmoothed.setCurrentAndTargetValue (juce::Decibels::decibelsToGain (parameters.getRawParameterValue ("inputGain")->load()));
    m_outputGainSmoothed.setCurrentAndTargetValue (juce::Decibels::decibelsToGain (parameters.getRawParameterValue ("outputGain")->load()));
    m_mixSmoothed.setCurrentAndTargetValue (parameters.getRawParameterValue ("mix")->load() / 100.0f);
    m_bypassSmoothed.setCurrentAndTargetValue (parameters.getRawParameterValue ("bypass")->load() > 0.5f ? 1.0f : 0.0f);

    m_envelopeShiftCents = 0.0f;
    m_humanizeFilteredCents = 0.0f;

    applyMode (static_cast<int> (parameters.getRawParameterValue ("mode")->load()));
}

void CrayLdrRealtimeTuneAudioProcessor::applyMode (int modeIdx)
{
    // Granos mas largos reducen los saltos de fase del pitch shifter. La latencia declarada es grain / 2.
    // CLASSIC prioriza respuesta; MODERN usa un grano algo mayor para una transicion mas suave.
    const int grain = (modeIdx == 0) ? 224 : 256;
    m_pitchShifterL.setGrainSize (grain);
    m_pitchShifterR.setGrainSize (grain);
    m_envelopeShiftCents = 0.0f;
    m_humanizeFilteredCents = 0.0f;
    m_currentMode = modeIdx;
    setLatencySamples (m_pitchShifterL.getLatencySamples());
}

void CrayLdrRealtimeTuneAudioProcessor::releaseResources()
{
    m_pitchDetector.reset();
    m_pitchShifterL.reset();
    m_pitchShifterR.reset();
    m_quantizer.reset();
}

void CrayLdrRealtimeTuneAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;
    const int totalNumInputChannels  = getTotalNumInputChannels();
    const int totalNumOutputChannels = getTotalNumOutputChannels();

    for (int i = totalNumInputChannels; i < totalNumOutputChannels; ++i)
        buffer.clear (i, 0, buffer.getNumSamples());

    const int numSamples = buffer.getNumSamples();
    if (numSamples == 0 || totalNumInputChannels == 0)
        return;

    auto p = [this] (const char* id) { return parameters.getRawParameterValue (id)->load(); };

    const float rawRetuneSpeed = p ("retuneSpeed");
    const int modeIdx = static_cast<int> (p ("mode"));
    if (modeIdx != m_currentMode)
        applyMode (modeIdx);

    // CLASSIC = correccion directa (Retune Speed manda). MODERN = Humanize y Flex-Tune activos.
    const float rawHumanize = (modeIdx == 1) ? p ("humanize") : 0.0f;
    const float rawFlexTune = (modeIdx == 1) ? p ("flexTune") : 0.0f;
    const float rawAttack = p ("attack");
    const float rawRelease = p ("release");
    const float rawCorrectionAmount = p ("correctionAmount") / 100.0f;

    const int trackingModeIdx = static_cast<int> (p ("trackingMode"));
    const int scaleModeIdx = static_cast<int> (p ("scaleMode"));
    const int vocalRangeIdx = static_cast<int> (p ("vocalRange"));
    const int keyIdx = static_cast<int> (p ("key"));
    const int scaleIdx = juce::jlimit (0, 8, static_cast<int> (p ("scale")));

    m_pitchDetector.setTrackingMode (trackingModeIdx == 0 ? CrayDSP::YinPitchDetector::TrackingMode::Fast
                                                          : CrayDSP::YinPitchDetector::TrackingMode::Stable);
    m_pitchDetector.setRangePreset (vocalRangeIdx == 0 ? CrayDSP::YinPitchDetector::RangePreset::Low
                                   : (vocalRangeIdx == 1 ? CrayDSP::YinPitchDetector::RangePreset::Mid
                                                         : CrayDSP::YinPitchDetector::RangePreset::High));

    for (int n = 0; n < 12; ++n)
        m_quantizer.setCustomNoteState (n, m_customNotes[static_cast<size_t> (n)].load (std::memory_order_relaxed));

    m_quantizer.setKey (keyIdx);
    m_quantizer.setScaleType (static_cast<CrayDSP::ScaleQuantizer::ScaleType> (scaleIdx));
    m_quantizer.setScaleMode (scaleModeIdx == 0 ? CrayDSP::ScaleQuantizer::ScaleMode::Strict
                                                : CrayDSP::ScaleQuantizer::ScaleMode::Safe);
    m_quantizer.setFlexTune (rawFlexTune);

    m_inputGainSmoothed.setTargetValue (juce::Decibels::decibelsToGain (p ("inputGain")));
    m_outputGainSmoothed.setTargetValue (juce::Decibels::decibelsToGain (p ("outputGain")));
    m_mixSmoothed.setTargetValue (p ("mix") / 100.0f);
    m_bypassSmoothed.setTargetValue (p ("bypass") > 0.5f ? 1.0f : 0.0f);

    const float sr = static_cast<float> (m_currentSampleRate);
    const float speedNormalized = rawRetuneSpeed / 100.0f;
    const float retuneTimeSec = 0.0005f + (speedNormalized * speedNormalized) * 0.120f;
    const float attackTimeSec = retuneTimeSec + (rawAttack / 100.0f) * 0.080f;
    const float releaseTimeSec = 0.005f + (rawRelease / 100.0f) * 0.100f;

    const float attackCoeff = std::exp (-1.0f / (attackTimeSec * sr));
    const float releaseCoeff = std::exp (-1.0f / (releaseTimeSec * sr));
    const float humanizeCoeff = std::exp (-1.0f / (0.040f * sr));

    auto* channelDataL = buffer.getWritePointer (0);
    auto* channelDataR = totalNumInputChannels > 1 ? buffer.getWritePointer (1) : nullptr;

    for (int i = 0; i < numSamples; ++i)
    {
        const float currentInGain = m_inputGainSmoothed.getNextValue();
        const float currentOutGain = m_outputGainSmoothed.getNextValue();
        const float currentMix = m_mixSmoothed.getNextValue();
        const float currentBypass = m_bypassSmoothed.getNextValue();

        const float inL = channelDataL[i] * currentInGain;
        const float inR = (channelDataR != nullptr) ? (channelDataR[i] * currentInGain) : inL;

        const float monoAnalysis = 0.5f * (inL + inR);
        const auto pitchRes = m_pitchDetector.processSample (monoAnalysis);

        float targetShiftCents = 0.0f;
        if (pitchRes.isVoiced)
        {
            const auto quantRes = m_quantizer.quantize (pitchRes.frequencyHz, pitchRes.confidence, pitchRes.isVoiced);
            targetShiftCents = quantRes.targetCentsShift * rawCorrectionAmount;

            if (rawHumanize > 0.0f)
            {
                m_humanizeFilteredCents = (m_humanizeFilteredCents * humanizeCoeff) + (targetShiftCents * (1.0f - humanizeCoeff));
                const float preserveRatio = (rawHumanize / 100.0f) * 0.45f;
                targetShiftCents = (1.0f - preserveRatio) * targetShiftCents + preserveRatio * m_humanizeFilteredCents;
            }

            if (std::abs (targetShiftCents) > std::abs (m_envelopeShiftCents))
                m_envelopeShiftCents = (m_envelopeShiftCents * attackCoeff) + (targetShiftCents * (1.0f - attackCoeff));
            else
                m_envelopeShiftCents = (m_envelopeShiftCents * releaseCoeff) + (targetShiftCents * (1.0f - releaseCoeff));

            m_visualDetectedFreq.store (pitchRes.frequencyHz, std::memory_order_relaxed);
            m_visualDetectedNote.store (quantRes.detectedNoteIndex, std::memory_order_relaxed);
            m_visualTargetNote.store (quantRes.targetNoteIndex, std::memory_order_relaxed);
            m_visualCentsDeviation.store (quantRes.centsDeviation, std::memory_order_relaxed);
            m_visualConfidence.store (pitchRes.confidence, std::memory_order_relaxed);
            m_visualIsVoiced.store (true, std::memory_order_relaxed);
            m_visualIsAllowed.store (quantRes.isAllowedNote, std::memory_order_relaxed);
        }
        else
        {
            m_envelopeShiftCents *= releaseCoeff;
            m_visualIsVoiced.store (false, std::memory_order_relaxed);
            m_visualConfidence.store (0.0f, std::memory_order_relaxed);
        }

        const float pitchRatio = std::pow (2.0f, m_envelopeShiftCents / 1200.0f);
        m_pitchShifterL.setShiftRatio (pitchRatio);
        m_pitchShifterR.setShiftRatio (pitchRatio);

        const float wetL = m_pitchShifterL.processSample (inL);
        const float wetR = (channelDataR != nullptr) ? m_pitchShifterR.processSample (inR) : wetL;

        const float outL = ((1.0f - currentMix) * inL + currentMix * wetL) * currentOutGain;
        const float outR = ((1.0f - currentMix) * inR + currentMix * wetR) * currentOutGain;

        channelDataL[i] = (1.0f - currentBypass) * outL + currentBypass * channelDataL[i];
        if (channelDataR != nullptr)
            channelDataR[i] = (1.0f - currentBypass) * outR + currentBypass * channelDataR[i];
    }
}

juce::AudioProcessorEditor* CrayLdrRealtimeTuneAudioProcessor::createEditor()
{
    return new CrayLdrRealtimeTuneAudioProcessorEditor (*this);
}

bool CrayLdrRealtimeTuneAudioProcessor::hasEditor() const { return true; }

void CrayLdrRealtimeTuneAudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    auto state = parameters.copyState();

    int mask = 0;
    for (int i = 0; i < 12; ++i)
        if (m_customNotes[static_cast<size_t> (i)].load())
            mask |= (1 << i);
    state.setProperty ("customNotes", mask, nullptr);

    std::unique_ptr<juce::XmlElement> xml (state.createXml());
    copyXmlToBinary (*xml, destData);
}

void CrayLdrRealtimeTuneAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    std::unique_ptr<juce::XmlElement> xmlState (getXmlFromBinary (data, sizeInBytes));
    if (xmlState != nullptr && xmlState->hasTagName (parameters.state.getType()))
    {
        auto tree = juce::ValueTree::fromXml (*xmlState);

        if (tree.hasProperty ("customNotes"))
        {
            const int mask = static_cast<int> (tree.getProperty ("customNotes"));
            for (int i = 0; i < 12; ++i)
                m_customNotes[static_cast<size_t> (i)].store ((mask & (1 << i)) != 0);
        }

        parameters.replaceState (tree);
    }
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new CrayLdrRealtimeTuneAudioProcessor();
}
