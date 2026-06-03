#pragma once
#include <JuceHeader.h>

// Vertical dBFS level meter: -60 to +6 dBFS
// Green up to -12, yellow to -3, red above -3.
// Shows a white peak-hold line that decays after ~2s (driven by timerCallback).
class LevelMeter : public juce::Component
{
public:
    LevelMeter() = default;

    void setLevel(float dBFS)
    {
        currentDB = juce::jlimit(minDB, maxDB, dBFS);

        if (currentDB >= peakDB)
        {
            peakDB        = currentDB;
            peakHoldTicks = holdTicks;
        }

        repaint();
    }

    // Call once per timer tick so the peak hold decays correctly.
    void advancePeakHold()
    {
        if (peakHoldTicks > 0)
        {
            --peakHoldTicks;
        }
        else
        {
            peakDB -= decayPerTick;
            if (peakDB < minDB) peakDB = minDB;
        }
    }

    void paint(juce::Graphics& g) override
    {
        auto b = getLocalBounds().toFloat();

        // Background
        g.setColour(juce::Colour(0xFF0D0D10));
        g.fillRoundedRectangle(b, 3.0f);

        const float totalRange = maxDB - minDB;                // 66 dB
        const float dbToFrac   = [&](float db) -> float {
            return 1.0f - (db - minDB) / totalRange;           // 0 = top (+6), 1 = bottom (-60)
        }(currentDB);

        const float h = b.getHeight();

        // Filled bar
        {
            float barTop = h * dbToFrac;

            // Split into colour zones (measured from bottom=minDB, top=maxDB):
            //   red:    maxDB  → -3 dBFS   (top 9/66 of height)
            //   yellow: -3     → -12 dBFS
            //   green:  -12    → minDB
            auto barRect = juce::Rectangle<float>(b.getX(), b.getY() + barTop,
                                                   b.getWidth(), h - barTop);

            if (barRect.getHeight() > 0.0f)
            {
                juce::ColourGradient grad(
                    juce::Colour(0xFFCC2222), b.getX(), b.getY(),
                    juce::Colour(0xFF33BB33), b.getX(), b.getBottom(),
                    false);
                grad.addColour(double(9.0f / totalRange),  juce::Colour(0xFFCCCC22)); // -3 dB
                grad.addColour(double(18.0f / totalRange), juce::Colour(0xFF55AA33)); // -12 dB

                g.setGradientFill(grad);
                g.fillRect(barRect);
            }
        }

        // Peak-hold line
        {
            float peakFrac = 1.0f - (peakDB - minDB) / totalRange;
            float peakY    = b.getY() + h * peakFrac;
            g.setColour(juce::Colours::white.withAlpha(0.85f));
            g.drawHorizontalLine(static_cast<int>(peakY), b.getX(), b.getRight());
        }

        // Tick marks at 0, -6, -12, -18, -24, -36, -48 dBFS
        g.setColour(juce::Colour(0x55FFFFFF));
        g.setFont(7.0f);
        for (float tick : { 0.0f, -6.0f, -12.0f, -18.0f, -24.0f, -36.0f, -48.0f })
        {
            float frac = 1.0f - (tick - minDB) / totalRange;
            float y    = b.getY() + h * frac;
            g.drawHorizontalLine(static_cast<int>(y), b.getX(), b.getRight());
        }

        // Border
        g.setColour(juce::Colour(0xFF333340));
        g.drawRoundedRectangle(getLocalBounds().toFloat().reduced(0.5f), 3.0f, 1.0f);

        // dB value readout at top
        if (currentDB > minDB + 3.0f)
        {
            g.setColour(juce::Colours::white.withAlpha(0.75f));
            g.setFont(juce::Font(7.5f, juce::Font::bold));
            g.drawText(juce::String(currentDB, 0),
                       getLocalBounds().reduced(1, 1),
                       juce::Justification::centredBottom);
        }
    }

private:
    static constexpr float minDB        = -60.0f;
    static constexpr float maxDB        =   6.0f;
    static constexpr int   holdTicks    = 60;     // ~2s at 30 Hz
    static constexpr float decayPerTick = 0.5f;   // dB per tick after hold

    float currentDB     = minDB;
    float peakDB        = minDB;
    int   peakHoldTicks = 0;
};
