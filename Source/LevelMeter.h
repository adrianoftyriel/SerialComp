#pragma once
#include <JuceHeader.h>

// Vertical dBFS level meter: -60 to +6 dBFS
// Green/yellow/red gradient bar, white peak-hold line, dB tick labels.
// Call advancePeakHold() once per timer tick to animate the peak decay.
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

    void advancePeakHold()
    {
        if (peakHoldTicks > 0)
            --peakHoldTicks;
        else
            peakDB = juce::jmax(minDB, peakDB - decayPerTick);
    }

    void paint(juce::Graphics& g) override
    {
        auto b = getLocalBounds().toFloat();

        // Background
        g.setColour(juce::Colour(0xFF0D0D10));
        g.fillRoundedRectangle(b, 3.0f);

        const float totalRange = maxDB - minDB;  // 66 dB

        auto dbToFrac = [&](float db) -> float {
            return 1.0f - (db - minDB) / totalRange;  // 0 = top (+6), 1 = bottom (-60)
        };

        const float h = b.getHeight();

        // Filled bar
        {
            float barTop  = h * dbToFrac(currentDB);
            float barH    = h - barTop;
            if (barH > 0.0f)
            {
                juce::ColourGradient grad(
                    juce::Colour(0xFFCC2222), b.getX(), b.getY(),
                    juce::Colour(0xFF33BB33), b.getX(), b.getBottom(),
                    false);
                grad.addColour(double(9.0f  / totalRange), juce::Colour(0xFFCCCC22));  // −3 dB
                grad.addColour(double(18.0f / totalRange), juce::Colour(0xFF55AA33));  // −12 dB

                g.setGradientFill(grad);
                g.fillRect(juce::Rectangle<float>(b.getX(), b.getY() + barTop,
                                                  b.getWidth(), barH));
            }
        }

        // Peak-hold line
        {
            float peakY = b.getY() + h * dbToFrac(peakDB);
            g.setColour(juce::Colours::white.withAlpha(0.85f));
            g.drawHorizontalLine(static_cast<int>(peakY), b.getX(), b.getRight());
        }

        // Border
        g.setColour(juce::Colour(0xFF333340));
        g.drawRoundedRectangle(getLocalBounds().toFloat().reduced(0.5f), 3.0f, 1.0f);

        // Tick marks and labels at key dBFS values
        g.setFont(juce::Font(7.0f));
        constexpr float ticks[] = { 0.0f, -6.0f, -12.0f, -18.0f, -24.0f, -36.0f, -48.0f };
        for (float tick : ticks)
        {
            int y = static_cast<int>(b.getY() + h * dbToFrac(tick));
            g.setColour(juce::Colour(0x55FFFFFF));
            g.drawHorizontalLine(y, b.getX(), b.getRight());
            g.setColour(juce::Colour(0xCCFFFFFF));
            juce::String lbl = (juce::approximatelyEqual(tick, 0.0f)) ? juce::String("0") : juce::String(static_cast<int>(tick));
            g.drawText(lbl,
                       static_cast<int>(b.getX()) + 1,
                       y - 7,
                       static_cast<int>(b.getWidth()) - 2, 7,
                       juce::Justification::centredRight);
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
