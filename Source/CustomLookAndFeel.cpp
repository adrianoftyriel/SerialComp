#include <JuceHeader.h>
#include "CustomLookAndFeel.h"

CustomLookAndFeel::CustomLookAndFeel()
{
    setColour(juce::ResizableWindow::backgroundColourId, background());
    setColour(juce::Slider::thumbColourId,              accent());
    setColour(juce::Slider::rotarySliderFillColourId,   accent());
    setColour(juce::Slider::rotarySliderOutlineColourId,knobTrack());
    setColour(juce::Label::textColourId,                textPrimary());
    setColour(juce::ComboBox::backgroundColourId,       panelBg());
    setColour(juce::ComboBox::textColourId,             textPrimary());
    setColour(juce::ComboBox::outlineColourId,          panelBorder());
    setColour(juce::ComboBox::arrowColourId,            accent());
    setColour(juce::PopupMenu::backgroundColourId,      juce::Colour(0xFF1E1E26));
    setColour(juce::PopupMenu::textColourId,            textPrimary());
    setColour(juce::PopupMenu::highlightedBackgroundColourId, accent().withAlpha(0.4f));
    setColour(juce::ToggleButton::textColourId,         textPrimary());
    setColour(juce::ToggleButton::tickColourId,         accent());
    setColour(juce::ToggleButton::tickDisabledColourId, textSecond());
}

