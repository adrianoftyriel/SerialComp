#pragma once
#include <JuceHeader.h>

// Bar meter supporting both vertical and horizontal orientations.
// Vertical (default): top = 0 dB GR, bottom = maximum reduction.
// Horizontal: left = 0 dB GR, right = maximum reduction.
class GainReductionMeter : public juce::Component
{
public:
    GainReductionMeter() = default;

    void setGainReduction(float dB)
    {
        gainReductionDB = juce::jlimit(0.0f, maxDB, dB);
        repaint();
    }

    void setHorizontal(bool h)
    {
        horizontal = h;
        repaint();
    }

    void paint(juce::Graphics& g) override
    {
        if (horizontal)
            paintHorizontal(g);
        else
            paintVertical(g);
    }

private:
    float gainReductionDB = 0.0f;
    bool  horizontal      = false;
    static constexpr float maxDB = 20.0f;

    void paintVertical(juce::Graphics& g)
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

    void paintHorizontal(juce::Graphics& g)
    {
        auto b = getLocalBounds().toFloat();

        // Background
        g.setColour(juce::Colour(0xFF0D0D10));
        g.fillRoundedRectangle(b, 3.0f);

        const float fraction = gainReductionDB / maxDB;

        if (fraction > 0.0f)
        {
            float barW = b.getWidth() * fraction;
            auto barR  = juce::Rectangle<float>(b.getX(), b.getY(), barW, b.getHeight());

            // Horizontal gradient: red (left/high GR) -> yellow -> green (right/low GR)
            juce::ColourGradient grad(
                juce::Colour(0xFFBB2222), barR.getTopLeft(),
                juce::Colour(0xFF44BB44), barR.getTopRight(),
                false);
            grad.addColour(0.5, juce::Colour(0xFFBBBB22));

            g.setGradientFill(grad);
            g.fillRoundedRectangle(barR, 2.0f);
        }

        // Border
        g.setColour(juce::Colour(0xFF333340));
        g.drawRoundedRectangle(getLocalBounds().toFloat().reduced(0.5f), 3.0f, 1.0f);

        // Vertical tick marks at 3, 6, 12 dB GR positions with small labels above
        g.setFont(juce::Font(7.0f));
        constexpr float ticks[] = { 3.0f, 6.0f, 12.0f };
        const float w = static_cast<float>(getWidth());
        const float h = static_cast<float>(getHeight());
        for (float tick : ticks)
        {
            float x = (tick / maxDB) * w;
            g.setColour(juce::Colour(0x66FFFFFF));
            g.drawVerticalLine(static_cast<int>(x), 0.0f, h);
            // Label above tick (inside bar area, near top)
            g.setColour(juce::Colour(0xAAFFFFFF));
            g.drawText(juce::String(static_cast<int>(tick)),
                       static_cast<int>(x) - 8, 1, 16, 7,
                       juce::Justification::centred);
        }

        // Numeric readout at right edge
        if (gainReductionDB > 0.5f)
        {
            g.setColour(juce::Colours::white.withAlpha(0.85f));
            g.setFont(juce::Font(8.0f, juce::Font::bold));
            g.drawText(juce::String(gainReductionDB, 1),
                       getLocalBounds().removeFromRight(28), juce::Justification::centredRight);
        }
    }
};
