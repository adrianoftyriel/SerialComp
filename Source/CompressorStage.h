#pragma once
#include <JuceHeader.h>
#include "CompressorModels.h"

class CompressorStage
{
public:
    CompressorStage();
    ~CompressorStage() = default;

    void prepare(double sampleRate, int samplesPerBlock);
    void reset();
    void processBlock(juce::AudioBuffer<float>& buffer);

    void setEnabled(bool e)          { enabled = e; }
    void setModel(CompressorModel m);
    void setThreshold(float dB)      { threshold = dB; }
    void setRatio(float r)           { ratio = std::max(1.0f, r); }
    void setAttack(float ms);
    void setRelease(float ms);
    void setKnee(float dB)           { kneeDB = juce::jmax(0.0f, dB); }
    void setInputGain(float dB)      { inputGainDB = dB; }
    void setMakeupGain(float dB)     { makeupGainDB = dB; }
    void setMix(float m)             { mix = juce::jlimit(0.0f, 1.0f, m); }

    float getGainReductionDB() const { return displayGainReductionDB.load(); }

private:
    double sampleRate = 44100.0;
    bool prepared = false;

    // Parameters
    bool           enabled      = true;
    CompressorModel model       = CompressorModel::ModelManual;
    float threshold             = -18.0f;
    float ratio                 = 4.0f;
    float attackMs              = 10.0f;
    float releaseMs             = 200.0f;
    float kneeDB                = 2.0f;
    float inputGainDB           = 0.0f;
    float makeupGainDB          = 0.0f;
    float mix                   = 1.0f;

    // Pre-allocated dry buffer (avoids heap alloc in the audio thread)
    juce::AudioBuffer<float> dryBuffer;

    // Envelope state – per channel (max 2)
    float fastEnv[2]  = { 0.0f, 0.0f };
    float slowEnv[2]  = { 0.0f, 0.0f };

    // Smoothed display value
    std::atomic<float> displayGainReductionDB { 0.0f };

    // Coefficients
    float attackCoeff     = 0.0f;
    float releaseCoeff    = 0.0f;
    float slowAtkCoeff    = 0.0f;  // for optical / auto-release tail
    float slowRelCoeff    = 0.0f;

    float computeGainReductionDB(float levelDB) const noexcept;
    float applySaturation(float sample) const noexcept;
    void  updateCoefficients();

    static float makeCoeff(float timeMs, double sr) noexcept
    {
        return std::exp(-1000.0f / (static_cast<float>(sr) * timeMs));
    }
};
