#include <JuceHeader.h>
#include "PluginEditor.h"
#include "CustomLookAndFeel.h"

// ---------------------------------------------------------------------------
// Layout constants
// ---------------------------------------------------------------------------
static constexpr int HEADER_H       = 52;
static constexpr int STAGE_W        = 210;
static constexpr int STAGE_GAP      = 4;
static constexpr int PADDING        = 8;
static constexpr int STAGE_H        = 420;
static constexpr int LEVEL_METER_W  = 36;   // wider to accommodate dB labels
static constexpr int HISTORY_H      = 270;  // 3× the original 90 px strip
static constexpr int TOGGLE_ROW_H   = 22;

static constexpr int EDITOR_W =
    PADDING * 2
    + (LEVEL_METER_W + PADDING) * 2
    + MAX_STAGES * STAGE_W
    + (MAX_STAGES - 1) * STAGE_GAP;

// Full height (history visible) and compact height (history hidden)
static constexpr int EDITOR_H_FULL =
    HEADER_H + STAGE_H + PADDING * 2 + TOGGLE_ROW_H + PADDING + HISTORY_H + PADDING;
static constexpr int EDITOR_H_COMPACT =
    HEADER_H + STAGE_H + PADDING * 2 + TOGGLE_ROW_H + PADDING;

