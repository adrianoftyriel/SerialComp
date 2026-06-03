#include <JuceHeader.h>
#include "StageComponent.h"
#include "CustomLookAndFeel.h"

static void configureRotarySlider(juce::Slider& s)
{
    s.setSliderStyle(juce::Slider::RotaryVerticalDrag);
    s.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 60, 14);
    s.setColour(juce::Slider::textBoxTextColourId,    CustomLookAndFeel::textPrimary());
    s.setColour(juce::Slider::textBoxBackgroundColourId, juce::Colours::transparentBlack);
    s.setColour(juce::Slider::textBoxOutlineColourId,    juce::Colours::transparentBlack);
}

StageComponent::StageComponent(juce::AudioProcessorValueTreeState& apvts_, int idx)
    : apvts(apvts_), stageIndex(idx)
{
    // --- Model ComboBox ---
    auto modelNames = getModelNames();
    for (int i = 0; i < modelNames.size(); i++)
        modelBox.addItem(modelNames[i], i + 1);
    modelBox.setJustificationType(juce::Justification::centred);
    modelBox.onChange = [this] { applyModelDefaults(); };
    addAndMakeVisible(modelBox);
    modelAtt = std::make_unique<ComboBoxAttachment>(apvts, pid("model"), modelBox);

    // --- Enable button ---
    addAndMakeVisible(enableButton);
    enableAtt = std::make_unique<ButtonAttachment>(apvts, pid("enabled"), enableButton);

    // --- Stage label ---
    stageLabel.setText("STAGE " + juce::String(stageIndex + 1),
                       juce::dontSendNotification);
    stageLabel.setJustificationType(juce::Justification::centredLeft);
    stageLabel.setColour(juce::Label::textColourId, CustomLookAndFeel::textSecond());
    stageLabel.setFont(juce::Font(10.0f, juce::Font::bold));
    addAndMakeVisible(stageLabel);

    // --- Knobs ---
    setupSlider(thresholdSlider, thresholdLabel, "THR");
    setupSlider(ratioSlider,     ratioLabel,     "RATIO");
    setupSlider(attackSlider,    attackLabel,    "ATK");
    setupSlider(releaseSlider,   releaseLabel,   "REL");
    setupSlider(kneeSlider,      kneeLabel,      "KNEE");
    setupSlider(inputGainSlider, inputGainLabel, "INPUT");
    setupSlider(makeupGainSlider,makeupGainLabel,"MAKEUP");
    setupSlider(mixSlider,       mixLabel,       "MIX");

    // Custom display formats
    thresholdSlider.textFromValueFunction = [](double v) { return juce::String(v, 1) + " dB"; };
    ratioSlider.textFromValueFunction     = [](double v) { return juce::String(v, 1) + ":1"; };
    attackSlider.textFromValueFunction    = [](double v) { return juce::String(v, 2) + " ms"; };
    releaseSlider.textFromValueFunction   = [](double v) { return juce::String(v < 1000.0 ? v : v / 1000.0, 2)
                                                                        + (v < 1000.0 ? " ms" : " s"); };
    kneeSlider.textFromValueFunction      = [](double v) { return juce::String(v, 1) + " dB"; };
    inputGainSlider.textFromValueFunction = [](double v) { return juce::String(v, 1) + " dB"; };
    makeupGainSlider.textFromValueFunction= [](double v) { return juce::String(v, 1) + " dB"; };
    mixSlider.textFromValueFunction       = [](double v) { return juce::String(juce::roundToInt(v * 100.0)) + "%"; };

    // Attachments
    thresholdAtt  = std::make_unique<SliderAttachment>(apvts, pid("threshold"),  thresholdSlider);
    ratioAtt      = std::make_unique<SliderAttachment>(apvts, pid("ratio"),      ratioSlider);
    attackAtt     = std::make_unique<SliderAttachment>(apvts, pid("attack"),     attackSlider);
    releaseAtt    = std::make_unique<SliderAttachment>(apvts, pid("release"),    releaseSlider);
    kneeAtt       = std::make_unique<SliderAttachment>(apvts, pid("knee"),       kneeSlider);
    inputGainAtt  = std::make_unique<SliderAttachment>(apvts, pid("inputGain"),  inputGainSlider);
    makeupGainAtt = std::make_unique<SliderAttachment>(apvts, pid("makeupGain"), makeupGainSlider);
    mixAtt        = std::make_unique<SliderAttachment>(apvts, pid("mix"),        mixSlider);

    // GR meter
    addAndMakeVisible(grMeter);
}

StageComponent::~StageComponent() = default;

