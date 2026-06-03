#pragma once
#include <JuceHeader.h>

// Vertical bar meter: top = 0 dB GR, bottom = maximum reduction
class GainReductionMeter : public juce::Component
{
public:
    GainReductionMeter() = default;

    void setGainReduction(float dB)
    {
        gainReductionDB = juce::jlimit(0.0f, maxDB, dB);
        repaint();
    }

    void paint(juce::Graphics& g) override
    {
        auto b = getLocalBounds().toFloat();

        // Background
        g.setColour(juce::Colour(0xFF0D0D10));
        g.fillRoundedRectangle(b, 3.0f);

        const float fraction = gainReductionDB / maxDB;

        if (fraction > 0.0f)
        {
            float barH = b.getHeight() * fraction;
            auto barR  = b.removeFromTop(barH);

            // Gradient: green → yellow → red
            juce::ColourGradient grad(
                juce::Colour(0xFFBB2222), barR.getTopLeft(),
                juce::Colour(0xFF44BB44), barR.getBottomLeft(),
                false);
            grad.addColour(0.5, juce::Colour(0xFFBBBB22));

            g.setGradientFill(grad);
            g.fillRoundedRectangle(barR, 2.0f);
        }

        // Border
        g.setColour(juce::Colour(0xFF333340));
        g.drawRoundedRectangle(getLocalBounds().toFloat().reduced(0.5f), 3.0f, 1.0f);

        // Tick marks at -3, -6, -12 dB GR
        g.setColour(juce::Colour(0x55FFFFFF));
        g.setFont(8.0f);
        for (float tick : { 3.0f, 6.0f, 12.0f })
        {
            float y = (tick / maxDB) * getHeight();
            g.drawHorizontalLine(static_cast<int>(y), 0.0f, static_cast<float>(getWidth()));
        }

        // Numeric readout
        if (gainReductionDB > 0.3f)
        {
            g.setColour(juce::Colours::white.withAlpha(0.85f));
            g.setFont(juce::Font(8.5f, juce::Font::bold));
            g.drawText(juce::String(gainReductionDB, 1),
                       getLocalBounds(), juce::Justification::centredBottom);
        }
    }

private:
    float gainReductionDB = 0.0f;
    static constexpr float maxDB = 20.0f;
};
