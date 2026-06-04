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

    void setEnabled(bool e)           { enabled = e; }
    void setModel(CompressorModel m);
    void setThreshold(float dB)       { threshold = dB; }
    void setRatio(float r)            { ratio = std::max(1.0f, r); }
    void setAttack(float ms);
    void setRelease(float ms);
    void setSaturation(float s)       { saturationAmount = juce::jlimit(0.0f, 1.0f, s); }
    void setMix(float m)              { mix = juce::jlimit(0.0f, 1.0f, m); }

    float getGainReductionDB() const  { return displayGainReductionDB.load(); }

private:
    double sampleRate = 44100.0;
    bool   prepared   = false;

    bool            enabled          = true;
    CompressorModel model            = CompressorModel::ModelManual;
    float           threshold        = -18.0f;
    float           ratio            = 4.0f;
    float           attackMs         = 10.0f;
    float           releaseMs        = 200.0f;
    float           saturationAmount = 0.0f;
    float           mix              = 1.0f;

    juce::AudioBuffer<float> dryBuffer;

    float fastEnv[2] = { 0.0f, 0.0f };
    float slowEnv[2] = { 0.0f, 0.0f };

    std::atomic<float> displayGainReductionDB { 0.0f };

    float attackCoeff  = 0.0f;
    float releaseCoeff = 0.0f;
    float slowAtkCoeff = 0.0f;
    float slowRelCoeff = 0.0f;

    float computeGainReductionDB(float levelDB) const noexcept;
    float applySaturation(float sample) const noexcept;
    void  updateCoefficients();

    static float makeCoeff(float timeMs, double sr) noexcept
    {
        return std::exp(-1000.0f / (static_cast<float>(sr) * timeMs));
    }
};
