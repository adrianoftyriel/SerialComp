#include <JuceHeader.h>
#include "PluginProcessor.h"
#include "PluginEditor.h"

// ---------------------------------------------------------------------------
// Parameter layout
// ---------------------------------------------------------------------------
juce::AudioProcessorValueTreeState::ParameterLayout
SerialCompProcessor::createParameterLayout()
{
    juce::AudioProcessorValueTreeState::ParameterLayout layout;

    const juce::StringArray modelChoices =
        { "1176", "LA-2A", "Destressor", "DBX 160", "Neve 33609", "SSL G-Bus", "Manual" };

    for (int i = 0; i < MAX_STAGES; i++)
    {
        const juce::String px   = "s" + juce::String(i) + "_";
        const juce::String name = "Stage " + juce::String(i + 1) + " ";

        layout.add(std::make_unique<juce::AudioParameterBool>(
            juce::ParameterID(px + "enabled", 1),
            name + "Enabled",
            i == 0));

        layout.add(std::make_unique<juce::AudioParameterChoice>(
            juce::ParameterID(px + "model", 1),
            name + "Model",
            modelChoices,
            6));        // default: Manual

        layout.add(std::make_unique<juce::AudioParameterFloat>(
            juce::ParameterID(px + "threshold", 1),
            name + "Threshold",
            juce::NormalisableRange<float>(-60.0f, 0.0f, 0.1f),
            -18.0f));

        layout.add(std::make_unique<juce::AudioParameterFloat>(
            juce::ParameterID(px + "ratio", 1),
            name + "Ratio",
            juce::NormalisableRange<float>(1.0f, 40.0f, 0.01f, 0.4f),
            4.0f));

        layout.add(std::make_unique<juce::AudioParameterFloat>(
            juce::ParameterID(px + "attack", 1),
            name + "Attack",
            juce::NormalisableRange<float>(0.01f, 200.0f, 0.01f, 0.4f),
            10.0f));

        layout.add(std::make_unique<juce::AudioParameterFloat>(
            juce::ParameterID(px + "release", 1),
            name + "Release",
            juce::NormalisableRange<float>(5.0f, 5000.0f, 1.0f, 0.4f),
            200.0f));

        layout.add(std::make_unique<juce::AudioParameterFloat>(
            juce::ParameterID(px + "sat", 1),
            name + "Saturation",
            juce::NormalisableRange<float>(0.0f, 1.0f, 0.01f),
            0.0f));

        layout.add(std::make_unique<juce::AudioParameterFloat>(
            juce::ParameterID(px + "mix", 1),
            name + "Mix",
            juce::NormalisableRange<float>(0.0f, 1.0f, 0.01f),
            1.0f));
    }

    layout.add(std::make_unique<juce::AudioParameterInt>(
        juce::ParameterID("numStages", 1),
        "Number of Stages",
        1, MAX_STAGES, 1));

    layout.add(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID("inputGain", 1),
        "Input Gain",
        juce::NormalisableRange<float>(-18.0f, 18.0f, 0.1f),
        0.0f));

    layout.add(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID("makeupGain", 1),
        "Makeup Gain",
        juce::NormalisableRange<float>(-18.0f, 24.0f, 0.1f),
        0.0f));

    return layout;
}

// ---------------------------------------------------------------------------
// Constructor
// ---------------------------------------------------------------------------
SerialCompProcessor::SerialCompProcessor()
    : AudioProcessor(BusesProperties()
                         .withInput ("Input",  juce::AudioChannelSet::stereo(), true)
                         .withOutput("Output", juce::AudioChannelSet::stereo(), true)),
      apvts(*this, nullptr, "SerialCompState", createParameterLayout())
{
    cacheParamPointers();
}

// ---------------------------------------------------------------------------
// Cache APVTS parameter pointers (called once from constructor, non-RT)
// ---------------------------------------------------------------------------
void SerialCompProcessor::cacheParamPointers()
{
    for (int i = 0; i < MAX_STAGES; i++)
    {
        auto px = "s" + juce::String(i) + "_";
        auto& p = stageParams[i];
        p.enabled   = apvts.getRawParameterValue(px + "enabled");
        p.model     = apvts.getRawParameterValue(px + "model");
        p.threshold = apvts.getRawParameterValue(px + "threshold");
        p.ratio     = apvts.getRawParameterValue(px + "ratio");
        p.attack    = apvts.getRawParameterValue(px + "attack");
        p.release   = apvts.getRawParameterValue(px + "release");
        p.sat       = apvts.getRawParameterValue(px + "sat");
        p.mix       = apvts.getRawParameterValue(px + "mix");
    }
    numStagesParam  = apvts.getRawParameterValue("numStages");
    inputGainParam  = apvts.getRawParameterValue("inputGain");
    makeupGainParam = apvts.getRawParameterValue("makeupGain");
}

// ---------------------------------------------------------------------------
// Lifecycle
// ---------------------------------------------------------------------------
void SerialCompProcessor::prepareToPlay(double sampleRate, int samplesPerBlock)
{
    for (auto& stage : stages)
        stage.prepare(sampleRate, samplesPerBlock);

    currentSampleRate = sampleRate;
    prevInputGain     = juce::Decibels::decibelsToGain(inputGainParam->load());
    prevMakeupGain    = juce::Decibels::decibelsToGain(makeupGainParam->load());
    inputEnvLinear    = 0.0f;
    outputEnvLinear   = 0.0f;
}

void SerialCompProcessor::releaseResources()
{
    for (auto& stage : stages)
        stage.reset();
}

