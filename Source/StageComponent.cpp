#include <JuceHeader.h>
#include "StageComponent.h"
#include "CustomLookAndFeel.h"

// ---------------------------------------------------------------------------
// File-scope helpers
// ---------------------------------------------------------------------------

static void configureRotarySlider(juce::Slider& s)
{
    s.setSliderStyle(juce::Slider::RotaryVerticalDrag);
    s.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 60, 14);
    s.setColour(juce::Slider::textBoxTextColourId,       CustomLookAndFeel::textPrimary());
    s.setColour(juce::Slider::textBoxBackgroundColourId, juce::Colours::transparentBlack);
    s.setColour(juce::Slider::textBoxOutlineColourId,    juce::Colours::transparentBlack);
}

static void drawScrew(juce::Graphics& g, float cx, float cy, float r = 4.5f)
{
    // Silver circle
    juce::ColourGradient screwGrad(
        juce::Colour(0xFFCCCCCC), cx - r * 0.4f, cy - r * 0.4f,
        juce::Colour(0xFF888888), cx + r * 0.4f, cy + r * 0.4f,
        false);
    g.setGradientFill(screwGrad);
    g.fillEllipse(cx - r, cy - r, r * 2.0f, r * 2.0f);

    // Outline
    g.setColour(juce::Colour(0xFF555555));
    g.drawEllipse(cx - r, cy - r, r * 2.0f, r * 2.0f, 0.8f);

    // Phillips cross slot
    g.setColour(juce::Colour(0xFF444444));
    const float slotLen = r * 0.65f;
    const float slotW   = 0.9f;
    g.drawLine(cx, cy - slotLen, cx, cy + slotLen, slotW);
    g.drawLine(cx - slotLen, cy, cx + slotLen, cy, slotW);
}

static bool isLightPanel(CompressorModel m)
{
    return (m == CompressorModel::Model1176
         || m == CompressorModel::ModelLA2A
         || m == CompressorModel::ModelDestressor
         || m == CompressorModel::ModelSSL);
}

static juce::Colour panelTextColour(CompressorModel m)
{
    return isLightPanel(m) ? juce::Colour(0xFF1A1A20) : juce::Colour(0xFFCCCCCC);
}

// ---------------------------------------------------------------------------
// Constructor / Destructor
// ---------------------------------------------------------------------------

StageComponent::StageComponent(juce::AudioProcessorValueTreeState& apvts_, int idx)
    : apvts(apvts_), stageIndex(idx)
{
    // --- Model ComboBox ---
    auto modelNames = getModelNames();
    for (int i = 0; i < modelNames.size(); i++)
        modelBox.addItem(modelNames[i], i + 1);
    modelBox.setJustificationType(juce::Justification::centred);
    modelBox.onChange = [this] { applyModelDefaults(); updateModelAppearance(); };
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

    // --- 6 knobs ---
    setupSlider(thresholdSlider, thresholdLabel, "THR");
    setupSlider(ratioSlider,     ratioLabel,     "RATIO");
    setupSlider(attackSlider,    attackLabel,    "ATK");
    setupSlider(releaseSlider,   releaseLabel,   "REL");
    setupSlider(kneeSlider,      kneeLabel,      "KNEE");
    setupSlider(mixSlider,       mixLabel,       "MIX");

    // Custom display formats
    thresholdSlider.textFromValueFunction = [](double v) { return juce::String(v, 1) + " dB"; };
    ratioSlider.textFromValueFunction     = [](double v) { return juce::String(v, 1) + ":1"; };
    attackSlider.textFromValueFunction    = [](double v) { return juce::String(v, 2) + " ms"; };
    releaseSlider.textFromValueFunction   = [](double v) {
        return juce::String(v < 1000.0 ? v : v / 1000.0, 2) + (v < 1000.0 ? " ms" : " s");
    };
    kneeSlider.textFromValueFunction = [](double v) { return juce::String(v, 1) + " dB"; };
    mixSlider.textFromValueFunction  = [](double v) {
        return juce::String(juce::roundToInt(v * 100.0)) + "%";
    };

    // Attachments
    thresholdAtt = std::make_unique<SliderAttachment>(apvts, pid("threshold"), thresholdSlider);
    ratioAtt     = std::make_unique<SliderAttachment>(apvts, pid("ratio"),     ratioSlider);
    attackAtt    = std::make_unique<SliderAttachment>(apvts, pid("attack"),    attackSlider);
    releaseAtt   = std::make_unique<SliderAttachment>(apvts, pid("release"),   releaseSlider);
    kneeAtt      = std::make_unique<SliderAttachment>(apvts, pid("knee"),      kneeSlider);
    mixAtt       = std::make_unique<SliderAttachment>(apvts, pid("mix"),       mixSlider);

    addAndMakeVisible(grMeter);

    // Apply initial model appearance
    updateModelAppearance();
}

