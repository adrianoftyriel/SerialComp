#pragma once
#include <JuceHeader.h>
#include "PluginProcessor.h"
#include "StageComponent.h"
#include "CustomLookAndFeel.h"
#include "WaveformDisplay.h"

class SerialCompEditor : public juce::AudioProcessorEditor,
                         private juce::Timer
{
public:
    explicit SerialCompEditor(SerialCompProcessor&);
    ~SerialCompEditor() override;

    void paint(juce::Graphics&) override;
    void resized() override;

private:
    SerialCompProcessor& processor;
    CustomLookAndFeel laf;

    // Stage panels
    std::unique_ptr<StageComponent> stageComponents[MAX_STAGES];

    // Header controls
    juce::Label    titleLabel;
    juce::Label    stagesLabel;
    juce::Slider   numStagesSlider;     // 1–5 stepper
    juce::Label    outputLabel;
    juce::Slider   outputGainSlider;

    // Waveform display (bottom strip)
    WaveformDisplay waveformDisplay;

    using SliderAttachment = juce::AudioProcessorValueTreeState::SliderAttachment;
    std::unique_ptr<SliderAttachment> numStagesAtt;
    std::unique_ptr<SliderAttachment> outputGainAtt;

    void timerCallback() override;
    void updateStageVisibility();

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(SerialCompEditor)
};
