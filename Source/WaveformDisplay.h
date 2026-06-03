#pragma once
#include <JuceHeader.h>

// ---------------------------------------------------------------------------
// Lock-free circular buffer for waveform capture.
// Push called on the audio thread; read on the UI thread.
// ---------------------------------------------------------------------------
class WaveformBuffer
{
public:
    // 2^19 = 524 288 samples ≈ 11.9 s at 44 100 Hz / 10.9 s at 48 000 Hz.
    static constexpr int SIZE = 524288;

    void push(float sample) noexcept
    {
        buf[writePos.load(std::memory_order_relaxed) & (SIZE - 1)] = sample;
        writePos.fetch_add(1, std::memory_order_release);
    }

    // Reads the most-recent windowSamples from the buffer, decimated to
    // out.size() evenly-spaced points (oldest → newest).
    void readDecimated(std::vector<float>& out, int windowSamples) const noexcept
    {
        const int n = static_cast<int>(out.size());
        if (n == 0) return;

        const int end    = static_cast<int>(writePos.load(std::memory_order_acquire));
        const int avail  = juce::jmin(end, SIZE);          // samples currently in buffer
        const int window = juce::jmin(windowSamples, avail); // cap to what's available

        if (window == 0)
        {
            std::fill(out.begin(), out.end(), 0.0f);
            return;
        }

        const int start = end - window;
        for (int i = 0; i < n; i++)
        {
            // Map display index i → source sample index via even decimation.
            const int srcOff = static_cast<int>(
                static_cast<float>(i) * static_cast<float>(window)
                / static_cast<float>(n));
            out[static_cast<size_t>(i)] = buf[(start + srcOff) & (SIZE - 1)];
        }
    }

private:
    float buf[SIZE] = {};
    std::atomic<int> writePos { 0 };
};

// ---------------------------------------------------------------------------
// Waveform display component.
// Reads from external WaveformBuffer pointers (owned by the processor).
// Safe: editor is always destroyed before the processor.
// ---------------------------------------------------------------------------
class WaveformDisplay : public juce::Component
{
public:
    static constexpr int MAX_STAGES  = 5;
    static constexpr int DISPLAY_PTS = 1024;

    WaveformDisplay()
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

    void setStageColour(int idx, juce::Colour c)
    {
        if (idx >= 0 && idx < MAX_STAGES) stageColours[idx] = c;
    }

    void paint(juce::Graphics& g) override
    {
        auto bounds = getLocalBounds().toFloat();

        // Background
        g.setColour(juce::Colour(0xFF0C0C12));
        g.fillRoundedRectangle(bounds, 4.0f);

        // Label
        g.setColour(juce::Colour(0xFF44445A));
        g.setFont(9.0f);
        g.drawText("WAVEFORM  (gray = pre-compress, coloured = per-stage output)  |  10 s window",
                   bounds.reduced(6.0f, 2.0f).toNearestInt(),
                   juce::Justification::topLeft);

        auto waveBounds = bounds.reduced(2.0f, 16.0f);

        // Centre guide line
        g.setColour(juce::Colour(0xFF1E1E2A));
        g.drawHorizontalLine(static_cast<int>(waveBounds.getCentreY()),
                             waveBounds.getX(), waveBounds.getRight());

        if (inputSrc == nullptr) return;

        const int windowSamples = static_cast<int>(sampleRate * 10.0);

        // Draw input waveform in gray (back layer)
        inputSrc->readDecimated(samples, windowSamples);
        g.setColour(juce::Colour(0xFF4A4A5A));
        drawWave(g, waveBounds, 1.5f);

        // Draw each active stage output (front layers)
        for (int i = 0; i < numActiveSt && i < MAX_STAGES; i++)
        {
            if (stageSrc[i] == nullptr) continue;
            stageSrc[i]->readDecimated(samples, windowSamples);
            g.setColour(stageColours[i].withAlpha(0.80f));
            drawWave(g, waveBounds, 1.2f);
        }

        // Border
        g.setColour(juce::Colour(0xFF282835));
        g.drawRoundedRectangle(bounds.reduced(0.5f), 4.0f, 1.0f);
    }

private:
    WaveformBuffer* inputSrc            = nullptr;
    WaveformBuffer* stageSrc[MAX_STAGES] = {};
    int    numActiveSt = 0;
    double sampleRate  = 44100.0;
    std::vector<float> samples;

    juce::Colour stageColours[MAX_STAGES] = {
        juce::Colour(0xFF4488CC),   // 1176  blue
        juce::Colour(0xFF66CC44),   // LA-2A green
        juce::Colour(0xFFCC8833),   // Dest  amber
        juce::Colour(0xFFCC3333),   // DBX   red
        juce::Colour(0xFF9944CC),   // Neve  purple
    };

    void drawWave(juce::Graphics& g,
                  juce::Rectangle<float> b,
                  float thickness) const
    {
        if (b.getWidth() <= 0.0f || b.getHeight() <= 0.0f) return;

        const int   n  = DISPLAY_PTS;
        const float w  = b.getWidth();
        const float cy = b.getCentreY();
        const float h2 = b.getHeight() * 0.44f;

        juce::Path path;
        bool first = true;
        for (int i = 0; i < n; i++)
        {
            float x = b.getX() + static_cast<float>(i) / static_cast<float>(n - 1) * w;
            float y = cy - juce::jlimit(-1.0f, 1.0f, samples[static_cast<size_t>(i)]) * h2;
            if (first) { path.startNewSubPath(x, y); first = false; }
            else        path.lineTo(x, y);
        }
        g.strokePath(path, juce::PathStrokeType(thickness));
    }
};
