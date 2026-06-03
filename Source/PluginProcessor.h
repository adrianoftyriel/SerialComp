#pragma once
#include <JuceHeader.h>
#include "CompressorStage.h"
#include "WaveformDisplay.h"

class SerialCompProcessor : public juce::AudioProcessor
{
public:
    SerialCompProcessor();
    ~SerialCompProcessor() override = default;

    // AudioProcessor interface
    void prepareToPlay(double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;
    void processBlock(juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "SerialComp"; }
    bool acceptsMidi()  const override { return false; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram(int) override {}
    const juce::String getProgramName(int) override { return "Default"; }
    void changeProgramName(int, const juce::String&) override {}

    void getStateInformation(juce::MemoryBlock& destData) override;
    void setStateInformation(const void* data, int sizeInBytes) override;

    juce::AudioProcessorValueTreeState apvts;
    float getStageGainReduction(int stage) const;

    // Waveform capture buffers – written on audio thread, read on UI thread
    WaveformBuffer waveformInput;
    WaveformBuffer waveformStages[MAX_STAGES];

private:
    CompressorStage stages[MAX_STAGES];

    void syncStageParameters(int stageIdx);
    static juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(SerialCompProcessor)
};