void StageComponent::setupSlider(juce::Slider& s, juce::Label& lbl, const juce::String& text)
{
    configureRotarySlider(s);
    addAndMakeVisible(s);

    lbl.setText(text, juce::dontSendNotification);
    lbl.setJustificationType(juce::Justification::centred);
    lbl.setFont(juce::Font(9.5f, juce::Font::bold));
    lbl.setColour(juce::Label::textColourId, CustomLookAndFeel::textSecond());
    addAndMakeVisible(lbl);
}

void StageComponent::applyModelDefaults()
{
    int idx = modelBox.getSelectedId() - 1;
    if (idx < 0 || idx >= static_cast<int>(CompressorModel::NumModels)) return;
    auto def = getModelDefaults(static_cast<CompressorModel>(idx));

    auto setParam = [&](const juce::String& paramId, float value)
    {
        if (auto* p = apvts.getParameter(paramId))
            p->setValueNotifyingHost(p->getNormalisableRange().convertTo0to1(value));
    };

    setParam(pid("threshold"),  def.threshold);
    setParam(pid("ratio"),      def.ratio);
    setParam(pid("attack"),     def.attackMs);
    setParam(pid("release"),    def.releaseMs);
    setParam(pid("knee"),       def.kneeDB);
    setParam(pid("makeupGain"), def.makeupGainDB);
}

void StageComponent::setActive(bool active)
{
    isActive = active;
    setAlpha(active ? 1.0f : 0.45f);
    repaint();
}

void StageComponent::setGainReduction(float dB)
{
    grMeter.setGainReduction(dB);
}

juce::String StageComponent::pid(const juce::String& param) const
{
    return "s" + juce::String(stageIndex) + "_" + param;
}

void StageComponent::paint(juce::Graphics& g)
{
    auto bounds = getLocalBounds().toFloat();

    // Panel background
    g.setColour(CustomLookAndFeel::panelBg());
    g.fillRoundedRectangle(bounds, 5.0f);

    // Model-accent top strip
    int modelIdx  = modelBox.getSelectedId() - 1;
    if (modelIdx >= 0 && modelIdx < static_cast<int>(CompressorModel::NumModels))
    {
        auto accent = getModelColour(static_cast<CompressorModel>(modelIdx));
        g.setColour(accent.withAlpha(0.55f));
        g.fillRoundedRectangle(bounds.removeFromTop(3.0f), 5.0f);
    }

    // Border
    g.setColour(CustomLookAndFeel::panelBorder());
    g.drawRoundedRectangle(getLocalBounds().toFloat().reduced(0.5f), 5.0f, 1.0f);
}

void StageComponent::resized()
{
    auto bounds = getLocalBounds().reduced(6, 4);

    // --- Header row ---
    auto headerRow = bounds.removeFromTop(22);
    stageLabel.setBounds(headerRow.removeFromLeft(60));
    enableButton.setBounds(headerRow.removeFromRight(34).withSizeKeepingCentre(34, 18));

    bounds.removeFromTop(2);

    // --- Model selector ---
    modelBox.setBounds(bounds.removeFromTop(22));
    bounds.removeFromTop(4);

    // --- GR meter (right strip) ---
    auto grStrip = bounds.removeFromRight(20);
    grMeter.setBounds(grStrip);

    bounds.removeFromRight(3);

    // --- 8 knobs in 2 columns × 4 rows ---
    const int knobAreaW  = bounds.getWidth();
    const int knobAreaH  = bounds.getHeight();
    const int cols       = 2;
    const int rows       = 4;
    const int cellW      = knobAreaW / cols;
    const int cellH      = knobAreaH / rows;
    const int knobSize   = juce::jmin(cellW, cellH) - 8;
    const int labelH     = 12;

    juce::Slider* sliders[] = {
        &thresholdSlider, &ratioSlider,
        &attackSlider,    &releaseSlider,
        &kneeSlider,      &mixSlider,
        &inputGainSlider, &makeupGainSlider
    };
    juce::Label* labels[] = {
        &thresholdLabel, &ratioLabel,
        &attackLabel,    &releaseLabel,
        &kneeLabel,      &mixLabel,
        &inputGainLabel, &makeupGainLabel
    };

    for (int i = 0; i < 8; i++)
    {
        int col = i % cols;
        int row = i / cols;
        int cx  = bounds.getX() + col * cellW + cellW / 2;
        int cy  = bounds.getY() + row * cellH + (cellH - knobSize - labelH) / 2;

        sliders[i]->setBounds(cx - knobSize / 2, cy, knobSize, knobSize + 14);
        labels[i]->setBounds(cx - knobSize / 2, cy + knobSize + 2, knobSize, labelH);
    }
}
