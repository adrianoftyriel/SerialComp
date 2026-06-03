#pragma once
#include <JuceHeader.h>
#include "PluginProcessor.h"
#include "StageComponent.h"
#include "CustomLookAndFeel.h"
#include "WaveformDisplay.h"
#include "LevelMeter.h"

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
    juce::Label  titleLabel;
    juce::Label  stagesLabel;
    juce::Slider numStagesSlider;

    // Global input gain (left of header)
    juce::Label  inputGainLabel;
    juce::Slider inputGainSlider;

    // Global makeup gain (right of header)
    juce::Label  makeupGainLabel;
    juce::Slider makeupGainSlider;

    // Input / output level meters (flank the stage panels)
    LevelMeter inputMeter;
    LevelMeter outputMeter;
    juce::Label inputMeterLabel;
    juce::Label outputMeterLabel;

    // Volume history display (collapsible bottom panel)
    VolumeHistoryDisplay historyDisplay;
    juce::TextButton     historyToggleButton;
    bool historyVisible = true;

    // Standalone-only: audio device settings (sample rate, bit depth, buffer)
    juce::TextButton audioSettingsButton { "AUDIO" };
    void showAudioSettings();

    using SliderAttachment = juce::AudioProcessorValueTreeState::SliderAttachment;
    std::unique_ptr<SliderAttachment> numStagesAtt;
    std::unique_ptr<SliderAttachment> inputGainAtt;
    std::unique_ptr<SliderAttachment> makeupGainAtt;

    void timerCallback() override;
    void updateStageVisibility();

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(SerialCompEditor)
};