StageComponent::~StageComponent() = default;

// ---------------------------------------------------------------------------
// setupSlider
// ---------------------------------------------------------------------------

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

// ---------------------------------------------------------------------------
// applyModelDefaults
// ---------------------------------------------------------------------------

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

    setParam(pid("threshold"), def.threshold);
    setParam(pid("ratio"),     def.ratio);
    setParam(pid("attack"),    def.attackMs);
    setParam(pid("release"),   def.releaseMs);
    setParam(pid("knee"),      def.kneeDB);
}

// ---------------------------------------------------------------------------
// updateModelAppearance
// ---------------------------------------------------------------------------

void StageComponent::updateModelAppearance()
{
    // 1. Get current model
    int idx = modelBox.getSelectedId() - 1;
    CompressorModel model = CompressorModel::ModelManual;
    if (idx >= 0 && idx < static_cast<int>(CompressorModel::NumModels))
        model = static_cast<CompressorModel>(idx);

    // 2. Update label text per model
    struct LabelTexts { const char* thr; const char* ratio; const char* atk; const char* rel; const char* knee; const char* mix; };
    LabelTexts texts = {};

    switch (model)
    {
        case CompressorModel::Model1176:
            texts = { "INPUT", "RATIO", "ATTACK", "RELEASE", "KNEE", "OUTPUT" };
            break;
        case CompressorModel::ModelLA2A:
            texts = { "PEAK RED.", "RATIO", "ATTACK", "RELEASE", "KNEE", "GAIN" };
            break;
        case CompressorModel::ModelDestressor:
            texts = { "THRESH", "RATIO", "ATK", "REL", "KNEE", "MIX" };
            break;
        case CompressorModel::ModelDBX160A:
            texts = { "THRESHOLD", "RATIO", "ATTACK", "RELEASE", "KNEE", "MIX" };
            break;
        case CompressorModel::ModelNeve:
            texts = { "THRESHOLD", "RATIO", "ATTACK", "RELEASE", "KNEE", "MIX" };
            break;
        case CompressorModel::ModelSSL:
            texts = { "THRESHOLD", "RATIO", "ATTACK", "RELEASE", "KNEE", "MIX" };
            break;
        case CompressorModel::ModelManual:
        default:
            texts = { "THR", "RATIO", "ATK", "REL", "KNEE", "MIX" };
            break;
    }

    thresholdLabel.setText(texts.thr,   juce::dontSendNotification);
    ratioLabel    .setText(texts.ratio, juce::dontSendNotification);
    attackLabel   .setText(texts.atk,   juce::dontSendNotification);
    releaseLabel  .setText(texts.rel,   juce::dontSendNotification);
    kneeLabel     .setText(texts.knee,  juce::dontSendNotification);
    mixLabel      .setText(texts.mix,   juce::dontSendNotification);

    // 3. Knob arc colour (fill)
    juce::Colour arcColour = getModelColour(model);

    // 4. Track colour
    juce::Colour trackColour = isLightPanel(model)
        ? juce::Colour(0xFF888880)
        : juce::Colour(0xFF303038);

    juce::Slider* sliders[] = {
        &thresholdSlider, &ratioSlider, &attackSlider,
        &releaseSlider,   &kneeSlider,  &mixSlider
    };

    for (auto* s : sliders)
    {
        s->setColour(juce::Slider::rotarySliderFillColourId,    arcColour);
        s->setColour(juce::Slider::rotarySliderOutlineColourId, trackColour);
    }

    // 5. Label text colours
    juce::Colour txtCol = panelTextColour(model);

    juce::Label* labels[] = {
        &thresholdLabel, &ratioLabel, &attackLabel,
        &releaseLabel,   &kneeLabel,  &mixLabel
    };

    for (auto* lbl : labels)
        lbl->setColour(juce::Label::textColourId, txtCol);

    stageLabel.setColour(juce::Label::textColourId, txtCol);

    // 6. Slider textbox text colour
    for (auto* s : sliders)
        s->setColour(juce::Slider::textBoxTextColourId, txtCol);

    // 7. GR meter orientation / visibility
    if (model == CompressorModel::Model1176)
    {
        grMeter.setHorizontal(true);
        grMeter.setVisible(true);
    }
    else if (model == CompressorModel::ModelLA2A)
    {
        grMeter.setVisible(false);
    }
    else
    {
        grMeter.setHorizontal(false);
        grMeter.setVisible(true);
    }

    // 8 & 9. Trigger layout and repaint
    if (getWidth() > 0)
        resized();
    repaint();
}