// ---------------------------------------------------------------------------
// Sync APVTS → CompressorStage  (zero allocation – uses cached pointers)
// ---------------------------------------------------------------------------
void SerialCompProcessor::syncStageParameters(int i)
{
    const auto& p = stageParams[i];
    auto& s = stages[i];

    s.setEnabled  (p.enabled->load()   > 0.5f);
    s.setModel    (static_cast<CompressorModel>(static_cast<int>(p.model->load())));
    s.setThreshold(p.threshold->load());
    s.setRatio    (p.ratio->load());
    s.setAttack   (p.attack->load());
    s.setRelease  (p.release->load());
    s.setSaturation(p.sat->load());
    s.setMix      (p.mix->load());
}

// ---------------------------------------------------------------------------
// processBlock – global input gain → serial stage chain → global makeup gain
// ---------------------------------------------------------------------------
void SerialCompProcessor::processBlock(juce::AudioBuffer<float>& buffer,
                                        juce::MidiBuffer& /*midi*/)
{
    juce::ScopedNoDenormals noDenormals;

    const int numSamples  = buffer.getNumSamples();
    const int numChannels = buffer.getNumChannels();

    if (numSamples == 0 || numChannels == 0) return;

    const int numStages = juce::jlimit(0, MAX_STAGES, static_cast<int>(numStagesParam->load()));

    // Meter ballistics in seconds, so they don't depend on the host's block size.
    const float blockSec = static_cast<float>(numSamples / currentSampleRate);
    const float attCoeff = 1.0f - std::exp(-blockSec / 0.010f);   // 10 ms rise
    const float relCoeff = std::exp(-blockSec / 0.300f);          // 300 ms fall

    // Mono sum used for the history display.
    const float* l = buffer.getReadPointer(0);
    const float* r = numChannels >= 2 ? buffer.getReadPointer(1) : nullptr;

    // --- Global input gain (ramped across the block to avoid zipper noise) ---
    const float inGain = juce::Decibels::decibelsToGain(inputGainParam->load());
    if (!juce::approximatelyEqual(inGain, prevInputGain) || !juce::approximatelyEqual(inGain, 1.0f))
        buffer.applyGainRamp(0, numSamples, prevInputGain, inGain);
    prevInputGain = inGain;

    // --- Capture input + measure input level ---
    float inputPeak = 0.0f;
    for (int s = 0; s < numSamples; s++)
        waveformInput.push(r ? (l[s] + r[s]) * 0.5f : l[s]);
    for (int ch = 0; ch < numChannels; ++ch)
        inputPeak = juce::jmax(inputPeak, buffer.getMagnitude(ch, 0, numSamples));

    inputEnvLinear = inputPeak > inputEnvLinear
        ? inputEnvLinear + attCoeff * (inputPeak - inputEnvLinear)
        : relCoeff * inputEnvLinear;
    inputLevelDB.store(juce::Decibels::gainToDecibels(inputEnvLinear, -96.0f));

    // --- Stage chain (stages past numStages are idled so their meters fall to zero) ---
    for (int i = 0; i < MAX_STAGES; i++)
    {
        syncStageParameters(i);

        if (i >= numStages)
            stages[i].setEnabled(false);

        stages[i].processBlock(buffer);

        if (i < numStages)
            for (int s = 0; s < numSamples; s++)
                waveformStages[i].push(r ? (l[s] + r[s]) * 0.5f : l[s]);
    }

    // --- Global makeup gain ---
    const float mkGain = juce::Decibels::decibelsToGain(makeupGainParam->load());
    if (!juce::approximatelyEqual(mkGain, prevMakeupGain) || !juce::approximatelyEqual(mkGain, 1.0f))
        buffer.applyGainRamp(0, numSamples, prevMakeupGain, mkGain);
    prevMakeupGain = mkGain;

    // --- Measure output level (after makeup gain) ---
    float outputPeak = 0.0f;
    for (int ch = 0; ch < numChannels; ++ch)
        outputPeak = juce::jmax(outputPeak, buffer.getMagnitude(ch, 0, numSamples));

    outputEnvLinear = outputPeak > outputEnvLinear
        ? outputEnvLinear + attCoeff * (outputPeak - outputEnvLinear)
        : relCoeff * outputEnvLinear;
    outputLevelDB.store(juce::Decibels::gainToDecibels(outputEnvLinear, -96.0f));
}

// ---------------------------------------------------------------------------
// Metering accessor (UI thread)
// ---------------------------------------------------------------------------
float SerialCompProcessor::getStageGainReduction(int stage) const
{
    if (stage < 0 || stage >= MAX_STAGES) return 0.0f;
    return stages[stage].getGainReductionDB();
}

// ---------------------------------------------------------------------------
// Editor factory
// ---------------------------------------------------------------------------
juce::AudioProcessorEditor* SerialCompProcessor::createEditor()
{
    return new SerialCompEditor(*this);
}

// ---------------------------------------------------------------------------
// State persistence
// ---------------------------------------------------------------------------
void SerialCompProcessor::getStateInformation(juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();
    std::unique_ptr<juce::XmlElement> xml(state.createXml());
    copyXmlToBinary(*xml, destData);
}

void SerialCompProcessor::setStateInformation(const void* data, int sizeInBytes)
{
    std::unique_ptr<juce::XmlElement> xmlState(getXmlFromBinary(data, sizeInBytes));
    if (xmlState && xmlState->hasTagName(apvts.state.getType()))
        apvts.replaceState(juce::ValueTree::fromXml(*xmlState));
}

// ---------------------------------------------------------------------------
// Plugin factory
// ---------------------------------------------------------------------------
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new SerialCompProcessor();
}