void CustomLookAndFeel::drawRotarySlider(juce::Graphics& g,
                                          int x, int y, int width, int height,
                                          float sliderPos,
                                          float startAngle, float endAngle,
                                          juce::Slider& slider)
{
    // Guard 1: zero/negative dimensions (host calls before first resized())
    if (width <= 0 || height <= 0) return;

    // Guard 2: NaN/Inf in any angle argument — can arrive before SliderAttachment
    // has written the initial value, making sliderPos = NaN propagate to angle,
    // then to sin/cos, then to fillEllipse() which crashes the JUCE renderer.
    if (!std::isfinite(sliderPos) || !std::isfinite(startAngle) || !std::isfinite(endAngle))
        return;

    const float radius   = static_cast<float>(juce::jmin(width, height)) * 0.5f - 4.0f;
    if (radius < 4.0f) return;  // ensure knobRadius > stroke width

    // Clamp sliderPos so subsequent sin/cos are always well-defined
    sliderPos = juce::jlimit(0.0f, 1.0f, sliderPos);

    const float centreX  = (float) x + (float) width  * 0.5f;
    const float centreY  = (float) y + (float) height * 0.5f;
    const float angle    = startAngle + sliderPos * (endAngle - startAngle);

    const float cx           = centreX;
    const float cy           = centreY;
    const juce::Colour fillColour = slider.findColour(juce::Slider::rotarySliderFillColourId);

    // --- Track arc (common to all styles) ---
    juce::Path trackPath;
    trackPath.addArc(cx - radius, cy - radius,
                     radius * 2.0f, radius * 2.0f,
                     startAngle, endAngle, true);
    juce::PathStrokeType stroke(3.5f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded);
    g.setColour(slider.findColour(juce::Slider::rotarySliderOutlineColourId));
    g.strokePath(trackPath, stroke);

    // --- Value arc (common to all styles) ---
    if (sliderPos > 0.0f)
    {
        juce::Path valuePath;
        valuePath.addArc(cx - radius, cy - radius,
                         radius * 2.0f, radius * 2.0f,
                         startAngle, angle, true);
        g.setColour(fillColour);
        g.strokePath(valuePath, stroke);
    }

    // --- Dispatch to per-model knob body style ---
    const int knobStyle = (int) slider.getProperties().getWithDefault("knobStyle", 0);

    switch (knobStyle)
    {
        // ------------------------------------------------------------------
        case 1: // vintage-bakelite (1176, Distressor)
        {
            // Polished silver outer ring
            juce::ColourGradient ringGrad(
                juce::Colour(0xFFCCCCCC), cx - radius*0.35f, cy - radius*0.35f,
                juce::Colour(0xFF606060), cx + radius*0.35f, cy + radius*0.35f, false);
            g.setGradientFill(ringGrad);
            g.fillEllipse(cx-radius, cy-radius, radius*2.0f, radius*2.0f);

            // Black knob body
            const float br = radius * 0.78f;
            juce::ColourGradient bodyGrad(
                juce::Colour(0xFF282828), cx - br*0.3f, cy - br*0.4f,
                juce::Colour(0xFF080808), cx + br*0.3f, cy + br*0.3f, false);
            g.setGradientFill(bodyGrad);
            g.fillEllipse(cx-br, cy-br, br*2.0f, br*2.0f);

            // Top-left gloss
            g.setColour(juce::Colours::white.withAlpha(0.07f));
            g.fillEllipse(cx - br*0.55f, cy - br*0.65f, br*0.7f, br*0.5f);

            // Triangular white pointer from body-center outward
            const float pLen = br * 0.80f;
            const float tipX = cx + pLen * std::sin(angle);
            const float tipY = cy - pLen * std::cos(angle);
            const float perpX = std::cos(angle), perpY = std::sin(angle);
            const float hw = br * 0.12f;
            juce::Path ptr;
            ptr.startNewSubPath(tipX, tipY);
            ptr.lineTo(cx + hw * perpX, cy + hw * perpY);
            ptr.lineTo(cx - hw * perpX, cy - hw * perpY);
            ptr.closeSubPath();
            g.setColour(juce::Colours::white.withAlpha(0.92f));
            g.fillPath(ptr);
            break;
        }

        // ------------------------------------------------------------------
        case 2: // la2a-cream (LA-2A)
        {
            // Warm dark-brown outer ring
            juce::ColourGradient ringGrad(
                juce::Colour(0xFF504030), cx - radius*0.3f, cy - radius*0.4f,
                juce::Colour(0xFF201810), cx + radius*0.3f, cy + radius*0.3f, false);
            g.setGradientFill(ringGrad);
            g.fillEllipse(cx-radius, cy-radius, radius*2.0f, radius*2.0f);

            // Cream/ivory body
            const float br = radius * 0.76f;
            juce::ColourGradient bodyGrad(
                juce::Colour(0xFFE8DEC0), cx - br*0.3f, cy - br*0.4f,
                juce::Colour(0xFFBEB48A), cx + br*0.3f, cy + br*0.3f, false);
            g.setGradientFill(bodyGrad);
            g.fillEllipse(cx-br, cy-br, br*2.0f, br*2.0f);

            // Warm gloss
            g.setColour(juce::Colour(0xFFFFFFD0).withAlpha(0.18f));
            g.fillEllipse(cx - br*0.5f, cy - br*0.62f, br*0.65f, br*0.44f);

            // Dark-brown triangular pointer
            const float pLen = br * 0.80f;
            const float tipX = cx + pLen * std::sin(angle);
            const float tipY = cy - pLen * std::cos(angle);
            const float perpX = std::cos(angle), perpY = std::sin(angle);
            const float hw = br * 0.11f;
            juce::Path ptr;
            ptr.startNewSubPath(tipX, tipY);
            ptr.lineTo(cx + hw * perpX, cy + hw * perpY);
            ptr.lineTo(cx - hw * perpX, cy - hw * perpY);
            ptr.closeSubPath();
            g.setColour(juce::Colour(0xFF2A1E0A).withAlpha(0.88f));
            g.fillPath(ptr);
            break;
        }

        // ------------------------------------------------------------------
        case 3: // neve-console (Neve, SSL)
        {
            // Thin chrome ring
            g.setColour(juce::Colour(0xFF989898));
            g.drawEllipse(cx-radius, cy-radius, radius*2.0f, radius*2.0f, 1.8f);

            // Very dark matte body
            const float br = radius * 0.84f;
            juce::ColourGradient bodyGrad(
                juce::Colour(0xFF1E1E1E), cx, cy - br,
                juce::Colour(0xFF0A0A0A), cx, cy + br, false);
            g.setGradientFill(bodyGrad);
            g.fillEllipse(cx-br, cy-br, br*2.0f, br*2.0f);

            // Thin white line pointer
            const float p0x = cx + br * 0.28f * std::sin(angle);
            const float p0y = cy - br * 0.28f * std::cos(angle);
            const float p1x = cx + br * 0.88f * std::sin(angle);
            const float p1y = cy - br * 0.88f * std::cos(angle);
            g.setColour(juce::Colours::white.withAlpha(0.88f));
            g.drawLine(p0x, p0y, p1x, p1y, 1.8f);
            break;
        }

        // ------------------------------------------------------------------
        case 4: // dbx-rubber (DBX 160)
        {
            // Ridged rubber body
            const float br = radius * 0.86f;
            // Draw radial ridges first (12 faint lines from center to edge)
            g.setColour(juce::Colour(0xFF303030));
            for (int i = 0; i < 12; i++)
            {
                float a = (float)i * juce::MathConstants<float>::twoPi / 12.0f;
                g.drawLine(cx + br*0.45f*std::sin(a), cy - br*0.45f*std::cos(a),
                           cx + br*0.92f*std::sin(a), cy - br*0.92f*std::cos(a), 1.2f);
            }
            juce::ColourGradient bodyGrad(
                juce::Colour(0xFF252525), cx, cy - br,
                juce::Colour(0xFF0E0E0E), cx, cy + br, false);
            g.setGradientFill(bodyGrad);
            g.fillEllipse(cx-br, cy-br, br*2.0f, br*2.0f);
            // Silver top cap (small bright disc)
            const float capR = br * 0.28f;
            juce::ColourGradient capGrad(
                juce::Colour(0xFFDDDDDD), cx - capR*0.4f, cy - capR*0.4f,
                juce::Colour(0xFF888888), cx + capR*0.4f, cy + capR*0.4f, false);
            g.setGradientFill(capGrad);
            g.fillEllipse(cx-capR, cy-capR, capR*2.0f, capR*2.0f);
            // White dot pointer
            const float dotX = cx + (br * 0.70f) * std::sin(angle);
            const float dotY = cy - (br * 0.70f) * std::cos(angle);
            g.setColour(juce::Colours::white.withAlpha(0.90f));
            g.fillEllipse(dotX-2.5f, dotY-2.5f, 5.0f, 5.0f);
            break;
        }

        // ------------------------------------------------------------------
        case 0: // default dark
        default:
        {
            // --- Knob body ---
            const float knobRadius = radius * 0.72f;
            juce::ColourGradient bodyGrad(
                juce::Colour(0xFF3A3A44), cx, cy - knobRadius,
                juce::Colour(0xFF1C1C22), cx, cy + knobRadius,
                false);
            g.setGradientFill(bodyGrad);
            g.fillEllipse(cx - knobRadius, cy - knobRadius,
                          knobRadius * 2.0f, knobRadius * 2.0f);

            // Subtle highlight ring
            g.setColour(juce::Colour(0xFF4A4A55));
            g.drawEllipse(cx - knobRadius, cy - knobRadius,
                          knobRadius * 2.0f, knobRadius * 2.0f, 1.0f);

            // --- Pointer dot ---
            const float pointerLen    = knobRadius * 0.65f;
            const float pointerRadius = 3.0f;
            const float px = cx + pointerLen * std::sin(angle);
            const float py = cy - pointerLen * std::cos(angle);
            // Guard 3: sin/cos of a clamped finite angle should always be finite, but
            // belt-and-suspenders to prevent any degenerate path from crashing the renderer.
            if (!std::isfinite(px) || !std::isfinite(py)) return;
            g.setColour(fillColour);
            g.fillEllipse(px - pointerRadius, py - pointerRadius,
                          pointerRadius * 2.0f, pointerRadius * 2.0f);
            break;
        }
    }
}

