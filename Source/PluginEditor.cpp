#include <JuceHeader.h>
#include "PluginEditor.h"
#include "CustomLookAndFeel.h"

static constexpr int HEADER_H   = 52;
static constexpr int STAGE_W    = 210;
static constexpr int STAGE_GAP  = 4;
static constexpr int PADDING    = 8;
static constexpr int STAGE_H    = 420;
static constexpr int WAVE_H     = 90;   // waveform strip height
static constexpr int EDITOR_W   = PADDING * 2 + MAX_STAGES * STAGE_W + (MAX_STAGES - 1) * STAGE_GAP;
static constexpr int EDITOR_H   = HEADER_H + STAGE_H + WAVE_H + PADDING * 3;

SerialCompEditor::SerialCompEditor(SerialCompProcessor& p)
    : AudioProcessorEditor(&p), processor(p)
{
    setLookAndFeel(&laf);
    setSize(EDITOR_W, EDITOR_H);
    setResizable(false, false);

    // --- Stage panels ---
    for (int i = 0; i < MAX_STAGES; i++)
    {
        stageComponents[i] = std::make_unique<StageComponent>(processor.apvts, i);
        addAndMakeVisible(*stageComponents[i]);
    }

    // --- Title ---
    titleLabel.setText("SERIAL COMP", juce::dontSendNotification);
    titleLabel.setFont(juce::Font(18.0f, juce::Font::bold));
    titleLabel.setColour(juce::Label::textColourId, CustomLookAndFeel::accent());
    titleLabel.setJustificationType(juce::Justification::centredLeft);
    addAndMakeVisible(titleLabel);

    // --- Stages label ---
    stagesLabel.setText("STAGES", juce::dontSendNotification);
    stagesLabel.setFont(juce::Font(9.5f, juce::Font::bold));
    stagesLabel.setColour(juce::Label::textColourId, CustomLookAndFeel::textSecond());
    stagesLabel.setJustificationType(juce::Justification::centred);
    addAndMakeVisible(stagesLabel);

    // --- Num-stages slider (discrete 1-5) ---
    numStagesSlider.setSliderStyle(juce::Slider::IncDecButtons);
    numStagesSlider.setTextBoxStyle(juce::Slider::TextBoxLeft, false, 24, 20);
    numStagesSlider.setColour(juce::Slider::textBoxTextColourId,    CustomLookAndFeel::textPrimary());
    numStagesSlider.setColour(juce::Slider::textBoxBackgroundColourId, CustomLookAndFeel::panelBg());
    numStagesSlider.setColour(juce::Slider::textBoxOutlineColourId,    CustomLookAndFeel::panelBorder());
    numStagesSlider.onValueChange = [this] { updateStageVisibility(); };
    addAndMakeVisible(numStagesSlider);
    numStagesAtt = std::make_unique<SliderAttachment>(processor.apvts, "numStages", numStagesSlider);

    // --- Output gain ---
    outputLabel.setText("OUT", juce::dontSendNotification);
    outputLabel.setFont(juce::Font(9.5f, juce::Font::bold));
    outputLabel.setColour(juce::Label::textColourId, CustomLookAndFeel::textSecond());
    outputLabel.setJustificationType(juce::Justification::centred);
    addAndMakeVisible(outputLabel);

    outputGainSlider.setSliderStyle(juce::Slider::RotaryVerticalDrag);
    outputGainSlider.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 50, 14);
    outputGainSlider.setColour(juce::Slider::textBoxTextColourId,    CustomLookAndFeel::textPrimary());
    outputGainSlider.setColour(juce::Slider::textBoxBackgroundColourId, juce::Colours::transparentBlack);
    outputGainSlider.setColour(juce::Slider::textBoxOutlineColourId,    juce::Colours::transparentBlack);
    outputGainSlider.textFromValueFunction = [](double v) { return juce::String(v, 1) + " dB"; };
    addAndMakeVisible(outputGainSlider);
    outputGainAtt = std::make_unique<SliderAttachment>(processor.apvts, "outputGain", outputGainSlider);

    // --- Waveform display ---
    // Wire up the processor's WaveformBuffer objects as read sources.
    {
        WaveformBuffer* stagePtrs[MAX_STAGES];
        for (int i = 0; i < MAX_STAGES; i++)
            stagePtrs[i] = &processor.waveformStages[i];
        waveformDisplay.setSources(&processor.waveformInput, stagePtrs, 1);
    }
    addAndMakeVisible(waveformDisplay);

    updateStageVisibility();
    startTimerHz(30);   // 30 Hz meter + waveform refresh
}

