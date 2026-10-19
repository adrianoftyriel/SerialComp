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
    void setSaturation(float s);
    void setMix(float m)              { mix = juce::jlimit(0.0f, 1.0f, m); }

    float getGainReductionDB() const  { return displayGainReductionDB.load(); }

private:
    static constexpr float kFloorDB = -120.0f;

    double sampleRate = 44100.0;
    int    maxBlockSize = 0;
    bool   prepared   = false;

    bool            enabled          = true;
    CompressorModel model            = CompressorModel::ModelManual;
    float           threshold        = -18.0f;
    float           ratio            = 4.0f;
    float           attackMs         = 10.0f;
    float           releaseMs        = 200.0f;
    float           saturationAmount = 0.0f;
    float           mix              = 1.0f;
    float           kneeDB           = 2.0f;   // per-model, set in setModel()

    // Wet/dry amount actually applied; ramps toward (enabled ? mix : 0) so
    // toggling a stage or moving the mix knob never steps the output.
    float mixApplied = 0.0f;

    juce::AudioBuffer<float> dryBuffer;

    float fastEnv[2] = { kFloorDB, kFloorDB };
    float slowEnv[2] = { kFloorDB, kFloorDB };
    float dbxPower   = 0.0f;                    // DBX 160 linear-power detector

    std::atomic<float> displayGainReductionDB { 0.0f };

    float attackCoeff  = 0.0f;
    float releaseCoeff = 0.0f;
    float slowAtkCoeff = 0.0f;
    float slowRelCoeff = 0.0f;
    float dcCoeff      = 0.999f;

    // Saturation: out = (f(d*x + b) - f(b)) * invNorm, with f = tanh or atan.
    //  - d -> 0 as the amount -> 0, so the curve fades to unity (no level step
    //    when saturation is switched on).
    //  - bias b gives true even-order harmonics (the old y*|y| term was odd).
    //  - invNorm scales the larger of the two rails to ±1 (peaks never exceed ±1).
    struct Shaper
    {
        bool  useAtan = false;
        float drive   = 0.0f;
        float bias    = 0.0f;
        float offset  = 0.0f;
        float invNorm = 1.0f;
    } shaper;
    bool  saturationActive = false;
    float dcX[2] = { 0.0f, 0.0f };
    float dcY[2] = { 0.0f, 0.0f };

    juce::dsp::Oversampling<float> oversampler { 2, 1,
        juce::dsp::Oversampling<float>::filterHalfBandPolyphaseIIR, true, true };

    void  updateCoefficients();
    void  updateShaper();
    void  saturateBlock(juce::AudioBuffer<float>& buffer, int numChannels, int numSamples);
    void  decayDisplay(float target) noexcept;

    static float makeCoeff(float timeMs, double sr) noexcept
    {
        return std::exp(-1000.0f / (static_cast<float>(sr) * timeMs));
    }
};
