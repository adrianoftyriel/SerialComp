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
                                          juce::Slider& /*slider*/)
{
    const float radius   = (float) juce::jmin(width / 2, height / 2) - 4.0f;

    // Reaper (and some other hosts) call drawRotarySlider before the first
    // resized(), passing width=0 / height=0.  A negative radius produces a
    // degenerate path inside addArc / drawEllipse that crashes the renderer.
    if (radius <= 0.0f) return;

    const float centreX  = (float) x + (float) width  * 0.5f;
    const float centreY  = (float) y + (float) height * 0.5f;
    const float angle    = startAngle + sliderPos * (endAngle - startAngle);

    // --- Track arc ---
    juce::Path trackPath;
    trackPath.addArc(centreX - radius, centreY - radius,
                     radius * 2.0f, radius * 2.0f,
                     startAngle, endAngle, true);
    juce::PathStrokeType stroke(3.5f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded);
    g.setColour(knobTrack());
    g.strokePath(trackPath, stroke);

    // --- Value arc ---
    if (sliderPos > 0.0f)
    {
        juce::Path valuePath;
        valuePath.addArc(centreX - radius, centreY - radius,
                         radius * 2.0f, radius * 2.0f,
                         startAngle, angle, true);
        g.setColour(accent());
        g.strokePath(valuePath, stroke);
    }

    // --- Knob body ---
    const float knobRadius = radius * 0.72f;
    juce::ColourGradient bodyGrad(
        juce::Colour(0xFF3A3A44), centreX, centreY - knobRadius,
        juce::Colour(0xFF1C1C22), centreX, centreY + knobRadius,
        false);
    g.setGradientFill(bodyGrad);
    g.fillEllipse(centreX - knobRadius, centreY - knobRadius,
                  knobRadius * 2.0f, knobRadius * 2.0f);

    // Subtle highlight ring
    g.setColour(juce::Colour(0xFF4A4A55));
    g.drawEllipse(centreX - knobRadius, centreY - knobRadius,
                  knobRadius * 2.0f, knobRadius * 2.0f, 1.0f);

    // --- Pointer dot ---
    const float pointerLen    = knobRadius * 0.65f;
    const float pointerRadius = 3.0f;
    float px = centreX + pointerLen * std::sin(angle);
    float py = centreY - pointerLen * std::cos(angle);
    g.setColour(accent());
    g.fillEllipse(px - pointerRadius, py - pointerRadius,
                  pointerRadius * 2.0f, pointerRadius * 2.0f);
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
