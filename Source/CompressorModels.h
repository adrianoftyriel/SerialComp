#pragma once
#include <JuceHeader.h>

static constexpr int MAX_STAGES = 5;

enum class CompressorModel
{
    Model1176 = 0,
    ModelLA2A,
    ModelDestressor,
    ModelDBX160,
    ModelNeve,
    ModelSSL,
    ModelManual,
    NumModels
};

inline juce::StringArray getModelNames()
{
    return { "1176", "LA-2A", "Destressor", "DBX 160", "Neve 33609", "SSL G-Bus", "Manual" };
}

struct ModelDefaults
{
    float threshold;
    float ratio;
    float attackMs;
    float releaseMs;
    float saturationDefault; // 0-1 input drive amount
    float mix;
    bool  programDependentRelease;
    bool  opticalSimulation;
    bool  transformerSaturation;
    bool  fetCharacter;
    bool  vcaCharacter;
};

inline ModelDefaults getModelDefaults(CompressorModel model)
{
    switch (model)
    {
        case CompressorModel::Model1176:
            return { -20.0f, 8.0f, 0.3f, 250.0f, 0.3f, 1.0f,
                     true, false, false, true, false };

        case CompressorModel::ModelLA2A:
            return { -18.0f, 3.5f, 10.0f, 500.0f, 0.2f, 1.0f,
                     true, true, true, false, false };

        case CompressorModel::ModelDestressor:
            return { -18.0f, 6.0f, 1.0f, 150.0f, 0.25f, 1.0f,
                     false, false, false, false, false };

        case CompressorModel::ModelDBX160:
            return { -20.0f, 4.0f, 0.1f, 80.0f, 0.1f, 1.0f,
                     false, false, false, false, true };

        case CompressorModel::ModelNeve:
            return { -15.0f, 4.0f, 5.0f, 350.0f, 0.25f, 1.0f,
                     false, false, true, false, false };

        case CompressorModel::ModelSSL:
            return { -12.0f, 4.0f, 3.0f, 200.0f, 0.15f, 1.0f,
                     true, false, false, false, true };

        case CompressorModel::ModelManual:
        default:
            return { -18.0f, 4.0f, 10.0f, 200.0f, 0.0f, 1.0f,
                     false, false, false, false, false };
    }
}

inline juce::Colour getModelColour(CompressorModel model)
{
    switch (model)
    {
        case CompressorModel::Model1176:      return juce::Colour(0xFF4488CC);
        case CompressorModel::ModelLA2A:      return juce::Colour(0xFF88CC44);
        case CompressorModel::ModelDestressor: return juce::Colour(0xFFCC8844);
        case CompressorModel::ModelDBX160:    return juce::Colour(0xFFCC4444);
        case CompressorModel::ModelNeve:      return juce::Colour(0xFFEE8833);
        case CompressorModel::ModelSSL:       return juce::Colour(0xFFAACC22);
        default:                              return juce::Colour(0xFFAAAAAA);
    }
}
