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
            auto barR  = juce::Rectangle<float>(b.getX(), b.getY(), b.getWidth(), barH);

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

        // Tick marks with dB labels at 3, 6, 12 dB GR
        g.setFont(juce::Font(7.0f));
        constexpr float ticks[] = { 3.0f, 6.0f, 12.0f };
        for (float tick : ticks)
        {
            int y = static_cast<int>((tick / maxDB) * getHeight());
            g.setColour(juce::Colour(0x66FFFFFF));
            g.drawHorizontalLine(y, 0.0f, static_cast<float>(getWidth()));
            g.setColour(juce::Colour(0xAAFFFFFF));
            g.drawText(juce::String(static_cast<int>(tick)),
                       1, y - 7, getWidth() - 2, 7,
                       juce::Justification::centredRight);
        }

        // Numeric readout (active level)
        if (gainReductionDB > 0.5f)
        {
            g.setColour(juce::Colours::white.withAlpha(0.85f));
            g.setFont(juce::Font(8.0f, juce::Font::bold));
            g.drawText(juce::String(gainReductionDB, 1),
                       getLocalBounds(), juce::Justification::centredBottom);
        }
    }

private:
    float gainReductionDB = 0.0f;
    static constexpr float maxDB = 20.0f;
};
