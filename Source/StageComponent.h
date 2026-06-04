#pragma once
#include <JuceHeader.h>
#include "CompressorModels.h"
#include "GainReductionMeter.h"

class StageComponent : public juce::Component
{
public:
    StageComponent(juce::AudioProcessorValueTreeState& apvts, int stageIndex);
    ~StageComponent() override;

    void paint(juce::Graphics& g) override;
    void resized() override;
    void mouseDown(const juce::MouseEvent& e) override;

    void setActive(bool active);
    void setGainReduction(float dB);

private:
    juce::AudioProcessorValueTreeState& apvts;
    int  stageIndex;
    bool isActive = true;

    juce::ToggleButton enableButton { "ON" };
    juce::ComboBox     modelBox;

    // 6 rotary knobs: threshold, ratio, attack, release, saturation, mix
    juce::Slider thresholdSlider, ratioSlider, attackSlider, releaseSlider;
    juce::Slider satSlider, mixSlider;

    // Labels
    juce::Label thresholdLabel, ratioLabel, attackLabel, releaseLabel;
    juce::Label satLabel, mixLabel;
    juce::Label stageLabel;

    GainReductionMeter grMeter;

    using SliderAttachment   = juce::AudioProcessorValueTreeState::SliderAttachment;
    using ButtonAttachment   = juce::AudioProcessorValueTreeState::ButtonAttachment;
    using ComboBoxAttachment = juce::AudioProcessorValueTreeState::ComboBoxAttachment;

    std::unique_ptr<SliderAttachment>   thresholdAtt, ratioAtt, attackAtt, releaseAtt;
    std::unique_ptr<SliderAttachment>   satAtt, mixAtt;
    std::unique_ptr<ButtonAttachment>   enableAtt;
    std::unique_ptr<ComboBoxAttachment> modelAtt;

    float currentGainReductionDB = 0.0f;

    // Hit-test rectangles for drawn ratio buttons (1176 and Distressor)
    juce::Rectangle<int> ratioButtonRects[6];
    int  numRatioButtons = 0;

    void updateModelAppearance();
    void setupSlider(juce::Slider& s, juce::Label& lbl, const juce::String& text);
    void applyModelDefaults();
    juce::String pid(const juce::String& param) const;
};