void CustomLookAndFeel::drawToggleButton(juce::Graphics& g,
                                          juce::ToggleButton& button,
                                          bool isMouseOverButton,
                                          bool /*isButtonDown*/)
{
    const bool on = button.getToggleState();
    auto bounds   = button.getLocalBounds().toFloat();

    // Pill-shaped button
    const float cornerSize = bounds.getHeight() * 0.5f;
    juce::Colour bg = on
        ? accent().withAlpha(isMouseOverButton ? 1.0f : 0.85f)
        : juce::Colour(0xFF2E2E38).withAlpha(isMouseOverButton ? 0.9f : 1.0f);

    g.setColour(bg);
    g.fillRoundedRectangle(bounds, cornerSize);

    g.setColour(on ? juce::Colour(0xFF111116) : textSecond());
    g.drawRoundedRectangle(bounds.reduced(0.5f), cornerSize, 1.0f);

    g.setColour(on ? juce::Colour(0xFF111116) : textSecond());
    g.setFont(juce::Font(11.0f, juce::Font::bold));
    g.drawText(button.getButtonText(), bounds, juce::Justification::centred, false);
}

void CustomLookAndFeel::drawComboBox(juce::Graphics& g, int width, int height,
                                      bool /*isButtonDown*/,
                                      int /*bX*/, int /*bY*/, int /*bW*/, int /*bH*/,
                                      juce::ComboBox& /*box*/)
{
    auto bounds = juce::Rectangle<float>(0, 0, (float) width, (float) height);
    g.setColour(panelBg());
    g.fillRoundedRectangle(bounds, 3.0f);
    g.setColour(panelBorder());
    g.drawRoundedRectangle(bounds.reduced(0.5f), 3.0f, 1.0f);

    // Arrow
    const float arrowX = width - 14.0f;
    const float arrowY = height * 0.5f;
    juce::Path arrow;
    arrow.addTriangle(arrowX - 4.0f, arrowY - 2.0f,
                      arrowX + 4.0f, arrowY - 2.0f,
                      arrowX,        arrowY + 3.0f);
    g.setColour(accent());
    g.fillPath(arrow);
}

void CustomLookAndFeel::positionComboBoxText(juce::ComboBox& box, juce::Label& label)
{
    label.setBounds(4, 1, box.getWidth() - 20, box.getHeight() - 2);
    label.setFont(getLabelFont(label));
}

juce::Font CustomLookAndFeel::getLabelFont(juce::Label&)
{
    return juce::Font(11.5f);
}