// ---------------------------------------------------------------------------
// setActive / setGainReduction
// ---------------------------------------------------------------------------

void StageComponent::setActive(bool active)
{
    isActive = active;
    setAlpha(active ? 1.0f : 0.45f);
    repaint();
}

void StageComponent::setGainReduction(float dB)
{
    currentGainReductionDB = dB;
    grMeter.setGainReduction(dB);
    int idx = modelBox.getSelectedId() - 1;
    if (idx == static_cast<int>(CompressorModel::ModelLA2A))
        repaint();  // update LA-2A VU needle
}

juce::String StageComponent::pid(const juce::String& param) const
{
    return "s" + juce::String(stageIndex) + "_" + param;
}

// ---------------------------------------------------------------------------
// paint  — model-specific background + decorations
// ---------------------------------------------------------------------------

void StageComponent::paint(juce::Graphics& g)
{
    auto bounds = getLocalBounds().toFloat();

    int modelIdx = modelBox.getSelectedId() - 1;
    CompressorModel model = CompressorModel::ModelManual;
    if (modelIdx >= 0 && modelIdx < static_cast<int>(CompressorModel::NumModels))
        model = static_cast<CompressorModel>(modelIdx);

    const float cornerR = 5.0f;
    const float W = bounds.getWidth();
    const float H = bounds.getHeight();

    switch (model)
    {
        // ------------------------------------------------------------------
        case CompressorModel::Model1176:
        {
            // Silver brushed-aluminum gradient
            juce::ColourGradient bg(
                juce::Colour(0xFFCCCCCA), 0.0f, 0.0f,
                juce::Colour(0xFFB4B4BC), 0.0f, H,
                false);
            g.setGradientFill(bg);
            g.fillRoundedRectangle(bounds, cornerR);

            // Subtle horizontal brush lines every 4px
            for (int lineY = 0; lineY < (int) H; lineY += 4)
            {
                g.setColour(juce::Colour(0x10FFFFFF));
                g.drawHorizontalLine(lineY, 0.0f, W);
            }

            // Navy blue top accent stripe
            g.setColour(juce::Colour(0xFF1133AA));
            g.fillRect(0.0f, 0.0f, W, 5.0f);

            // Corner screws
            drawScrew(g, 8.0f, 8.0f);
            drawScrew(g, W - 8.0f, 8.0f);
            drawScrew(g, 8.0f, H - 8.0f);
            drawScrew(g, W - 8.0f, H - 8.0f);

            // Branding at bottom
            g.setColour(juce::Colour(0xFF222233));
            g.setFont(juce::Font(10.0f, juce::Font::bold));
            g.drawText("UA 1176LN", 0, (int) H - 16, (int) W, 14,
                       juce::Justification::centred);

            // "GR" label above the GR meter strip
            g.setFont(juce::Font(8.0f, juce::Font::bold));
            g.setColour(juce::Colour(0xFF222233));
            // Header overhead: 6 (padding) + 22 (stageLabel row) + 2 (gap) + 22 (model box) + 4 = 56
            g.drawText("GR", 0, 56, (int) W, 12, juce::Justification::centred);

            // Border
            g.setColour(juce::Colour(0xFF888899));
            g.drawRoundedRectangle(bounds.reduced(0.5f), cornerR, 1.0f);
            break;
        }

        // ------------------------------------------------------------------
        case CompressorModel::ModelLA2A:
        {
            // Warm gray gradient
            juce::ColourGradient bg(
                juce::Colour(0xFF989890), 0.0f, 0.0f,
                juce::Colour(0xFFA8A8A0), 0.0f, H,
                false);
            g.setGradientFill(bg);
            g.fillRoundedRectangle(bounds, cornerR);

            // Content area starts below header (approx 56px overhead)
            const float headerH  = 56.0f;
            const float contentH = H - headerH - 6.0f; // 6px bottom padding
            const float vuH      = contentH * 0.40f;
            const float vuY      = headerH;

            // VU meter face background
            auto vuRect = juce::Rectangle<float>(6.0f, vuY, W - 12.0f, vuH);
            g.setColour(juce::Colour(0xFF1A1A18));
            g.fillRoundedRectangle(vuRect, 4.0f);

            // "GR" label inside VU area
            g.setFont(juce::Font(8.0f, juce::Font::bold));
            g.setColour(juce::Colour(0xFF666655));
            g.drawText("GR", (int) vuRect.getX() + 4, (int) vuRect.getY() + 3,
                       24, 10, juce::Justification::centredLeft);

            // Semi-circular VU arc
            const float pivotX  = vuRect.getCentreX();
            const float pivotY  = vuRect.getBottom() - 4.0f;
            const float arcR    = juce::jmin(vuRect.getWidth() * 0.42f,
                                             vuRect.getHeight() * 0.90f);
            const float arcStart = 3.93f;
            const float arcEnd   = 5.50f;

            // Track arc
            juce::Path trackArc;
            trackArc.addArc(pivotX - arcR, pivotY - arcR,
                            arcR * 2.0f, arcR * 2.0f,
                            arcStart, arcEnd, true);
            g.setColour(juce::Colour(0xFF444433));
            g.strokePath(trackArc, juce::PathStrokeType(2.5f));

            // Scale ticks & labels: 0, -2, -4, -6, -8, -10, -14 dB GR
            // Map dB to angle: 0 dB -> arcStart, 20 dB -> arcEnd
            const float dbTicks[] = { 0.0f, 2.0f, 4.0f, 6.0f, 8.0f, 10.0f, 14.0f };
            const int   numTicks  = 7;
            for (int ti = 0; ti < numTicks; ti++)
            {
                float db      = dbTicks[ti];
                float ang     = arcStart + (db / 20.0f) * (arcEnd - arcStart);
                float tx      = pivotX + arcR * std::cos(ang);
                float ty      = pivotY + arcR * std::sin(ang);
                float tx2     = pivotX + (arcR - 6.0f) * std::cos(ang);
                float ty2     = pivotY + (arcR - 6.0f) * std::sin(ang);
                g.setColour(juce::Colour(0xFFAAAAAA));
                g.drawLine(tx, ty, tx2, ty2, 1.0f);

                // Label
                float lx = pivotX + (arcR - 14.0f) * std::cos(ang);
                float ly = pivotY + (arcR - 14.0f) * std::sin(ang);
                g.setFont(juce::Font(6.5f));
                g.setColour(juce::Colour(0xFF999988));
                g.drawText(juce::String((int) db),
                           (int) lx - 6, (int) ly - 5, 12, 8,
                           juce::Justification::centred);
            }

            // VU Needle
            {
                float needleDb  = juce::jlimit(0.0f, 20.0f, currentGainReductionDB);
                float needleAng = arcStart + (needleDb / 20.0f) * (arcEnd - arcStart);
                float needleTipX = pivotX + (arcR - 2.0f) * std::cos(needleAng);
                float needleTipY = pivotY + (arcR - 2.0f) * std::sin(needleAng);

                g.setColour(juce::Colours::white.withAlpha(0.9f));
                g.drawLine(pivotX, pivotY, needleTipX, needleTipY, 1.5f);

                // Small red triangle at tip
                juce::Path tip;
                const float tipSize = 3.5f;
                tip.addTriangle(
                    needleTipX, needleTipY,
                    needleTipX + tipSize * std::cos(needleAng + 0.4f),
                    needleTipY + tipSize * std::sin(needleAng + 0.4f),
                    needleTipX + tipSize * std::cos(needleAng - 0.4f),
                    needleTipY + tipSize * std::sin(needleAng - 0.4f));
                g.setColour(juce::Colour(0xFFCC2222));
                g.fillPath(tip);

                // Pivot dot
                g.setColour(juce::Colour(0xFFCCCCCC));
                g.fillEllipse(pivotX - 3.0f, pivotY - 3.0f, 6.0f, 6.0f);
            }

            // Branding
            g.setColour(juce::Colour(0xFF333322));
            g.setFont(juce::Font(10.0f, juce::Font::bold));
            g.drawText("LA-2A", 0, (int) H - 16, (int) W, 14,
                       juce::Justification::centred);

            // Corner screws
            drawScrew(g, 8.0f, 8.0f);
            drawScrew(g, W - 8.0f, 8.0f);
            drawScrew(g, 8.0f, H - 8.0f);
            drawScrew(g, W - 8.0f, H - 8.0f);

            // Border
            g.setColour(juce::Colour(0xFF777770));
            g.drawRoundedRectangle(bounds.reduced(0.5f), cornerR, 1.0f);
            break;
        }

        // ------------------------------------------------------------------
        case CompressorModel::ModelDestressor:
        {
            // Cream gradient
            juce::ColourGradient bg(
                juce::Colour(0xFFE0D4C0), 0.0f, 0.0f,
                juce::Colour(0xFFCEC2AC), 0.0f, H,
                false);
            g.setGradientFill(bg);
            g.fillRoundedRectangle(bounds, cornerR);

            // Green accent stripe at top
            g.setColour(juce::Colour(0xFF228844));
            g.fillRect(0.0f, 0.0f, W, 5.0f);

            // Branding at bottom
            g.setColour(juce::Colour(0xFF2A2A20));
            g.setFont(juce::Font(9.0f, juce::Font::bold));
            g.drawText("DISTRESSOR", 0, (int) H - 16, (int) W, 14,
                       juce::Justification::centred);

            // Corner screws
            drawScrew(g, 8.0f, 8.0f);
            drawScrew(g, W - 8.0f, 8.0f);
            drawScrew(g, 8.0f, H - 8.0f);
            drawScrew(g, W - 8.0f, H - 8.0f);

            // Border
            g.setColour(juce::Colour(0xFFAA9980));
            g.drawRoundedRectangle(bounds.reduced(0.5f), cornerR, 1.0f);
            break;
        }

        // ------------------------------------------------------------------
        case CompressorModel::ModelDBX160A:
        {
            // Dark gradient
            juce::ColourGradient bg(
                juce::Colour(0xFF0E0E10), 0.0f, 0.0f,
                juce::Colour(0xFF1A1A1C), 0.0f, H,
                false);
            g.setGradientFill(bg);
            g.fillRoundedRectangle(bounds, cornerR);

            // Red accent stripe at top
            g.setColour(juce::Colour(0xFFCC2222));
            g.fillRect(0.0f, 0.0f, W, 5.0f);

            // "dbx" branding (bold, red, left-aligned)
            g.setColour(juce::Colour(0xFFCC2222));
            g.setFont(juce::Font(12.0f, juce::Font::bold));
            g.drawText("dbx", 8, (int) H - 18, (int) W / 2, 16,
                       juce::Justification::centredLeft);

            // "MODEL 160A" (gray, right-aligned)
            g.setColour(juce::Colour(0xFF888888));
            g.setFont(juce::Font(8.0f));
            g.drawText("MODEL 160A", (int) W / 2, (int) H - 16, (int) W / 2 - 6, 14,
                       juce::Justification::centredRight);

            // Border
            g.setColour(juce::Colour(0xFF383838));
            g.drawRoundedRectangle(bounds.reduced(0.5f), cornerR, 1.0f);
            break;
        }

        // ------------------------------------------------------------------
        case CompressorModel::ModelNeve:
        {
            // Dark charcoal gradient
            juce::ColourGradient bg(
                juce::Colour(0xFF1A1C1E), 0.0f, 0.0f,
                juce::Colour(0xFF22262A), 0.0f, H,
                false);
            g.setGradientFill(bg);
            g.fillRoundedRectangle(bounds, cornerR);

            // Orange accent stripe at top
            g.setColour(juce::Colour(0xFFCC5522));
            g.fillRect(0.0f, 0.0f, W, 5.0f);

            // "neve" italic at bottom left
            g.setColour(juce::Colour(0xFFCC6633));
            g.setFont(juce::Font(14.0f, juce::Font::italic));
            g.drawText("neve", 8, (int) H - 18, 60, 16,
                       juce::Justification::centredLeft);

            // "33609" model number
            g.setColour(juce::Colour(0xFF888888));
            g.setFont(juce::Font(8.0f));
            g.drawText("33609", 68, (int) H - 16, (int) W - 76, 14,
                       juce::Justification::centredLeft);

            // Corner screws
            drawScrew(g, 8.0f, 8.0f);
            drawScrew(g, W - 8.0f, 8.0f);
            drawScrew(g, 8.0f, H - 8.0f);
            drawScrew(g, W - 8.0f, H - 8.0f);

            // Border
            g.setColour(juce::Colour(0xFF404448));
            g.drawRoundedRectangle(bounds.reduced(0.5f), cornerR, 1.0f);
            break;
        }

        // ------------------------------------------------------------------
        case CompressorModel::ModelSSL:
        {
            // Light gray gradient
            juce::ColourGradient bg(
                juce::Colour(0xFFC8C8C0), 0.0f, 0.0f,
                juce::Colour(0xFFD8D8D0), 0.0f, H,
                false);
            g.setGradientFill(bg);
            g.fillRoundedRectangle(bounds, cornerR);

            // Yellow-green accent stripe at top
            g.setColour(juce::Colour(0xFF88AA22));
            g.fillRect(0.0f, 0.0f, W, 5.0f);

            // "ssl" bold at bottom
            g.setColour(juce::Colour(0xFF444440));
            g.setFont(juce::Font(12.0f, juce::Font::bold));
            g.drawText("ssl", 8, (int) H - 28, (int) W - 16, 14,
                       juce::Justification::centredLeft);

            // "G-BUS COMP" smaller text below
            g.setColour(juce::Colour(0xFF888880));
            g.setFont(juce::Font(8.0f));
            g.drawText("G-BUS COMP", 8, (int) H - 15, (int) W - 16, 12,
                       juce::Justification::centredLeft);

            // Border
            g.setColour(juce::Colour(0xFFAAAAAA));
            g.drawRoundedRectangle(bounds.reduced(0.5f), cornerR, 1.0f);
            break;
        }

        // ------------------------------------------------------------------
        case CompressorModel::ModelManual:
        default:
        {
            // Dark panel with model-colour accent stripe
            juce::ColourGradient bg(
                CustomLookAndFeel::panelBg(), 0.0f, 0.0f,
                CustomLookAndFeel::panelBg().darker(0.15f), 0.0f, H,
                false);
            g.setGradientFill(bg);
            g.fillRoundedRectangle(bounds, cornerR);

            // Model accent stripe
            g.setColour(getModelColour(model).withAlpha(0.7f));
            g.fillRect(0.0f, 0.0f, W, 3.0f);

            // Border
            g.setColour(CustomLookAndFeel::panelBorder());
            g.drawRoundedRectangle(bounds.reduced(0.5f), cornerR, 1.0f);
            break;
        }
    }
}

