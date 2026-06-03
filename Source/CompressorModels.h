#pragma once
#include <JuceHeader.h>

static constexpr int MAX_STAGES = 5;

enum class CompressorModel
{
    Model1176 = 0,
    ModelLA2A,
    ModelDestressor,
    ModelDBX160A,
    ModelNeve,
    ModelSSL,
    ModelManual,
    NumModels
};

inline juce::StringArray getModelNames()
{
    return { "1176", "LA-2A", "Destressor", "DBX 160A", "Neve 33609", "SSL G-Bus", "Manual" };
}

struct ModelDefaults
{
    float threshold;   // dB
    float ratio;
    float attackMs;
    float releaseMs;
    float kneeDB;
    float inputGainDB;
    float makeupGainDB;
    float mix;         // 0–1
    // Processing character flags
    bool programDependentRelease;
    bool opticalSimulation;
    bool transformerSaturation;
    bool fetCharacter;
    bool vcaCharacter;
};

inline ModelDefaults getModelDefaults(CompressorModel model)
{
    switch (model)
    {
        case CompressorModel::Model1176:
            // FET: very fast, hard knee, discrete ratios (4/8/12/20)
            return { -20.0f, 8.0f, 0.3f, 250.0f, 0.0f, 0.0f, 6.0f, 1.0f,
                     true, false, false, true, false };

        case CompressorModel::ModelLA2A:
            // Optical: program-dependent, fixed soft-knee, ~3:1
            return { -18.0f, 3.5f, 10.0f, 500.0f, 6.0f, 0.0f, 6.0f, 1.0f,
                     true, true, true, false, false };

        case CompressorModel::ModelDestressor:
            // Empirical Labs: versatile, British mode character
            return { -18.0f, 6.0f, 1.0f, 150.0f, 1.0f, 0.0f, 4.0f, 1.0f,
                     false, false, false, false, false };

        case CompressorModel::ModelDBX160A:
            // VCA: feed-forward, hard knee, ultra fast
            return { -20.0f, 4.0f, 0.1f, 80.0f, 0.0f, 0.0f, 4.0f, 1.0f,
                     false, false, false, false, true };

        case CompressorModel::ModelNeve:
            // Transformer-based: musical, warm saturation
            return { -15.0f, 4.0f, 5.0f, 350.0f, 3.0f, 0.0f, 4.0f, 1.0f,
                     false, false, true, false, false };

        case CompressorModel::ModelSSL:
            // SSL G-Bus: VCA, auto-release, glue compression
            return { -12.0f, 4.0f, 3.0f, 200.0f, 2.0f, 0.0f, 3.0f, 1.0f,
                     true, false, false, false, true };

        case CompressorModel::ModelManual:
        default:
            return { -18.0f, 4.0f, 10.0f, 200.0f, 2.0f, 0.0f, 0.0f, 1.0f,
                     false, false, false, false, false };
    }
}

// Per-model accent colours used in the UI
inline juce::Colour getModelColour(CompressorModel model)
{
    switch (model)
    {
        case CompressorModel::Model1176:     return juce::Colour(0xFF4488CC); // blue
        case CompressorModel::ModelLA2A:     return juce::Colour(0xFF88CC44); // green
        case CompressorModel::ModelDestressor: return juce::Colour(0xFFCC8844); // amber
        case CompressorModel::ModelDBX160A:  return juce::Colour(0xFFCC4444); // red
        case CompressorModel::ModelNeve:     return juce::Colour(0xFF8844CC); // purple
        case CompressorModel::ModelSSL:      return juce::Colour(0xFF44CCCC); // teal
        default:                             return juce::Colour(0xFFAAAAAA); // grey
    }
}
