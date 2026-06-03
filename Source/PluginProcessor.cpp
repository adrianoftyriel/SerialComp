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
        { "1176", "LA-2A", "Destressor", "DBX 160A", "Neve 33609", "SSL G-Bus", "Manual" };

    for (int i = 0; i < MAX_STAGES; i++)
    {
        const juce::String px   = "s" + juce::String(i) + "_";
        const juce::String name = "Stage " + juce::String(i + 1) + " ";

        layout.add(std::make_unique<juce::AudioParameterBool>(
            juce::ParameterID(px + "enabled", 1),
            name + "Enabled",
            i == 0));   // only Stage 1 enabled by default

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
            juce::ParameterID(px + "knee", 1),
            name + "Knee",
            juce::NormalisableRange<float>(0.0f, 12.0f, 0.1f),
            2.0f));

        layout.add(std::make_unique<juce::AudioParameterFloat>(
            juce::ParameterID(px + "inputGain", 1),
            name + "Input Gain",
            juce::NormalisableRange<float>(-18.0f, 18.0f, 0.1f),
            0.0f));

        layout.add(std::make_unique<juce::AudioParameterFloat>(
            juce::ParameterID(px + "makeupGain", 1),
            name + "Makeup Gain",
            juce::NormalisableRange<float>(-18.0f, 24.0f, 0.1f),
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
        juce::ParameterID("outputGain", 1),
        "Output Gain",
        juce::NormalisableRange<float>(-18.0f, 18.0f, 0.1f),
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
}

// ---------------------------------------------------------------------------
// Lifecycle
// ---------------------------------------------------------------------------
void SerialCompProcessor::prepareToPlay(double sampleRate, int samplesPerBlock)
{
    for (auto& stage : stages)
        stage.prepare(sampleRate, samplesPerBlock);
}

void SerialCompProcessor::releaseResources()
{
    for (auto& stage : stages)
        stage.reset();
}

// ---------------------------------------------------------------------------
// Sync APVTS → CompressorStage (called once per block, on audio thread)
// ---------------------------------------------------------------------------
void SerialCompProcessor::syncStageParameters(int i)
{
    const juce::String px = "s" + juce::String(i) + "_";
    auto& s = stages[i];

    auto load = [&](const juce::String& id) -> float {
        return apvts.getRawParameterValue(id)->load();
    };

    s.setEnabled   (load(px + "enabled") > 0.5f);
    s.setModel     (static_cast<CompressorModel>(static_cast<int>(load(px + "model"))));
    s.setThreshold (load(px + "threshold"));
    s.setRatio     (load(px + "ratio"));
    s.setAttack    (load(px + "attack"));
    s.setRelease   (load(px + "release"));
    s.setKnee      (load(px + "knee"));
    s.setInputGain (load(px + "inputGain"));
    s.setMakeupGain(load(px + "makeupGain"));
    s.setMix       (load(px + "mix"));
}

// ---------------------------------------------------------------------------
// processBlock – serial chain: each stage feeds the next
// ---------------------------------------------------------------------------
void SerialCompProcessor::processBlock(juce::AudioBuffer<float>& buffer,
                                        juce::MidiBuffer& /*midi*/)
{
    juce::ScopedNoDenormals noDenormals;

    const int numStages = static_cast<int>(apvts.getRawParameterValue("numStages")->load());
    const int numSamples = buffer.getNumSamples();

    // Capture raw input (mix L+R to mono for display)
    for (int s = 0; s < numSamples; s++)
    {
        float sample = buffer.getNumChannels() > 1
            ? (buffer.getSample(0, s) + buffer.getSample(1, s)) * 0.5f
            : buffer.getSample(0, s);
        waveformInput.push(sample);
    }

    for (int i = 0; i < MAX_STAGES; i++)
    {
        syncStageParameters(i);

        if (i < numStages)
            stages[i].processBlock(buffer);

        // Capture stage output for waveform display
        for (int s = 0; s < numSamples; s++)
        {
            float sample = buffer.getNumChannels() > 1
                ? (buffer.getSample(0, s) + buffer.getSample(1, s)) * 0.5f
                : buffer.getSample(0, s);
            waveformStages[i].push(sample);
        }
    }

    // Master output gain
    const float outGain = juce::Decibels::decibelsToGain(
        apvts.getRawParameterValue("outputGain")->load());

    if (!juce::approximatelyEqual(outGain, 1.0f))
        buffer.applyGain(outGain);
}

// ---------------------------------------------------------------------------
// Metering accessor (called from UI thread)
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
