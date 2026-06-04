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
    juce::ColourGradient screwGrad(
        juce::Colour(0xFFCCCCCC), cx - r * 0.4f, cy - r * 0.4f,
        juce::Colour(0xFF888888), cx + r * 0.4f, cy + r * 0.4f,
        false);
    g.setGradientFill(screwGrad);
    g.fillEllipse(cx - r, cy - r, r * 2.0f, r * 2.0f);

    g.setColour(juce::Colour(0xFF555555));
    g.drawEllipse(cx - r, cy - r, r * 2.0f, r * 2.0f, 0.8f);

    g.setColour(juce::Colour(0xFF444444));
    const float slotLen = r * 0.65f;
    const float slotW   = 0.9f;
    g.drawLine(cx, cy - slotLen, cx, cy + slotLen, slotW);
    g.drawLine(cx - slotLen, cy, cx + slotLen, cy, slotW);
}

static void setKnobStyle(juce::Slider& s, int style)
{
    s.getProperties().set("knobStyle", style);
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

static void drawRatioButton(juce::Graphics& g, juce::Rectangle<int> r, bool active,
                            juce::Colour accent, const juce::String& label, int style = 0)
{
    auto rf = r.toFloat();

    if (style == 1)
    {
        // Style 1: 1176 brushed-metal pushbuttons
        const float cr = 3.0f;
        if (active)
        {
            // Pressed: darker inset
            juce::ColourGradient mg(juce::Colour(0xFF505050), rf.getX(), rf.getY(),
                                     juce::Colour(0xFF383838), rf.getX(), rf.getBottom(), false);
            g.setGradientFill(mg);
            g.fillRoundedRectangle(rf, cr);
            // Amber LED indicator dot at top-centre
            const float dotR = 3.5f;
            g.setColour(juce::Colour(0xFFFF8800).withAlpha(0.95f));
            g.fillEllipse(rf.getCentreX() - dotR, rf.getY() + 2.5f, dotR*2.0f, dotR*2.0f);
            g.setColour(juce::Colour(0xFFFFCC44).withAlpha(0.5f)); // glow
            g.fillEllipse(rf.getCentreX() - dotR*2.0f, rf.getY() + 0.5f, dotR*4.0f, dotR*3.5f);
        }
        else
        {
            // Raised metal look
            juce::ColourGradient mg(juce::Colour(0xFF909090), rf.getX(), rf.getY(),
                                     juce::Colour(0xFF606060), rf.getX(), rf.getBottom(), false);
            g.setGradientFill(mg);
            g.fillRoundedRectangle(rf, cr);
            // Unlit LED dot
            const float dotR = 3.5f;
            g.setColour(juce::Colour(0xFF442200));
            g.fillEllipse(rf.getCentreX() - dotR, rf.getY() + 2.5f, dotR*2.0f, dotR*2.0f);
        }
        // Chamfered border
        g.setColour(active ? juce::Colour(0xFF303030) : juce::Colour(0xFF404040));
        g.drawRoundedRectangle(rf.reduced(0.5f), cr, 1.0f);
        // Number label in bottom half
        const auto labelR = r.withTrimmedTop(r.getHeight() / 2);
        g.setColour(active ? juce::Colours::black : juce::Colour(0xFF111111));
        g.setFont(juce::Font(9.0f, juce::Font::bold));
        g.drawText(label, labelR, juce::Justification::centred);
    }
    else
    {
        // Style 0 (default): Distressor LED lamp buttons
        const float cornerR = 3.0f;

        if (active)
        {
            // Outer glow
            g.setColour(accent.withAlpha(0.35f));
            g.fillRoundedRectangle(rf.expanded(2.0f), cornerR + 1.0f);

            g.setColour(accent);
            g.fillRoundedRectangle(rf, cornerR);

            // LED highlight top
            juce::ColourGradient highlight(juce::Colours::white.withAlpha(0.4f), rf.getX(), rf.getY(),
                                           juce::Colours::transparentBlack, rf.getX(), rf.getCentreY(),
                                           false);
            g.setGradientFill(highlight);
            g.fillRoundedRectangle(rf, cornerR);

            g.setColour(accent.brighter(0.5f));
            g.drawRoundedRectangle(rf.reduced(0.5f), cornerR, 1.0f);

            g.setColour(juce::Colours::black.withAlpha(0.9f));
        }
        else
        {
            g.setColour(juce::Colour(0xFF222222));
            g.fillRoundedRectangle(rf, cornerR);

            g.setColour(juce::Colour(0xFF444444));
            g.drawRoundedRectangle(rf.reduced(0.5f), cornerR, 1.0f);

            g.setColour(juce::Colour(0xFF888888));
        }

        g.setFont(juce::Font(9.5f, juce::Font::bold));
        g.drawText(label, r, juce::Justification::centred);
    }
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
    setupSlider(satSlider,       satLabel,       "SAT");
    setupSlider(mixSlider,       mixLabel,       "MIX");

    thresholdSlider.textFromValueFunction = [](double v) { return juce::String(v, 1) + " dB"; };
    ratioSlider.textFromValueFunction     = [](double v) { return juce::String(v, 1) + ":1"; };
    attackSlider.textFromValueFunction    = [](double v) { return juce::String(v, 2) + " ms"; };
    releaseSlider.textFromValueFunction   = [](double v) {
        return juce::String(v < 1000.0 ? v : v / 1000.0, 2) + (v < 1000.0 ? " ms" : " s");
    };
    satSlider.textFromValueFunction = [](double v) {
        return juce::String(juce::roundToInt(v * 100.0)) + "%";
    };
    mixSlider.textFromValueFunction = [](double v) {
        return juce::String(juce::roundToInt(v * 100.0)) + "%";
    };

    thresholdAtt = std::make_unique<SliderAttachment>(apvts, pid("threshold"), thresholdSlider);
    ratioAtt     = std::make_unique<SliderAttachment>(apvts, pid("ratio"),     ratioSlider);
    attackAtt    = std::make_unique<SliderAttachment>(apvts, pid("attack"),    attackSlider);
    releaseAtt   = std::make_unique<SliderAttachment>(apvts, pid("release"),   releaseSlider);
    satAtt       = std::make_unique<SliderAttachment>(apvts, pid("sat"),       satSlider);
    mixAtt       = std::make_unique<SliderAttachment>(apvts, pid("mix"),       mixSlider);

    addAndMakeVisible(grMeter);

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
    auto model = static_cast<CompressorModel>(modelBox.getSelectedItemIndex());
    auto d = getModelDefaults(model);
    thresholdSlider.setValue(d.threshold,         juce::dontSendNotification);
    ratioSlider    .setValue(d.ratio,             juce::dontSendNotification);
    attackSlider   .setValue(d.attackMs,          juce::dontSendNotification);
    releaseSlider  .setValue(d.releaseMs,         juce::dontSendNotification);
    satSlider      .setValue(d.saturationDefault, juce::dontSendNotification);
    mixSlider      .setValue(d.mix,               juce::dontSendNotification);

    // For models with hidden controls, lock them to authentic fixed values
    switch (model)
    {
        case CompressorModel::Model1176:
            // 1176 has no threshold knob — INPUT gain drives compression;
            // fix threshold at a sensible point, mix = 100% (no parallel blend)
            thresholdSlider.setValue(-20.0, juce::dontSendNotification);
            mixSlider      .setValue(1.0,   juce::dontSendNotification);
            break;
        case CompressorModel::ModelLA2A:
            // Optical: ratio/attack/release are program-dependent — fix them
            ratioSlider  .setValue(3.5,   juce::dontSendNotification); // ~3:1 compress
            attackSlider .setValue(10.0,  juce::dontSendNotification); // slow optical attack
            releaseSlider.setValue(500.0, juce::dontSendNotification); // slow opto release
            mixSlider    .setValue(1.0,   juce::dontSendNotification);
            break;
        case CompressorModel::ModelDBX160:
            // VCA: attack/release are automatic in the hardware
            attackSlider .setValue(0.1,  juce::dontSendNotification); // near-instant
            releaseSlider.setValue(80.0, juce::dontSendNotification); // fast auto
            satSlider    .setValue(0.1,  juce::dontSendNotification); // clean
            mixSlider    .setValue(1.0,  juce::dontSendNotification);
            break;
        default:
            break;
    }
}

// ---------------------------------------------------------------------------
// updateModelAppearance
// ---------------------------------------------------------------------------

void StageComponent::updateModelAppearance()
{
    auto model = static_cast<CompressorModel>(modelBox.getSelectedItemIndex());

    // Label texts per model
    struct LabelTexts { const char* thr; const char* ratio; const char* atk; const char* rel; const char* sat; const char* mix; };
    LabelTexts texts = {};

    switch (model)
    {
        case CompressorModel::Model1176:
            texts = { "THR", "RATIO", "ATTACK", "RELEASE", "INPUT", "MIX" };
            break;
        case CompressorModel::ModelLA2A:
            texts = { "PEAK RED.", "RATIO", "ATTACK", "RELEASE", "DRIVE", "MIX" };
            break;
        case CompressorModel::ModelDestressor:
            texts = { "THRESH", "RATIO", "ATK", "REL", "INPUT", "MIX" };
            break;
        case CompressorModel::ModelDBX160:
            texts = { "THRESHOLD", "COMP", "ATTACK", "RELEASE", "SAT", "MIX" };
            break;
        case CompressorModel::ModelNeve:
            texts = { "THRESHOLD", "RATIO", "ATK TIME", "RECOVERY", "DRIVE", "MIX" };
            break;
        case CompressorModel::ModelSSL:
            texts = { "THRESHOLD", "RATIO", "ATTACK", "RELEASE", "DRIVE", "MIX" };
            break;
        case CompressorModel::ModelManual:
        default:
            texts = { "THR", "RATIO", "ATK", "REL", "SAT", "MIX" };
            break;
    }

    thresholdLabel.setText(texts.thr,   juce::dontSendNotification);
    ratioLabel    .setText(texts.ratio, juce::dontSendNotification);
    attackLabel   .setText(texts.atk,   juce::dontSendNotification);
    releaseLabel  .setText(texts.rel,   juce::dontSendNotification);
    mixLabel      .setText(texts.mix,   juce::dontSendNotification);

    // SAT label: matches what's visible on the real hardware
    switch (model)
    {
        case CompressorModel::Model1176:
        case CompressorModel::ModelDestressor:
            satLabel.setText("INPUT", juce::dontSendNotification);
            break;
        case CompressorModel::ModelLA2A:
        case CompressorModel::ModelNeve:
        case CompressorModel::ModelSSL:
            satLabel.setText("DRIVE", juce::dontSendNotification);
            break;
        default:
            satLabel.setText("SAT", juce::dontSendNotification);
            break;
    }

    // Knob arc colours per model
    struct ModelColors { juce::Colour fill; juce::Colour track; };
    ModelColors mc = {};
    switch (model)
    {
        case CompressorModel::Model1176:
            mc = { juce::Colour(0xFFCC6622), juce::Colour(0xFF1A2A4A) };
            break;
        case CompressorModel::ModelLA2A:
            mc = { juce::Colour(0xFF44AA44), juce::Colour(0xFF445533) };
            break;
        case CompressorModel::ModelDestressor:
            mc = { juce::Colour(0xFF44AA88), juce::Colour(0xFF998877) };
            break;
        case CompressorModel::ModelDBX160:
            mc = { juce::Colour(0xFFCC5500), juce::Colour(0xFF332211) };
            break;
        case CompressorModel::ModelNeve:
            mc = { juce::Colour(0xFFEE7722), juce::Colour(0xFF2A2A30) };
            break;
        case CompressorModel::ModelSSL:
            mc = { juce::Colour(0xFFAACC22), juce::Colour(0xFF778877) };
            break;
        default:
            mc = { juce::Colour(0xFF4488CC), juce::Colour(0xFF222233) };
            break;
    }

    juce::Slider* sliders[] = {
        &thresholdSlider, &ratioSlider, &attackSlider,
        &releaseSlider,   &satSlider,   &mixSlider
    };
    for (auto* s : sliders)
    {
        s->setColour(juce::Slider::rotarySliderFillColourId,    mc.fill);
        s->setColour(juce::Slider::rotarySliderOutlineColourId, mc.track);
    }

    // After the per-slider fill/track colour loop, set knob styles:
    switch (model)
    {
        case CompressorModel::Model1176:
            for (auto* s : { &thresholdSlider, &ratioSlider, &attackSlider,
                             &releaseSlider,   &satSlider,   &mixSlider })
                setKnobStyle(*s, 1);  // vintage-bakelite
            break;
        case CompressorModel::ModelLA2A:
            for (auto* s : { &thresholdSlider, &ratioSlider, &attackSlider,
                             &releaseSlider,   &satSlider,   &mixSlider })
                setKnobStyle(*s, 2);  // la2a-cream
            break;
        case CompressorModel::ModelDBX160:
            for (auto* s : { &thresholdSlider, &ratioSlider, &attackSlider,
                             &releaseSlider,   &satSlider,   &mixSlider })
                setKnobStyle(*s, 4);  // dbx-rubber
            break;
        case CompressorModel::ModelNeve:
        case CompressorModel::ModelSSL:
            for (auto* s : { &thresholdSlider, &ratioSlider, &attackSlider,
                             &releaseSlider,   &satSlider,   &mixSlider })
                setKnobStyle(*s, 3);  // neve-console
            break;
        case CompressorModel::ModelDestressor:
            for (auto* s : { &thresholdSlider, &ratioSlider, &attackSlider,
                             &releaseSlider,   &satSlider,   &mixSlider })
                setKnobStyle(*s, 1);  // vintage-bakelite (with teal accent already set)
            break;
        default:
            for (auto* s : { &thresholdSlider, &ratioSlider, &attackSlider,
                             &releaseSlider,   &satSlider,   &mixSlider })
                setKnobStyle(*s, 0);  // default
            break;
    }

    juce::Colour txtCol = panelTextColour(model);
    juce::Label* labels[] = {
        &thresholdLabel, &ratioLabel, &attackLabel,
        &releaseLabel,   &satLabel,   &mixLabel
    };
    for (auto* lbl : labels)
        lbl->setColour(juce::Label::textColourId, txtCol);

    stageLabel.setColour(juce::Label::textColourId, txtCol);

    for (auto* s : sliders)
        s->setColour(juce::Slider::textBoxTextColourId, txtCol);

    // GR meter orientation
    if (model == CompressorModel::Model1176)
    {
        grMeter.setHorizontal(true);
        grMeter.setVisible(false);  // VU needle drawn in paint()
    }
    else if (model == CompressorModel::ModelLA2A)
    {
        grMeter.setVisible(false);
    }
    else if (model == CompressorModel::ModelNeve)
    {
        grMeter.setHorizontal(false);
        grMeter.setVisible(false);  // VU needle drawn in paint()
    }
    else
    {
        grMeter.setHorizontal(false);
        grMeter.setVisible(true);
    }

    if (getWidth() > 0) { numRatioButtons = 0; resized(); }
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
    auto model = static_cast<CompressorModel>(modelBox.getSelectedItemIndex());
    if (model == CompressorModel::ModelLA2A
     || model == CompressorModel::Model1176
     || model == CompressorModel::ModelNeve)
        repaint();
}

juce::String StageComponent::pid(const juce::String& param) const
{
    return "s" + juce::String(stageIndex) + "_" + param;
}

// ---------------------------------------------------------------------------
// mouseDown
// ---------------------------------------------------------------------------

void StageComponent::mouseDown(const juce::MouseEvent& e)
{
    if (numRatioButtons == 0) return;

    auto model = static_cast<CompressorModel>(modelBox.getSelectedItemIndex());

    const float vals1176[]      = { 2.0f, 4.0f, 8.0f, 12.0f, 20.0f };
    const float valsDistressor[] = { 1.0f, 2.0f, 4.0f, 6.0f, 10.0f, 20.0f };
    const float* vals = (model == CompressorModel::Model1176) ? vals1176 : valsDistressor;

    for (int i = 0; i < numRatioButtons; i++)
    {
        if (ratioButtonRects[i].contains(e.getPosition()))
        {
            ratioSlider.setValue(vals[i], juce::sendNotification);
            repaint();
            return;
        }
    }
    juce::Component::mouseDown(e);
}

// ---------------------------------------------------------------------------
// paint  — model-specific backgrounds and decorations
// ---------------------------------------------------------------------------

void StageComponent::paint(juce::Graphics& g)
{
    auto bounds = getLocalBounds().toFloat();

    auto model = static_cast<CompressorModel>(modelBox.getSelectedItemIndex());

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
                juce::Colour(0xFF8888AA), 0.0f, 0.0f,
                juce::Colour(0xFFAAAAAA), 0.0f, H,
                false);
            g.setGradientFill(bg);
            g.fillRoundedRectangle(bounds, cornerR);

            // Subtle horizontal brush lines every 4px
            for (int lineY = 0; lineY < (int) H; lineY += 4)
            {
                g.setColour(juce::Colour(0x0CFFFFFF));
                g.drawHorizontalLine(lineY, 0.0f, W);
            }

            // Navy blue top stripe (36px)
            g.setColour(juce::Colour(0xFF1A2A4A));
            g.fillRect(0.0f, 0.0f, W, 36.0f);

            // "1176LN" text in stripe
            g.setColour(juce::Colours::white);
            g.setFont(juce::Font(14.0f, juce::Font::bold));
            g.drawText("1176LN", 0, 0, (int) W, 36, juce::Justification::centred);

            // Corner screws on stripe
            drawScrew(g, 8.0f,     8.0f);
            drawScrew(g, W - 8.0f, 8.0f);

            // Bottom panel screws
            drawScrew(g, 8.0f,     H - 8.0f);
            drawScrew(g, W - 8.0f, H - 8.0f);

            // Draw ratio buttons
            {
                const juce::String ratioLabels[] = { "2:1", "4:1", "8:1", "12:1", "20:1" };
                const float        ratioVals[]   = { 2.0f,  4.0f,  8.0f,  12.0f,  20.0f };
                const juce::Colour accent(0xFFCC7722);
                for (int i = 0; i < numRatioButtons && i < 5; i++)
                {
                    bool active = (std::abs((float) ratioSlider.getValue() - ratioVals[i]) < 0.5f);
                    drawRatioButton(g, ratioButtonRects[i], active, accent, ratioLabels[i], 1);
                }
            }

            // VU needle meter for 1176 (bounds match resized() layout)
            {
                const int pad = 8;
                auto mR = juce::Rectangle<float>((float)pad, (float)(H - 104),
                                                  (float)(W - pad*2), 94.0f);

                // Black face with silver border
                g.setColour(juce::Colour(0xFF080808));
                g.fillRoundedRectangle(mR, 4.0f);
                g.setColour(juce::Colour(0xFF909090));
                g.drawRoundedRectangle(mR.reduced(0.5f), 4.0f, 1.5f);

                // "GAIN REDUCTION" label inside
                g.setColour(juce::Colour(0xFF888888));
                g.setFont(juce::Font(7.5f, juce::Font::bold));
                g.drawText("GAIN REDUCTION", mR.removeFromTop(13).toNearestInt(),
                           juce::Justification::centred);

                // Pivot at bottom centre
                const float pivotX = mR.getCentreX();
                const float pivotY = mR.getBottom() - 6.0f;
                const float arcR   = mR.getHeight() * 1.35f;

                // Scale arc: sweeps from right-upward (0 GR) to left-upward (max GR)
                // angMin = 0 GR (needle at ~-40° from vertical = rightward)
                // angMax = 20 dB GR (needle at ~+40° from vertical = leftward)
                const float angMin = -0.70f;   // radians from 12-o-clock, right side
                const float angMax =  0.70f;   // radians from 12-o-clock, left side
                // NOTE: sin(angMin)<0 → right, sin(angMax)>0 → left; BUT on screen:
                //       x = pivotX + r*sin(a)  → positive sin = rightward
                //       y = pivotY - r*cos(a)  → negative cos at small |a| = upward
                // We want 0 GR = needle points to the RIGHT → angle = +angMin_right
                // Let's define: angle = +0.65 at 0 GR (right side), -0.65 at max GR (left)
                const float angZero =  0.65f;
                const float angFull = -0.65f;

                // Track arc
                juce::Path trackArc;
                trackArc.addArc(pivotX - arcR, pivotY - arcR, arcR*2.0f, arcR*2.0f,
                                juce::MathConstants<float>::pi + angFull,
                                juce::MathConstants<float>::pi + angZero, true);
                g.setColour(juce::Colour(0xFF333333));
                g.strokePath(trackArc, juce::PathStrokeType(1.2f));

                // Scale marks and labels along the arc
                g.setFont(juce::Font(6.5f));
                const float grMarks[] = { 0.0f, -2.0f, -4.0f, -7.0f, -10.0f, -14.0f, -20.0f };
                const char* grLabels[] = { "0", "2", "4", "7", "10", "14", "20" };
                for (int mi = 0; mi < 7; mi++)
                {
                    float frac = juce::jmap(grMarks[mi], 0.0f, -20.0f, 0.0f, 1.0f);
                    float tickAng = angZero + frac * (angFull - angZero);
                    float tx = pivotX + arcR * std::sin(tickAng);
                    float ty = pivotY - arcR * std::cos(tickAng);
                    float tx2 = pivotX + (arcR - 7.0f) * std::sin(tickAng);
                    float ty2 = pivotY - (arcR - 7.0f) * std::cos(tickAng);
                    g.setColour(juce::Colour(0xFFAAAAAA));
                    g.drawLine(tx, ty, tx2, ty2, 1.0f);
                    // Label at selected marks (0, -7, -14, -20)
                    if (mi == 0 || mi == 3 || mi == 5 || mi == 6)
                    {
                        float lx = pivotX + (arcR - 15.0f) * std::sin(tickAng);
                        float ly = pivotY - (arcR - 15.0f) * std::cos(tickAng);
                        g.setColour(juce::Colour(0xFF999999));
                        g.drawText(grLabels[mi],
                                   (int)(lx - 8), (int)(ly - 5), 16, 10,
                                   juce::Justification::centred);
                    }
                }

                // Needle
                float grFrac = juce::jlimit(0.0f, 1.0f, currentGainReductionDB / 20.0f);
                float needleAng = angZero + grFrac * (angFull - angZero);
                float nx = pivotX + (arcR - 4.0f) * std::sin(needleAng);
                float ny = pivotY - (arcR - 4.0f) * std::cos(needleAng);
                g.setColour(juce::Colour(0xFFEE8833));
                g.drawLine(pivotX, pivotY, nx, ny, 1.8f);
                // Pivot dot
                g.setColour(juce::Colour(0xFF808080));
                g.fillEllipse(pivotX - 3.5f, pivotY - 3.5f, 7.0f, 7.0f);
            }

            // Border
            g.setColour(juce::Colour(0xFF888899));
            g.drawRoundedRectangle(bounds.reduced(0.5f), cornerR, 1.0f);
            break;
        }

        // ------------------------------------------------------------------
        case CompressorModel::ModelLA2A:
        {
            // Warm olive/gray gradient
            juce::ColourGradient bg(
                juce::Colour(0xFF7A7868), 0.0f, 0.0f,
                juce::Colour(0xFF6A6858), 0.0f, H,
                false);
            g.setGradientFill(bg);
            g.fillRoundedRectangle(bounds, cornerR);

            // Top stripe
            g.setColour(juce::Colour(0xFF5A5848));
            g.fillRect(0.0f, 0.0f, W, 36.0f);

            // "LA-2A" text in stripe
            g.setColour(juce::Colour(0xFFEEDDBB));
            g.setFont(juce::Font(14.0f, juce::Font::bold));
            g.drawText("LA-2A", 0, 0, (int) W, 36, juce::Justification::centred);

            // Corner screws
            drawScrew(g, 8.0f,     8.0f);
            drawScrew(g, W - 8.0f, 8.0f);
            drawScrew(g, 8.0f,     H - 8.0f);
            drawScrew(g, W - 8.0f, H - 8.0f);

            // VU meter area: middle 170px starting at y=122
            const float vuY = 122.0f;
            const float vuH = 170.0f;
            auto vuRect = juce::Rectangle<float>(6.0f, vuY, W - 12.0f, vuH);

            // Dark oval inset
            g.setColour(juce::Colour(0xFF2A2820));
            g.fillRoundedRectangle(vuRect, 6.0f);

            // "GR" label
            g.setFont(juce::Font(8.0f, juce::Font::bold));
            g.setColour(juce::Colour(0xFF888877));
            g.drawText("GR", (int) vuRect.getX() + 6, (int) vuRect.getY() + 5,
                       24, 10, juce::Justification::centredLeft);

            // Semi-circular VU arc (180° sweep)
            const float pivotX = vuRect.getCentreX();
            const float pivotY = vuRect.getBottom() - 14.0f;
            const float arcR   = juce::jmin(vuRect.getWidth() * 0.40f,
                                            vuRect.getHeight() * 0.85f);
            // Arc from ~210° to ~330° (bottom of circle, sweeping right)
            const float arcStart = 3.93f;  // ~225 deg (lower-left)
            const float arcEnd   = 5.50f;  // ~315 deg (lower-right)

            // Track arc
            juce::Path trackArc;
            trackArc.addArc(pivotX - arcR, pivotY - arcR,
                            arcR * 2.0f, arcR * 2.0f,
                            arcStart, arcEnd, true);
            g.setColour(juce::Colour(0xFF444433));
            g.strokePath(trackArc, juce::PathStrokeType(3.0f));

            // Scale ticks (0, -2, -4, -6, -8, -10, -14 dB GR)
            const float dbTicks[] = { 0.0f, 2.0f, 4.0f, 6.0f, 8.0f, 10.0f, 14.0f };
            for (int ti = 0; ti < 7; ti++)
            {
                float db  = dbTicks[ti];
                float ang = arcStart + (db / 20.0f) * (arcEnd - arcStart);
                float tx  = pivotX + arcR * std::cos(ang);
                float ty  = pivotY + arcR * std::sin(ang);
                float tx2 = pivotX + (arcR - 7.0f) * std::cos(ang);
                float ty2 = pivotY + (arcR - 7.0f) * std::sin(ang);
                g.setColour(juce::Colour(0xFFAAAAAA));
                g.drawLine(tx, ty, tx2, ty2, 1.0f);

                float lx = pivotX + (arcR - 15.0f) * std::cos(ang);
                float ly = pivotY + (arcR - 15.0f) * std::sin(ang);
                g.setFont(juce::Font(6.5f));
                g.setColour(juce::Colour(0xFF999988));
                g.drawText(juce::String((int) db),
                           (int) lx - 6, (int) ly - 5, 12, 8,
                           juce::Justification::centred);
            }

            // VU needle
            {
                float needleDb  = juce::jlimit(0.0f, 20.0f, currentGainReductionDB);
                float needleAng = arcStart + (needleDb / 20.0f) * (arcEnd - arcStart);
                float tipX = pivotX + (arcR - 2.0f) * std::cos(needleAng);
                float tipY = pivotY + (arcR - 2.0f) * std::sin(needleAng);

                g.setColour(juce::Colours::white.withAlpha(0.9f));
                g.drawLine(pivotX, pivotY, tipX, tipY, 1.5f);

                juce::Path tip;
                const float tipSz = 3.5f;
                tip.addTriangle(tipX, tipY,
                    tipX + tipSz * std::cos(needleAng + 0.4f),
                    tipY + tipSz * std::sin(needleAng + 0.4f),
                    tipX + tipSz * std::cos(needleAng - 0.4f),
                    tipY + tipSz * std::sin(needleAng - 0.4f));
                g.setColour(juce::Colour(0xFFCC2222));
                g.fillPath(tip);

                g.setColour(juce::Colour(0xFFCCCCCC));
                g.fillEllipse(pivotX - 3.0f, pivotY - 3.0f, 6.0f, 6.0f);
            }

            // Bottom label strip
            auto bottomRect = juce::Rectangle<float>(6.0f, vuRect.getBottom() + 2.0f,
                                                     W - 12.0f, 18.0f);
            g.setColour(juce::Colour(0xFF3A3828));
            g.fillRoundedRectangle(bottomRect, 3.0f);
            g.setColour(juce::Colour(0xFF999988));
            g.setFont(juce::Font(7.5f));
            g.drawText("PEAK REDUCTION", (int) bottomRect.getX(), (int) bottomRect.getY(),
                       (int) bottomRect.getWidth() / 2, (int) bottomRect.getHeight(),
                       juce::Justification::centredLeft);
            g.drawText("GAIN", (int) bottomRect.getCentreX(), (int) bottomRect.getY(),
                       (int) bottomRect.getWidth() / 2, (int) bottomRect.getHeight(),
                       juce::Justification::centredRight);

            // Border
            g.setColour(juce::Colour(0xFF777770));
            g.drawRoundedRectangle(bounds.reduced(0.5f), cornerR, 1.0f);
            break;
        }

        // ------------------------------------------------------------------
        case CompressorModel::ModelDestressor:
        {
            // Cream/off-white background
            g.setColour(juce::Colour(0xFFEEE8D8));
            g.fillRoundedRectangle(bounds, cornerR);

            // Slightly darker right column background
            g.setColour(juce::Colour(0xFFE0DAC8));
            g.fillRect(106.0f, 0.0f, W - 106.0f, H);

            // Teal/green top stripe
            g.setColour(juce::Colour(0xFF2A7A6A));
            g.fillRect(0.0f, 0.0f, W, 36.0f);

            // "DISTRESSOR" text in stripe
            g.setColour(juce::Colours::white);
            g.setFont(juce::Font(12.0f, juce::Font::bold));
            g.drawText("DISTRESSOR", 0, 2, (int) W, 20, juce::Justification::centred);

            // "EMPIRICAL LABS" small text
            g.setColour(juce::Colours::white.withAlpha(0.7f));
            g.setFont(juce::Font(7.5f));
            g.drawText("EMPIRICAL LABS", 0, 22, (int) W, 12, juce::Justification::centred);

            // Corner screws
            drawScrew(g, 8.0f,     8.0f);
            drawScrew(g, W - 8.0f, 8.0f);
            drawScrew(g, 8.0f,     H - 8.0f);
            drawScrew(g, W - 8.0f, H - 8.0f);

            // Ratio buttons (right column)
            {
                const juce::String ratioLabels[] = { "1:1", "2:1", "4:1", "6:1", "10:1", "20:1" };
                const float        ratioVals[]   = { 1.0f,  2.0f,  4.0f,  6.0f,  10.0f,  20.0f };
                const juce::Colour accent(0xFF44AA88);
                for (int i = 0; i < numRatioButtons && i < 6; i++)
                {
                    bool active = (std::abs((float) ratioSlider.getValue() - ratioVals[i]) < 0.5f);
                    drawRatioButton(g, ratioButtonRects[i], active, accent, ratioLabels[i]);
                }
            }

            // Panel border (tan/brown)
            g.setColour(juce::Colour(0xFFAA9980));
            g.drawRoundedRectangle(bounds.reduced(0.5f), cornerR, 1.0f);
            break;
        }

        // ------------------------------------------------------------------
        case CompressorModel::ModelDBX160:
        {
            // Flat black background
            g.setColour(juce::Colour(0xFF111111));
            g.fillRoundedRectangle(bounds, cornerR);

            // Orange accent bar top (48px)
            g.setColour(juce::Colour(0xFFCC5500));
            g.fillRect(0.0f, 0.0f, W, 48.0f);

            // "dbx" bold in bar
            g.setColour(juce::Colour(0xFFFF3300));
            g.setFont(juce::Font(18.0f, juce::Font::bold));
            g.drawText("dbx", 8, 4, (int) W / 2, 24, juce::Justification::centredLeft);

            // "MODEL 160" small white text
            g.setColour(juce::Colours::white);
            g.setFont(juce::Font(9.0f));
            g.drawText("MODEL 160", 8, 30, (int) W - 16, 14, juce::Justification::centredLeft);

            // LED bargraph GR meter (bottom 110px)
            {
                const int meterX = 8;
                const int meterY = (int) H - 118;
                const int meterW = (int) W - 16;
                const int segH   = 8;
                const int segGap = 1;
                const int numSegs = 20;

                for (int seg = 0; seg < numSegs; seg++)
                {
                    // seg 0 = top (most GR), seg 19 = bottom (least GR)
                    float segDb = ((numSegs - seg) / (float) numSegs) * 20.0f;
                    bool lit = (currentGainReductionDB >= segDb - 1.0f);

                    int y = meterY + seg * (segH + segGap);
                    juce::Colour segColour;
                    if (segDb > 12.0f)
                        segColour = lit ? juce::Colour(0xFFDD2200) : juce::Colour(0xFF2A0800);
                    else if (segDb > 6.0f)
                        segColour = lit ? juce::Colour(0xFFDDCC00) : juce::Colour(0xFF2A2600);
                    else
                        segColour = lit ? juce::Colour(0xFF22CC22) : juce::Colour(0xFF082208);

                    g.setColour(segColour);
                    g.fillRect(meterX, y, meterW, segH);
                }
            }

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
                juce::Colour(0xFF1A1A1E), 0.0f, 0.0f,
                juce::Colour(0xFF222228), 0.0f, H,
                false);
            g.setGradientFill(bg);
            g.fillRoundedRectangle(bounds, cornerR);

            // Orange top stripe
            g.setColour(juce::Colour(0xFFCC5500));
            g.fillRect(0.0f, 0.0f, W, 36.0f);

            // "neve" italic
            g.setColour(juce::Colours::white);
            g.setFont(juce::Font(16.0f, juce::Font::italic));
            g.drawText("neve", 6, 2, 70, 22, juce::Justification::centredLeft);

            // "33609" small text
            g.setColour(juce::Colours::white.withAlpha(0.7f));
            g.setFont(juce::Font(9.0f));
            g.drawText("33609", 6, 22, (int) W - 12, 12, juce::Justification::centredLeft);

            // Compressor section background
            g.setColour(juce::Colour(0xFF282830));
            g.fillRect(4.0f, 40.0f, W - 8.0f, 256.0f);

            // "COMPRESSOR" section label
            g.setColour(juce::Colour(0xFFCC5500));
            g.setFont(juce::Font(8.0f, juce::Font::bold));
            g.drawText("COMPRESSOR", 8, 42, (int) W - 16, 10, juce::Justification::centredLeft);

            // Compressor section divider
            g.setColour(juce::Colour(0xFF444448));
            g.drawHorizontalLine(54, 4.0f, W - 4.0f);

            // Limiter section label
            g.setColour(juce::Colour(0xFFCC5500));
            g.setFont(juce::Font(8.0f, juce::Font::bold));
            g.drawText("LIMITER", 8, 298, (int) W - 16, 10, juce::Justification::centredLeft);

            // Limiter section divider
            g.setColour(juce::Colour(0xFF444448));
            g.drawHorizontalLine(300, 4.0f, W - 4.0f);

            // Section corner screws
            drawScrew(g, 8.0f,     8.0f);
            drawScrew(g, W - 8.0f, 8.0f);
            drawScrew(g, 8.0f,     H - 8.0f);
            drawScrew(g, W - 8.0f, H - 8.0f);

            // Neve VU meter overlay (drawn over grMeter bounds)
            {
                const int pad = 8;
                auto mR = juce::Rectangle<float>((float)pad, (float)(H - 90),
                                                  (float)(W - pad*2), 82.0f);
                g.setColour(juce::Colour(0xFF0C0C0C));
                g.fillRoundedRectangle(mR, 3.0f);
                g.setColour(juce::Colour(0xFFCC5500).withAlpha(0.8f));
                g.drawRoundedRectangle(mR.reduced(0.5f), 3.0f, 1.5f);

                const float pivotX = mR.getCentreX();
                const float pivotY = mR.getBottom() - 5.0f;
                const float arcR   = mR.getHeight() * 1.30f;
                const float angZero =  0.60f;
                const float angFull = -0.60f;

                // Track
                juce::Path track;
                track.addArc(pivotX - arcR, pivotY - arcR, arcR*2.0f, arcR*2.0f,
                             juce::MathConstants<float>::pi + angFull,
                             juce::MathConstants<float>::pi + angZero, true);
                g.setColour(juce::Colour(0xFF2A2A2A));
                g.strokePath(track, juce::PathStrokeType(1.2f));

                // Scale marks (0, -3, -6, -10, -20)
                g.setFont(juce::Font(6.0f));
                const float gm[] = { 0.0f, -3.0f, -6.0f, -10.0f, -20.0f };
                const char* gl[] = { "0", "3", "6", "10", "20" };
                for (int mi = 0; mi < 5; mi++)
                {
                    float frac = juce::jmap(gm[mi], 0.0f, -20.0f, 0.0f, 1.0f);
                    float ta = angZero + frac * (angFull - angZero);
                    float tx  = pivotX + arcR * std::sin(ta);
                    float ty  = pivotY - arcR * std::cos(ta);
                    float tx2 = pivotX + (arcR - 6.0f) * std::sin(ta);
                    float ty2 = pivotY - (arcR - 6.0f) * std::cos(ta);
                    g.setColour(juce::Colour(0xFF886644));
                    g.drawLine(tx, ty, tx2, ty2, 1.0f);
                    if (mi == 0 || mi == 2 || mi == 4)
                    {
                        float lx = pivotX + (arcR - 13.0f) * std::sin(ta);
                        float ly = pivotY - (arcR - 13.0f) * std::cos(ta);
                        g.setColour(juce::Colour(0xFF886644));
                        g.drawText(gl[mi], (int)(lx-7),(int)(ly-4), 14, 9, juce::Justification::centred);
                    }
                }

                // Needle
                float grFrac  = juce::jlimit(0.0f, 1.0f, currentGainReductionDB / 20.0f);
                float nAng    = angZero + grFrac * (angFull - angZero);
                float nx = pivotX + (arcR - 4.0f) * std::sin(nAng);
                float ny = pivotY - (arcR - 4.0f) * std::cos(nAng);
                g.setColour(juce::Colour(0xFFEE6600));
                g.drawLine(pivotX, pivotY, nx, ny, 1.6f);
                g.setColour(juce::Colour(0xFF555555));
                g.fillEllipse(pivotX-3.0f, pivotY-3.0f, 6.0f, 6.0f);
            }

            // Border
            g.setColour(juce::Colour(0xFF404448));
            g.drawRoundedRectangle(bounds.reduced(0.5f), cornerR, 1.0f);
            break;
        }

        // ------------------------------------------------------------------
        case CompressorModel::ModelSSL:
        {
            // Light gray background
            g.setColour(juce::Colour(0xFFD8D8D0));
            g.fillRoundedRectangle(bounds, cornerR);

            // Yellow-green top stripe
            g.setColour(juce::Colour(0xFFAACC22));
            g.fillRect(0.0f, 0.0f, W, 36.0f);

            // "ssl" bold text
            g.setColour(juce::Colour(0xFF111111));
            g.setFont(juce::Font(14.0f, juce::Font::bold));
            g.drawText("ssl", 8, 2, 50, 20, juce::Justification::centredLeft);

            // "G-MASTER BUS COMP" small text
            g.setColour(juce::Colour(0xFF111111).withAlpha(0.8f));
            g.setFont(juce::Font(8.0f));
            g.drawText("G-MASTER BUS COMP", 8, 20, (int) W - 16, 14, juce::Justification::centredLeft);

            // GR meter border (yellow-green)
            {
                int grY = (int) H - 100;
                g.setColour(juce::Colour(0xFFAACC22).withAlpha(0.6f));
                g.drawRect(4, grY - 2, (int) W - 8, 96, 1);
            }

            // Panel border (medium gray)
            g.setColour(juce::Colour(0xFFAAAAAA));
            g.drawRoundedRectangle(bounds.reduced(0.5f), cornerR, 1.0f);
            break;
        }

        // ------------------------------------------------------------------
        case CompressorModel::ModelManual:
        default:
        {
            juce::ColourGradient bg(
                juce::Colour(0xFF1C1C26), 0.0f, 0.0f,
                juce::Colour(0xFF14141E), 0.0f, H,
                false);
            g.setGradientFill(bg);
            g.fillRoundedRectangle(bounds, cornerR);

            // Dark top border stripe
            g.setColour(juce::Colour(0xFF2A2A38));
            g.fillRect(0.0f, 0.0f, W, 36.0f);

            // "MANUAL" text
            g.setColour(juce::Colour(0xFF888888));
            g.setFont(juce::Font(11.0f, juce::Font::bold));
            g.drawText("MANUAL", 0, 0, (int) W, 36, juce::Justification::centred);

            // Model accent stripe
            g.setColour(getModelColour(CompressorModel::ModelManual).withAlpha(0.5f));
            g.fillRect(0.0f, 0.0f, W, 3.0f);

            g.setColour(CustomLookAndFeel::panelBorder());
            g.drawRoundedRectangle(bounds.reduced(0.5f), cornerR, 1.0f);
            break;
        }
    }
}