// ---------------------------------------------------------------------------
// Constructor
// ---------------------------------------------------------------------------
SerialCompEditor::SerialCompEditor(SerialCompProcessor& p)
    : AudioProcessorEditor(&p), processor(p)
{
    setLookAndFeel(&laf);
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

    // --- Stages label + slider ---
    stagesLabel.setText("STAGES", juce::dontSendNotification);
    stagesLabel.setFont(juce::Font(9.5f, juce::Font::bold));
    stagesLabel.setColour(juce::Label::textColourId, CustomLookAndFeel::textSecond());
    stagesLabel.setJustificationType(juce::Justification::centred);
    addAndMakeVisible(stagesLabel);

    numStagesSlider.setSliderStyle(juce::Slider::IncDecButtons);
    numStagesSlider.setTextBoxStyle(juce::Slider::TextBoxLeft, false, 24, 20);
    numStagesSlider.setColour(juce::Slider::textBoxTextColourId,      CustomLookAndFeel::textPrimary());
    numStagesSlider.setColour(juce::Slider::textBoxBackgroundColourId, CustomLookAndFeel::panelBg());
    numStagesSlider.setColour(juce::Slider::textBoxOutlineColourId,    CustomLookAndFeel::panelBorder());
    numStagesSlider.onValueChange = [this] { updateStageVisibility(); };
    addAndMakeVisible(numStagesSlider);
    numStagesAtt = std::make_unique<SliderAttachment>(processor.apvts, "numStages", numStagesSlider);

    // --- Global Input Gain (header, left area) ---
    auto setupHeaderKnob = [&](juce::Slider& s, juce::Label& lbl, const juce::String& text)
    {
        lbl.setText(text, juce::dontSendNotification);
        lbl.setFont(juce::Font(9.5f, juce::Font::bold));
        lbl.setColour(juce::Label::textColourId, CustomLookAndFeel::textSecond());
        lbl.setJustificationType(juce::Justification::centred);
        addAndMakeVisible(lbl);

        s.setSliderStyle(juce::Slider::RotaryVerticalDrag);
        s.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 50, 14);
        s.setColour(juce::Slider::textBoxTextColourId,      CustomLookAndFeel::textPrimary());
        s.setColour(juce::Slider::textBoxBackgroundColourId, juce::Colours::transparentBlack);
        s.setColour(juce::Slider::textBoxOutlineColourId,    juce::Colours::transparentBlack);
        s.textFromValueFunction = [](double v) { return juce::String(v, 1) + " dB"; };
        addAndMakeVisible(s);
    };

    setupHeaderKnob(inputGainSlider,  inputGainLabel,  "INPUT");
    setupHeaderKnob(makeupGainSlider, makeupGainLabel, "MAKEUP");

    inputGainAtt  = std::make_unique<SliderAttachment>(processor.apvts, "inputGain",  inputGainSlider);
    makeupGainAtt = std::make_unique<SliderAttachment>(processor.apvts, "makeupGain", makeupGainSlider);

    // --- Input / output level meters ---
    addAndMakeVisible(inputMeter);
    addAndMakeVisible(outputMeter);

    auto setupMeterLabel = [&](juce::Label& lbl, const juce::String& text)
    {
        lbl.setText(text, juce::dontSendNotification);
        lbl.setFont(juce::Font(9.0f, juce::Font::bold));
        lbl.setColour(juce::Label::textColourId, CustomLookAndFeel::textSecond());
        lbl.setJustificationType(juce::Justification::centred);
        addAndMakeVisible(lbl);
    };
    setupMeterLabel(inputMeterLabel,  "IN");
    setupMeterLabel(outputMeterLabel, "OUT");

    // --- Volume history display ---
    {
        WaveformBuffer* stagePtrs[MAX_STAGES];
        for (int i = 0; i < MAX_STAGES; i++)
            stagePtrs[i] = &processor.waveformStages[i];
        historyDisplay.setSources(&processor.waveformInput, stagePtrs, 1);
    }
    addAndMakeVisible(historyDisplay);

    // --- History toggle button ---
    historyToggleButton.setButtonText(juce::CharPointer_UTF8 ("\xe2\x96\xbc  VOLUME HISTORY"));
    historyToggleButton.setColour(juce::TextButton::buttonColourId,   juce::Colour(0xFF18181E));
    historyToggleButton.setColour(juce::TextButton::buttonOnColourId, juce::Colour(0xFF22222C));
    historyToggleButton.setColour(juce::TextButton::textColourOffId,  CustomLookAndFeel::textSecond());
    historyToggleButton.onClick = [this]
    {
        historyVisible = !historyVisible;
        historyDisplay.setVisible(historyVisible);
        historyToggleButton.setButtonText(historyVisible
            ? juce::CharPointer_UTF8 ("\xe2\x96\xbc  VOLUME HISTORY")
            : juce::CharPointer_UTF8 ("\xe2\x96\xb6  VOLUME HISTORY"));
        setSize(EDITOR_W, historyVisible ? EDITOR_H_FULL : EDITOR_H_COMPACT);
    };
    addAndMakeVisible(historyToggleButton);

    updateStageVisibility();
    startTimerHz(30);

    // setSize() must come last — triggers resized() synchronously.
    setSize(EDITOR_W, EDITOR_H_FULL);
}

SerialCompEditor::~SerialCompEditor()
{
    stopTimer();
    setLookAndFeel(nullptr);
}

// ---------------------------------------------------------------------------
// Timer – refresh meters and history display at 30 Hz
// ---------------------------------------------------------------------------
void SerialCompEditor::timerCallback()
{
    for (int i = 0; i < MAX_STAGES; i++)
        stageComponents[i]->setGainReduction(processor.getStageGainReduction(i));

    const double sr = processor.getSampleRate();
    if (sr > 0.0)
        historyDisplay.setSampleRate(sr);

    int numStages = static_cast<int>(numStagesSlider.getValue());
    historyDisplay.setNumActiveStages(numStages);

    if (historyVisible)
        historyDisplay.repaint();

    inputMeter.setLevel(processor.getInputLevelDB());
    inputMeter.advancePeakHold();

    outputMeter.setLevel(processor.getOutputLevelDB());
    outputMeter.advancePeakHold();
}