SerialCompEditor::~SerialCompEditor()
{
    stopTimer();
    setLookAndFeel(nullptr);
}

// ---------------------------------------------------------------------------
// Timer – refresh GR meters and waveform display at 30 Hz
// ---------------------------------------------------------------------------
void SerialCompEditor::timerCallback()
{
    for (int i = 0; i < MAX_STAGES; i++)
        stageComponents[i]->setGainReduction(processor.getStageGainReduction(i));

    int numStages = static_cast<int>(numStagesSlider.getValue());
    waveformDisplay.setNumActiveStages(numStages);
    waveformDisplay.repaint();
}

// ---------------------------------------------------------------------------
// Show/dim stages based on numStages slider
// ---------------------------------------------------------------------------
void SerialCompEditor::updateStageVisibility()
{
    int numStages = static_cast<int>(numStagesSlider.getValue());
    for (int i = 0; i < MAX_STAGES; i++)
        stageComponents[i]->setActive(i < numStages);
    waveformDisplay.setNumActiveStages(numStages);
}

// ---------------------------------------------------------------------------
// Paint
// ---------------------------------------------------------------------------
void SerialCompEditor::paint(juce::Graphics& g)
{
    // Background gradient
    juce::ColourGradient bg(
        juce::Colour(0xFF1C1C24), 0.0f, 0.0f,
        juce::Colour(0xFF14141A), 0.0f, (float) getHeight(),
        false);
    g.setGradientFill(bg);
    g.fillAll();

    // Header separator line
    g.setColour(CustomLookAndFeel::panelBorder());
    g.drawHorizontalLine(HEADER_H - 1, 0.0f, (float) getWidth());

    // Subtle brand text
    g.setColour(CustomLookAndFeel::textSecond().withAlpha(0.35f));
    g.setFont(juce::Font(9.0f));
    g.drawText("CanOps Audio  v1.0",
               getWidth() - 140, HEADER_H - 16, 135, 12,
               juce::Justification::centredRight);
}

// ---------------------------------------------------------------------------
// Layout
// ---------------------------------------------------------------------------
void SerialCompEditor::resized()
{
    // Header row
    auto header = juce::Rectangle<int>(0, 0, getWidth(), HEADER_H);
    header.reduce(PADDING, 0);

    titleLabel.setBounds(header.removeFromLeft(140).withSizeKeepingCentre(140, 30));
    header.removeFromLeft(16);

    auto stagesArea = header.removeFromLeft(100).withSizeKeepingCentre(100, 40);
    stagesLabel.setBounds(stagesArea.removeFromTop(14));
    numStagesSlider.setBounds(stagesArea);

    // Output gain (right side of header)
    auto outArea = header.removeFromRight(64).withSizeKeepingCentre(64, HEADER_H - 4);
    outputLabel.setBounds(outArea.removeFromTop(14));
    outputGainSlider.setBounds(outArea);

    // Stage panels (below header, equal-width columns)
    int stageTop   = HEADER_H + PADDING;
    int stageAvailW = getWidth() - PADDING * 2 - (MAX_STAGES - 1) * STAGE_GAP;
    int stageW     = stageAvailW / MAX_STAGES;

    for (int i = 0; i < MAX_STAGES; i++)
    {
        int x = PADDING + i * (stageW + STAGE_GAP);
        stageComponents[i]->setBounds(x, stageTop, stageW, STAGE_H);
    }

    // Waveform display strip (below stages)
    int waveTop = stageTop + STAGE_H + PADDING;
    waveformDisplay.setBounds(PADDING, waveTop,
                              getWidth() - PADDING * 2, WAVE_H);
}
