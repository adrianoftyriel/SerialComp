#pragma once
#include <JuceHeader.h>
#include "CompressorStage.h"
#include "WaveformDisplay.h"

class SerialCompProcessor : public juce::AudioProcessor
{
public:
    SerialCompProcessor();
    ~SerialCompProcessor() override = default;

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
    float getInputLevelDB()  const { return inputLevelDB.load();  }
    float getOutputLevelDB() const { return outputLevelDB.load(); }

    // Audio capture – written on audio thread, read on UI thread
    WaveformBuffer waveformInput;
    WaveformBuffer waveformStages[MAX_STAGES];

    // Level metering – written on audio thread, read on UI thread
    std::atomic<float> inputLevelDB  { -96.0f };
    std::atomic<float> outputLevelDB { -96.0f };

private:
    CompressorStage stages[MAX_STAGES];

    // -----------------------------------------------------------------------
    // Cached raw-parameter pointers (populated once, non-RT; read-only via
    // ->load() on the audio thread — always lock-free).
    // -----------------------------------------------------------------------
    struct StageParamPtrs
    {
        std::atomic<float>* enabled   = nullptr;
        std::atomic<float>* model     = nullptr;
        std::atomic<float>* threshold = nullptr;
        std::atomic<float>* ratio     = nullptr;
        std::atomic<float>* attack    = nullptr;
        std::atomic<float>* release   = nullptr;
        std::atomic<float>* sat       = nullptr;
        std::atomic<float>* mix       = nullptr;
    };
    StageParamPtrs      stageParams[MAX_STAGES];
    std::atomic<float>* numStagesParam  = nullptr;
    std::atomic<float>* inputGainParam  = nullptr;   // global pre-chain input gain
    std::atomic<float>* makeupGainParam = nullptr;   // global post-chain makeup gain

    void cacheParamPointers();
    void syncStageParameters(int stageIdx);

    // Ballistic envelope state (audio thread only)
    float  inputEnvLinear  = 0.0f;
    float  outputEnvLinear = 0.0f;
    double currentSampleRate = 44100.0;
    float  prevInputGain   = 1.0f;
    float  prevMakeupGain  = 1.0f;

    static juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(SerialCompProcessor)
};