// ---------------------------------------------------------------------------
// Show/dim stages based on numStages slider
// ---------------------------------------------------------------------------
void SerialCompEditor::updateStageVisibility()
{
    int numStages = static_cast<int>(numStagesSlider.getValue());
    for (int i = 0; i < MAX_STAGES; i++)
        stageComponents[i]->setActive(i < numStages);
    historyDisplay.setNumActiveStages(numStages);
}

// ---------------------------------------------------------------------------
// Paint
// ---------------------------------------------------------------------------
void SerialCompEditor::paint(juce::Graphics& g)
{
    juce::ColourGradient bg(
        juce::Colour(0xFF1C1C24), 0.0f, 0.0f,
        juce::Colour(0xFF14141A), 0.0f, (float) getHeight(),
        false);
    g.setGradientFill(bg);
    g.fillAll();

    g.setColour(CustomLookAndFeel::panelBorder());
    g.drawHorizontalLine(HEADER_H - 1, 0.0f, (float) getWidth());

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
    // --- Header row ---
    auto header = juce::Rectangle<int>(0, 0, getWidth(), HEADER_H);
    header.reduce(PADDING, 0);

    // Title (left)
    titleLabel.setBounds(header.removeFromLeft(140).withSizeKeepingCentre(140, 30));
    header.removeFromLeft(8);

    // Input gain knob (after title)
    auto inGainArea = header.removeFromLeft(64).withSizeKeepingCentre(64, HEADER_H - 4);
    inputGainLabel.setBounds(inGainArea.removeFromTop(14));
    inputGainSlider.setBounds(inGainArea);
    header.removeFromLeft(8);

    // Stages control
    auto stagesArea = header.removeFromLeft(100).withSizeKeepingCentre(100, 40);
    stagesLabel.setBounds(stagesArea.removeFromTop(14));
    numStagesSlider.setBounds(stagesArea);

    // Makeup gain knob (right side)
    auto mkArea = header.removeFromRight(64).withSizeKeepingCentre(64, HEADER_H - 4);
    makeupGainLabel.setBounds(mkArea.removeFromTop(14));
    makeupGainSlider.setBounds(mkArea);

    // --- Stage panel row ---
    const int stageTop   = HEADER_H + PADDING;
    const int stageLeft  = PADDING + LEVEL_METER_W + PADDING;
    const int stageRight = getWidth() - PADDING - LEVEL_METER_W - PADDING;
    const int stageAvailW = stageRight - stageLeft - (MAX_STAGES - 1) * STAGE_GAP;
    const int stageW     = stageAvailW / MAX_STAGES;

    for (int i = 0; i < MAX_STAGES; i++)
    {
        int x = stageLeft + i * (stageW + STAGE_GAP);
        stageComponents[i]->setBounds(x, stageTop, stageW, STAGE_H);
    }

    // Input level meter (left of Stage 1)
    const int meterLabelH = 14;
    inputMeterLabel.setBounds(PADDING, stageTop, LEVEL_METER_W, meterLabelH);
    inputMeter.setBounds(PADDING, stageTop + meterLabelH,
                         LEVEL_METER_W, STAGE_H - meterLabelH);

    // Output level meter (right of Stage 5)
    const int outMeterX = getWidth() - PADDING - LEVEL_METER_W;
    outputMeterLabel.setBounds(outMeterX, stageTop, LEVEL_METER_W, meterLabelH);
    outputMeter.setBounds(outMeterX, stageTop + meterLabelH,
                          LEVEL_METER_W, STAGE_H - meterLabelH);

    // --- Toggle button row ---
    const int toggleTop = stageTop + STAGE_H + PADDING;
    historyToggleButton.setBounds(PADDING, toggleTop,
                                  getWidth() - PADDING * 2, TOGGLE_ROW_H);

    // --- Volume history panel ---
    if (historyVisible)
    {
        const int histTop = toggleTop + TOGGLE_ROW_H + PADDING;
        historyDisplay.setBounds(PADDING, histTop,
                                 getWidth() - PADDING * 2, HISTORY_H);
    }
}