// ---------------------------------------------------------------------------
// resized — layout
// ---------------------------------------------------------------------------

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

    // At this point bounds is the content area (approx 364px × 198px)

    int modelIdx = modelBox.getSelectedId() - 1;
    CompressorModel model = CompressorModel::ModelManual;
    if (modelIdx >= 0 && modelIdx < static_cast<int>(CompressorModel::NumModels))
        model = static_cast<CompressorModel>(modelIdx);

    const int textBoxH = 14;
    const int labelH   = 12;
    const int gap      = 2;

    // Slider and label arrays for convenience
    juce::Slider* sliders[] = {
        &thresholdSlider, &ratioSlider,
        &attackSlider,    &releaseSlider,
        &kneeSlider,      &mixSlider
    };
    juce::Label* labels[] = {
        &thresholdLabel, &ratioLabel,
        &attackLabel,    &releaseLabel,
        &kneeLabel,      &mixLabel
    };

    auto placeKnob = [&](juce::Slider& sl, juce::Label& lbl,
                         int cellX, int cellY, int cellW, int cellH,
                         int knobSize)
    {
        const int totalH  = knobSize + textBoxH + gap + labelH;
        const int centreX = cellX + cellW / 2;
        const int startY  = cellY + (cellH - totalH) / 2;

        sl .setBounds(centreX - knobSize / 2, startY, knobSize, knobSize + textBoxH);
        lbl.setBounds(centreX - knobSize / 2, startY + knobSize + textBoxH + gap, knobSize, labelH);
    };

    switch (model)
    {
        // ------------------------------------------------------------------
        case CompressorModel::Model1176:
        {
            const int innerW = bounds.getWidth();

            // GR meter: horizontal strip at top of content area (28px tall)
            const int grH   = 28;
            auto grBounds   = bounds.removeFromTop(grH);
            grMeter.setBounds(grBounds);
            bounds.removeFromTop(6);

            // Large knobs area (148px): threshold (left) + mix (right)
            const int largeAreaH  = 148;
            auto largeArea        = bounds.removeFromTop(largeAreaH);
            const int halfW       = innerW / 2;
            const int largeKnob   = juce::jmin(halfW - 8, largeAreaH - textBoxH - gap - labelH - 4);

            placeKnob(thresholdSlider, thresholdLabel,
                      largeArea.getX(),          largeArea.getY(), halfW, largeAreaH, largeKnob);
            placeKnob(mixSlider, mixLabel,
                      largeArea.getX() + halfW,  largeArea.getY(), halfW, largeAreaH, largeKnob);

            bounds.removeFromTop(6);

            // Small knobs row: ratio, attack, release, knee (4 equal cells)
            const int smallAreaH = bounds.getHeight();
            const int cellW4     = innerW / 4;
            const int smallKnob  = juce::jmin(cellW4 - 8, smallAreaH - textBoxH - gap - labelH - 4);

            juce::Slider* smallSliders[] = { &ratioSlider, &attackSlider, &releaseSlider, &kneeSlider };
            juce::Label*  smallLabels[]  = { &ratioLabel,  &attackLabel,  &releaseLabel,  &kneeLabel  };

            for (int i = 0; i < 4; i++)
            {
                placeKnob(*smallSliders[i], *smallLabels[i],
                          bounds.getX() + i * cellW4, bounds.getY(),
                          cellW4, smallAreaH, smallKnob);
            }
            break;
        }

        // ------------------------------------------------------------------
        case CompressorModel::ModelLA2A:
        {
            // grMeter is hidden; hide it off-screen
            grMeter.setBounds(0, -100, 0, 0);

            const int innerW     = bounds.getWidth();
            const int innerH     = bounds.getHeight();

            // Top 130px: VU area — no components here, just paint()
            bounds.removeFromTop(130);

            // Small knobs row (80px): ratio, attack, release, knee
            const int smallH   = 80;
            auto smallArea     = bounds.removeFromTop(smallH);
            const int cellW4   = innerW / 4;
            const int smallKnob = juce::jmin(cellW4 - 8, smallH - textBoxH - gap - labelH - 4);

            juce::Slider* smallSliders[] = { &ratioSlider, &attackSlider, &releaseSlider, &kneeSlider };
            juce::Label*  smallLabels[]  = { &ratioLabel,  &attackLabel,  &releaseLabel,  &kneeLabel  };

            for (int i = 0; i < 4; i++)
            {
                placeKnob(*smallSliders[i], *smallLabels[i],
                          smallArea.getX() + i * cellW4, smallArea.getY(),
                          cellW4, smallH, smallKnob);
            }

            bounds.removeFromTop(6);

            // Large knobs (remaining): threshold (left), mix (right)
            const int largeH    = juce::jmax(bounds.getHeight(), 140);
            const int halfW     = innerW / 2;
            const int largeKnob = juce::jmin(halfW - 8, largeH - textBoxH - gap - labelH - 4);

            placeKnob(thresholdSlider, thresholdLabel,
                      bounds.getX(),         bounds.getY(), halfW, bounds.getHeight(), largeKnob);
            placeKnob(mixSlider, mixLabel,
                      bounds.getX() + halfW, bounds.getY(), halfW, bounds.getHeight(), largeKnob);

            (void) innerH;
            break;
        }

        // ------------------------------------------------------------------
        default:  // Destressor, DBX, Neve, SSL, Manual — standard 2×3 grid
        {
            // GR meter: right strip
            auto grStrip = bounds.removeFromRight(20);
            grMeter.setBounds(grStrip);
            bounds.removeFromRight(3);

            const int knobAreaW = bounds.getWidth();
            const int knobAreaH = bounds.getHeight();
            const int cols      = 2;
            const int rows      = 3;
            const int cellW     = knobAreaW / cols;
            const int cellH     = knobAreaH / rows;
            const int knobSize  = juce::jmin(cellW - 8, cellH - textBoxH - gap - labelH - 4);

            for (int i = 0; i < 6; i++)
            {
                int col = i % cols;
                int row = i / cols;
                placeKnob(*sliders[i], *labels[i],
                          bounds.getX() + col * cellW, bounds.getY() + row * cellH,
                          cellW, cellH, knobSize);
            }
            break;
        }
    }
}
