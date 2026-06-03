#pragma once
#include <JuceHeader.h>

// ---------------------------------------------------------------------------
// Lock-free circular buffer for audio capture.
// Push called on the audio thread; read on the UI thread.
// ---------------------------------------------------------------------------
class WaveformBuffer
{
public:
    // 2^20 = 1 048 576 samples ≈ 23.8 s at 44 100 Hz / 21.8 s at 48 000 Hz.
    static constexpr int SIZE = 1048576;

    void push(float sample) noexcept
    {
        buf[writePos.load(std::memory_order_relaxed) & (SIZE - 1)] = sample;
        writePos.fetch_add(1, std::memory_order_release);
    }

    // Returns the peak absolute value in each of out.size() evenly-spaced
    // bins covering the most-recent windowSamples (oldest → newest).
    void readPeakDecimated(std::vector<float>& out, int windowSamples) const noexcept
    {
        const int n = static_cast<int>(out.size());
        if (n == 0) return;

        const int end    = static_cast<int>(writePos.load(std::memory_order_acquire));
        const int avail  = juce::jmin(end, SIZE);
        const int window = juce::jmin(windowSamples, avail);

        if (window == 0)
        {
            std::fill(out.begin(), out.end(), 0.0f);
            return;
        }

        const int   start = end - window;
        const float binF  = static_cast<float>(window) / static_cast<float>(n);

        for (int i = 0; i < n; i++)
        {
            const int binStart = static_cast<int>(static_cast<float>(i)     * binF);
            int       binEnd   = static_cast<int>(static_cast<float>(i + 1) * binF);
            if (binEnd <= binStart) binEnd = binStart + 1;

            float peak = 0.0f;
            for (int j = binStart; j < binEnd && j < window; j++)
                peak = juce::jmax(peak, std::abs(buf[(start + j) & (SIZE - 1)]));

            out[static_cast<size_t>(i)] = peak;
        }
    }

private:
    float buf[SIZE] = {};
    std::atomic<int> writePos { 0 };
};

// ---------------------------------------------------------------------------
// Volume-history display.
// Shows peak dBFS over a 20-second rolling window.
// dBFS scale: 0 dB at top, -40 dB at bottom, grid every 6 dB with labels.
// Input shown as a gray filled area; each active stage as a coloured line.
// ---------------------------------------------------------------------------
class VolumeHistoryDisplay : public juce::Component
{
public:
    static constexpr int MAX_STAGES  = 5;
    static constexpr int DISPLAY_PTS = 512;

    VolumeHistoryDisplay()
    {
        samples.resize(static_cast<size_t>(DISPLAY_PTS));
    }

    void setSources(WaveformBuffer* input,
                    WaveformBuffer* stages[MAX_STAGES],
                    int numStages)
    {
        inputSrc    = input;
        for (int i = 0; i < MAX_STAGES; i++)
            stageSrc[i] = stages[i];
        numActiveSt = numStages;
    }

    void setNumActiveStages(int n) { numActiveSt = n; }
    void setSampleRate(double sr)  { sampleRate  = sr; }

    void paint(juce::Graphics& g) override
    {
        auto bounds = getLocalBounds().toFloat();

        // Background
        g.setColour(juce::Colour(0xFF0C0C12));
        g.fillRoundedRectangle(bounds, 4.0f);

        // Left margin for scale labels
        static constexpr float labelW = 30.0f;
        auto plotArea  = bounds.reduced(2.0f, 4.0f);
        auto labelArea = plotArea.removeFromLeft(labelW);

        // dBFS scale
        static constexpr float minDB =  -40.0f;
        static constexpr float maxDB =    0.0f;
        static constexpr float range  = maxDB - minDB;

        auto dbToY = [&](float db) -> float {
            return plotArea.getY() + ((maxDB - db) / range) * plotArea.getHeight();
        };

        // Grid and labels
        g.setFont(juce::Font(8.0f));
        constexpr float ticks[] = { 0.0f, -6.0f, -12.0f, -18.0f, -24.0f, -30.0f, -36.0f };
        for (float db : ticks)
        {
            float y = dbToY(db);
            g.setColour(juce::Colour(0xFF55556A));
            juce::String lbl = (juce::approximatelyEqual(db, 0.0f))
                                ? juce::String("0 dB")
                                : juce::String(static_cast<int>(db));
            g.drawText(lbl,
                       static_cast<int>(labelArea.getX()),
                       static_cast<int>(y) - 5,
                       static_cast<int>(labelArea.getWidth()) - 2, 10,
                       juce::Justification::centredRight);
            bool isZero = juce::approximatelyEqual(db, 0.0f);
            g.setColour(juce::Colour(isZero ? 0xFF2E2E3C : 0xFF1A1A26));
            g.drawHorizontalLine(static_cast<int>(y), plotArea.getX(), plotArea.getRight());
        }

        if (inputSrc == nullptr) return;

        const int windowSamples = static_cast<int>(sampleRate * 20.0);

        // Input history — gray filled area (back layer)
        inputSrc->readPeakDecimated(samples, windowSamples);
        g.setColour(juce::Colour(0xFF3A3A50).withAlpha(0.65f));
        drawHistory(g, plotArea, minDB, maxDB, true);

        // Stage histories — coloured lines (draw lowest active first)
        for (int i = numActiveSt - 1; i >= 0; i--)
        {
            if (stageSrc[i] == nullptr) continue;
            stageSrc[i]->readPeakDecimated(samples, windowSamples);
            g.setColour(stageColours[i].withAlpha(0.85f));
            drawHistory(g, plotArea, minDB, maxDB, false);
        }

        // Border
        g.setColour(juce::Colour(0xFF282835));
        g.drawRoundedRectangle(bounds.reduced(0.5f), 4.0f, 1.0f);
    }

private:
    WaveformBuffer*  inputSrc             = nullptr;
    WaveformBuffer*  stageSrc[MAX_STAGES] = {};
    int              numActiveSt          = 0;
    double           sampleRate           = 44100.0;
    std::vector<float> samples;

    juce::Colour stageColours[MAX_STAGES] = {
        juce::Colour(0xFF4488CC),
        juce::Colour(0xFF66CC44),
        juce::Colour(0xFFCC8833),
        juce::Colour(0xFFCC3333),
        juce::Colour(0xFF9944CC),
    };

    void drawHistory(juce::Graphics& g,
                     juce::Rectangle<float> b,
                     float minDB, float maxDB,
                     bool filled) const
    {
        if (b.getWidth() <= 0.0f || b.getHeight() <= 0.0f) return;

        const int   n     = DISPLAY_PTS;
        const float range = maxDB - minDB;

        juce::Path path;
        for (int i = 0; i < n; i++)
        {
            float x   = b.getX() + static_cast<float>(i) / static_cast<float>(n - 1) * b.getWidth();
            float lin = samples[static_cast<size_t>(i)];
            float db  = juce::Decibels::gainToDecibels(lin, minDB - 1.0f);
            db        = juce::jlimit(minDB, maxDB, db);
            float y   = b.getY() + ((maxDB - db) / range) * b.getHeight();

            if (i == 0) path.startNewSubPath(x, y);
            else        path.lineTo(x, y);
        }

        if (filled)
        {
            path.lineTo(b.getRight(), b.getBottom());
            path.lineTo(b.getX(),     b.getBottom());
            path.closeSubPath();
            g.fillPath(path);
        }
        else
        {
            g.strokePath(path, juce::PathStrokeType(1.5f));
        }
    }
};