// ---------------------------------------------------------------------------
// resized — model-specific layouts
// ---------------------------------------------------------------------------

void StageComponent::resized()
{
    const int W = getWidth();
    const int H = getHeight();
    const int pad = 8;

    auto model = static_cast<CompressorModel>(modelBox.getSelectedItemIndex());

    // Header strip common to all models (inside the top 36px stripe)
    stageLabel .setBounds(pad, 4, 70, 16);
    enableButton.setBounds(W - pad - 30, 8, 30, 18);
    modelBox   .setBounds(pad, 22, W - pad * 2, 14);

    const int textBoxH = 14;
    const int labelH   = 12;
    const int gap      = 2;

    auto placeKnob = [&](juce::Slider& sl, juce::Label& lbl,
                         int cellX, int cellY, int cellW, int cellH,
                         int ks)
    {
        const int totalH  = ks + textBoxH + gap + labelH;
        const int centreX = cellX + cellW / 2;
        const int startY  = cellY + (cellH - totalH) / 2;
        sl .setBounds(centreX - ks / 2, startY, ks, ks + textBoxH);
        lbl.setBounds(centreX - ks / 2, startY + ks + textBoxH + gap, ks, labelH);
    };

    // Reset all to visible before per-model visibility overrides
    for (auto* s : { &thresholdSlider, &ratioSlider, &attackSlider,
                     &releaseSlider,   &satSlider,   &mixSlider })
        s->setVisible(true);
    for (auto* l : { &thresholdLabel, &ratioLabel, &attackLabel,
                     &releaseLabel,   &satLabel,   &mixLabel })
        l->setVisible(true);
    grMeter.setVisible(true);

    switch (model)
    {
        // ------------------------------------------------------------------
        case CompressorModel::Model1176:
        {
            // Real 1176 has: INPUT (=SAT), ATTACK, RELEASE, RATIO buttons
            // No threshold knob, no mix knob
            thresholdSlider.setVisible(false); thresholdLabel.setVisible(false);
            ratioSlider    .setVisible(false); ratioLabel    .setVisible(false);
            mixSlider      .setVisible(false); mixLabel      .setVisible(false);
            grMeter.setVisible(false);
            grMeter.setHorizontal(true);
            numRatioButtons = 5;

            // Large SAT (INPUT) knob — centred, ks=95
            const int satKS = 95;
            placeKnob(satSlider, satLabel,
                      W / 2 - satKS / 2, 38, satKS, satKS + textBoxH + gap + labelH, satKS);

            // Attack + Release side-by-side, ks=68, y=152
            const int smKS = 68;
            const int smCW = (W - pad * 2) / 2;
            const int smCH = smKS + textBoxH + gap + labelH;
            placeKnob(attackSlider,  attackLabel,  pad,        152, smCW, smCH, smKS);
            placeKnob(releaseSlider, releaseLabel, pad + smCW, 152, smCW, smCH, smKS);

            // 5 ratio buttons: y=254..296 (42px tall)
            {
                const int btnAreaX = pad;
                const int btnAreaY = 254;
                const int btnAreaW = W - pad * 2;
                const int btnH     = 40;
                const int btnW     = (btnAreaW - 4 * 2) / 5;
                for (int i = 0; i < 5; i++)
                    ratioButtonRects[i] = { btnAreaX + i * (btnW + 2), btnAreaY, btnW, btnH };
            }

            // GR meter: horizontal strip at bottom (100px)
            grMeter.setBounds(pad, H - 104, W - pad * 2, 96);
            break;
        }

        // ------------------------------------------------------------------
        case CompressorModel::ModelLA2A:
        {
            // Real LA-2A has: PEAK REDUCTION (threshold) + GAIN (our DRIVE/SAT)
            // No separate attack, release, or ratio knobs — optical element handles all
            attackSlider .setVisible(false); attackLabel .setVisible(false);
            releaseSlider.setVisible(false); releaseLabel.setVisible(false);
            ratioSlider  .setVisible(false); ratioLabel  .setVisible(false);
            mixSlider    .setVisible(false); mixLabel    .setVisible(false);
            grMeter.setVisible(false);   // VU needle drawn in paint()
            grMeter.setBounds(0, -100, 1, 1);
            numRatioButtons = 0;

            // Small DRIVE (SAT) knob at top — ks=58, centred
            const int drvKS = 58;
            placeKnob(satSlider, satLabel,
                      W / 2 - drvKS / 2, 42, drvKS, drvKS + textBoxH + gap + labelH, drvKS);

            // VU area: y=118..304 — drawn entirely in paint()

            // Large PEAK REDUCTION (threshold) knob at bottom — ks=90, centred
            const int pkKS = 90;
            placeKnob(thresholdSlider, thresholdLabel,
                      W / 2 - pkKS / 2, 312, pkKS, pkKS + textBoxH + gap + labelH, pkKS);
            break;
        }

        // ------------------------------------------------------------------
        case CompressorModel::ModelDestressor:
        {
            grMeter    .setVisible(true);
            grMeter    .setHorizontal(false);
            numRatioButtons = 6;

            ratioSlider.setVisible(false);
            ratioLabel .setVisible(false);

            // Left column: x=8..104 (width=96), 5 knobs stacked ks=60
            const int lcX  = 8;
            const int lcW  = 96;
            const int ks   = 60;
            const int kCH  = ks + textBoxH + gap + labelH;

            placeKnob(satSlider,       satLabel,       lcX, 44,  lcW, kCH, ks);
            placeKnob(thresholdSlider, thresholdLabel, lcX, 108, lcW, kCH, ks);
            placeKnob(attackSlider,    attackLabel,    lcX, 172, lcW, kCH, ks);
            placeKnob(releaseSlider,   releaseLabel,   lcX, 236, lcW, kCH, ks);
            placeKnob(mixSlider,       mixLabel,       lcX, 300, lcW, kCH, ks);

            // Right column: x=112..W-8, ratio buttons stacked
            {
                const int rcX  = 112;
                const int rcW  = W - rcX - 8;
                const int btnH = 40;
                const int btnGap = 2;
                for (int i = 0; i < 6; i++)
                {
                    ratioButtonRects[i] = {
                        rcX,
                        44 + i * (btnH + btnGap),
                        rcW,
                        btnH
                    };
                }
                // GR meter below buttons: y = 44 + 6*(btnH+btnGap) + 4
                int grY = 44 + 6 * (btnH + btnGap) + 4;
                grMeter.setBounds(rcX, grY, rcW, H - grY - 8);
            }
            break;
        }

        // ------------------------------------------------------------------
        case CompressorModel::ModelDBX160:
        {
            // Original DBX 160: just THRESHOLD and COMPRESSION (ratio) knobs
            // Attack/release/mix are automatic — not exposed on the hardware
            satSlider    .setVisible(false); satLabel    .setVisible(false);
            attackSlider .setVisible(false); attackLabel .setVisible(false);
            releaseSlider.setVisible(false); releaseLabel.setVisible(false);
            mixSlider    .setVisible(false); mixLabel    .setVisible(false);
            numRatioButtons = 0;
            grMeter.setVisible(true);
            grMeter.setHorizontal(false);

            // Two large knobs at top: THRESHOLD left, COMPRESSION (ratio) right
            const int lgKS = 88;
            const int lgCW = (W - pad * 2) / 2;
            const int lgCH = lgKS + textBoxH + gap + labelH;
            placeKnob(thresholdSlider, thresholdLabel, pad,        52, lgCW, lgCH, lgKS);
            placeKnob(ratioSlider,     ratioLabel,     pad + lgCW, 52, lgCW, lgCH, lgKS);

            // LED bargraph GR meter fills the lower 2/3 of the panel
            grMeter.setBounds(pad, lgCH + 58, W - pad * 2, H - lgCH - 66);
            break;
        }

        // ------------------------------------------------------------------
        case CompressorModel::ModelNeve:
        {
            // Neve 33609: THRESHOLD, RECOVERY (release), RATIO, ATK TIME switch, DRIVE (sat)
            // No mix knob on the original hardware
            mixSlider.setVisible(false); mixLabel.setVisible(false);
            numRatioButtons = 0;
            grMeter.setVisible(false);  // VU needle drawn in paint()
            grMeter.setHorizontal(false);

            // COMPRESSOR section: THR + RECOVERY side by side, ks=70, y=52
            const int ks1 = 70;
            const int cw1 = (W - pad * 2) / 2;
            const int ch1 = ks1 + textBoxH + gap + labelH;
            placeKnob(thresholdSlider, thresholdLabel, pad,        52, cw1, ch1, ks1);
            placeKnob(releaseSlider,   releaseLabel,   pad + cw1,  52, cw1, ch1, ks1);

            // RATIO + ATK TIME side by side, ks=62, y=152
            const int ks2 = 62;
            const int cw2 = (W - pad * 2) / 2;
            const int ch2 = ks2 + textBoxH + gap + labelH;
            placeKnob(ratioSlider,  ratioLabel,  pad,        152, cw2, ch2, ks2);
            placeKnob(attackSlider, attackLabel, pad + cw2,  152, cw2, ch2, ks2);

            // DRIVE (SAT) centred below, ks=54
            const int ks3 = 54;
            placeKnob(satSlider, satLabel,
                      W / 2 - ks3 / 2, 248, ks3, ks3 + textBoxH + gap + labelH, ks3);

            // GR meter at bottom, 86px tall
            grMeter.setBounds(pad, H - 90, W - pad * 2, 82);
            break;
        }

        // ------------------------------------------------------------------
        case CompressorModel::ModelSSL:
        {
            // SSL G-Bus: THRESH, RATIO (2/4/10), ATK (stepped), REL (stepped/auto), DRIVE
            // No makeup (global) — no mix knob on the original hardware
            mixSlider.setVisible(false); mixLabel.setVisible(false);
            numRatioButtons = 0;
            grMeter.setVisible(true);
            grMeter.setHorizontal(true);

            // 2×2 grid of medium knobs (ks=68)
            const int ks = 68;
            const int cw = (W - pad * 2) / 2;
            const int ch = ks + textBoxH + gap + labelH;
            placeKnob(thresholdSlider, thresholdLabel, pad,      44, cw, ch, ks);
            placeKnob(ratioSlider,     ratioLabel,     pad + cw, 44, cw, ch, ks);
            placeKnob(attackSlider,    attackLabel,    pad,      44 + ch, cw, ch, ks);
            placeKnob(releaseSlider,   releaseLabel,   pad + cw, 44 + ch, cw, ch, ks);

            // DRIVE (SAT) centred, ks=56
            const int ks2 = 56;
            placeKnob(satSlider, satLabel,
                      W / 2 - ks2 / 2, 44 + ch * 2, ks2, ks2 + textBoxH + gap + labelH, ks2);

            // GR meter horizontal at bottom
            grMeter.setBounds(pad, H - 90, W - pad * 2, 82);
            break;
        }

        // ------------------------------------------------------------------
        case CompressorModel::ModelManual:
        default:
        {
            numRatioButtons = 0;
            ratioSlider.setVisible(true);
            ratioLabel .setVisible(true);
            grMeter    .setVisible(true);
            grMeter    .setHorizontal(false);

            // GR meter right strip
            grMeter.setBounds(W - 20, 40, 18, H - 48);

            // 2×3 grid of knobs
            const int gridX = pad;
            const int gridY = 40;
            const int gridW = W - pad * 2 - 22;
            const int gridH = H - gridY - 8;
            const int cols  = 2;
            const int rows  = 3;
            const int cw    = gridW / cols;
            const int ch    = gridH / rows;
            const int ks    = juce::jmin(cw - 8, ch - textBoxH - gap - labelH - 4);

            juce::Slider* sliders[] = {
                &thresholdSlider, &ratioSlider,
                &attackSlider,    &releaseSlider,
                &satSlider,       &mixSlider
            };
            juce::Label* labels[] = {
                &thresholdLabel, &ratioLabel,
                &attackLabel,    &releaseLabel,
                &satLabel,       &mixLabel
            };

            for (int i = 0; i < 6; i++)
            {
                int col = i % cols;
                int row = i / cols;
                placeKnob(*sliders[i], *labels[i],
                          gridX + col * cw, gridY + row * ch, cw, ch, ks);
            }
            break;
        }
    }
}
