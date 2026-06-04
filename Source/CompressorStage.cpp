#include <JuceHeader.h>
#include "CompressorStage.h"

CompressorStage::CompressorStage()
{
    updateCoefficients();
}

void CompressorStage::prepare(double sr, int samplesPerBlock)
{
    sampleRate = sr;
    prepared   = true;
    dryBuffer.setSize(2, samplesPerBlock, false, true, false);
    reset();
    updateCoefficients();
}

void CompressorStage::reset()
{
    for (int ch = 0; ch < 2; ch++)
        fastEnv[ch] = slowEnv[ch] = 0.0f;
    displayGainReductionDB.store(0.0f);
}

void CompressorStage::setModel(CompressorModel m)
{
    model = m;
}

void CompressorStage::setAttack(float ms)
{
    if (ms == attackMs) return;
    attackMs = ms;
    updateCoefficients();
}

void CompressorStage::setRelease(float ms)
{
    if (ms == releaseMs) return;
    releaseMs = ms;
    updateCoefficients();
}

void CompressorStage::updateCoefficients()
{
    if (sampleRate <= 0.0) return;
    attackCoeff  = makeCoeff(std::max(attackMs,  0.001f), sampleRate);
    releaseCoeff = makeCoeff(std::max(releaseMs,  1.0f),  sampleRate);
    slowAtkCoeff = makeCoeff(80.0f,   sampleRate);
    slowRelCoeff = makeCoeff(2500.0f, sampleRate);
}

// ---------------------------------------------------------------------------
// Gain computer — model-specific fixed knee
// ---------------------------------------------------------------------------
float CompressorStage::computeGainReductionDB(float levelDB) const noexcept
{
    float kneeDB;
    switch (model)
    {
        case CompressorModel::Model1176:      kneeDB = 0.0f; break;  // hard knee, FET
        case CompressorModel::ModelLA2A:      kneeDB = 6.0f; break;  // very soft, optical
        case CompressorModel::ModelDestressor: kneeDB = 2.0f; break;
        case CompressorModel::ModelDBX160:    kneeDB = 0.0f; break;  // hard knee, VCA
        case CompressorModel::ModelNeve:      kneeDB = 3.0f; break;
        case CompressorModel::ModelSSL:       kneeDB = 2.0f; break;
        default:                              kneeDB = 2.0f; break;
    }

    const float halfKnee = kneeDB * 0.5f;
    const float excess   = levelDB - threshold;

    if (excess < -halfKnee)
        return 0.0f;

    if (kneeDB > 0.0f && excess < halfKnee)
    {
        float x = excess + halfKnee;
        return (1.0f - 1.0f / ratio) * x * x / (2.0f * kneeDB);
    }

    return excess * (1.0f - 1.0f / ratio);
}

// ---------------------------------------------------------------------------
// Input saturation — model character, gain-compensated (peak at ±1 stays ±1)
//
// Formula: tanh(x*drive)/tanh(drive) is gain-compensated by construction.
// Even-harmonic (asymmetric) colour is layered via y*|y| term:
//   y' = (y + alpha*y*|y|) / (1+alpha)   →  peak still ≈ 1 at x=1.
// ---------------------------------------------------------------------------
float CompressorStage::applySaturation(float x) const noexcept
{
    if (saturationAmount < 0.001f)
        return x;

    switch (model)
    {
        case CompressorModel::Model1176:
        {
            // FET: asymmetric hard-drive — positive half clips tighter than negative
            const float driveP = 1.0f + saturationAmount * 8.0f;
            const float driveN = 1.0f + saturationAmount * 5.5f;
            if (x >= 0.0f)
                return std::tanh(x * driveP) / std::tanh(driveP);
            return -(std::tanh(-x * driveN) / std::tanh(driveN));
        }

        case CompressorModel::ModelLA2A:
        {
            // Tube/opto: soft atan-based saturation (warmer than tanh), +2nd harmonic
            const float drive = 1.0f + saturationAmount * 4.5f;
            const float norm  = std::atan(drive) * (2.0f / juce::MathConstants<float>::pi);
            float y = (std::atan(x * drive) * (2.0f / juce::MathConstants<float>::pi)) / norm;
            const float alpha = saturationAmount * 0.14f;
            return (y + alpha * y * std::abs(y)) / (1.0f + alpha);
        }

        case CompressorModel::ModelDestressor:
        {
            // Versatile: blend of FET drive + asymmetric 2nd-harmonic injection
            const float drive = 1.0f + saturationAmount * 6.0f;
            const float norm  = std::tanh(drive);
            float y = std::tanh(x * drive) / norm;
            const float alpha = saturationAmount * 0.09f;
            return (y + alpha * y * std::abs(y)) / (1.0f + alpha);
        }

        case CompressorModel::ModelDBX160:
        {
            // VCA: very clean — only gentle soft-limiting, minimal colouration
            const float drive = 1.0f + saturationAmount * 2.5f;
            return std::tanh(x * drive) / std::tanh(drive);
        }

        case CompressorModel::ModelNeve:
        {
            // Transformer: rich 2nd+3rd harmonics, clearly asymmetric
            const float drive = 1.0f + saturationAmount * 5.5f;
            const float norm  = std::tanh(drive);
            float y = std::tanh(x * drive) / norm;
            const float alpha = saturationAmount * 0.20f;
            return (y + alpha * y * std::abs(y)) / (1.0f + alpha);
        }

        case CompressorModel::ModelSSL:
        {
            // VCA bus: slightly warmer than DBX, faint even harmonics
            const float drive = 1.0f + saturationAmount * 3.5f;
            const float norm  = std::tanh(drive);
            float y = std::tanh(x * drive) / norm;
            const float alpha = saturationAmount * 0.06f;
            return (y + alpha * y * std::abs(y)) / (1.0f + alpha);
        }

        default:
            return x;
    }
}

// ---------------------------------------------------------------------------
// Main processing
// Order: saturation → level detect → gain compute → gain apply → dry/wet mix
// ---------------------------------------------------------------------------
void CompressorStage::processBlock(juce::AudioBuffer<float>& buffer)
{
    if (!enabled || !prepared) return;

    const int numSamples  = buffer.getNumSamples();
    const int numChannels = juce::jmin(buffer.getNumChannels(), 2);
    if (numSamples == 0 || numChannels == 0) return;

    dryBuffer.setSize(numChannels, numSamples, false, false, true);
    for (int ch = 0; ch < numChannels; ch++)
        dryBuffer.copyFrom(ch, 0, buffer, ch, 0, numSamples);

    // --- Apply input saturation (gain-compensated, before compression) ---
    if (saturationAmount > 0.001f)
    {
        for (int i = 0; i < numSamples; i++)
            for (int ch = 0; ch < numChannels; ch++)
                buffer.setSample(ch, i, applySaturation(buffer.getSample(ch, i)));
    }

    const bool isLA2A = (model == CompressorModel::ModelLA2A);
    const bool isSSL  = (model == CompressorModel::ModelSSL);
    const bool dualTC = isLA2A || isSSL;

    float sumGR = 0.0f;

    for (int i = 0; i < numSamples; i++)
    {
        // Stereo-linked level detection on (now-saturated) signal
        float peak = 0.0f;
        for (int ch = 0; ch < numChannels; ch++)
            peak = std::max(peak, std::abs(buffer.getSample(ch, i)));

        float levelDB = juce::Decibels::gainToDecibels(std::max(peak, 1e-9f));

        // --- Model-specific envelope followers ---
        float effectiveDB;

        if (dualTC)
        {
            // LA-2A: slow optical element blended with a fast path
            // SSL: auto-release = fast+slow blend at 70/30
            if (levelDB > fastEnv[0])
                fastEnv[0] = attackCoeff  * fastEnv[0] + (1.0f - attackCoeff)  * levelDB;
            else
                fastEnv[0] = releaseCoeff * fastEnv[0] + (1.0f - releaseCoeff) * levelDB;

            if (levelDB > slowEnv[0])
                slowEnv[0] = slowAtkCoeff * slowEnv[0] + (1.0f - slowAtkCoeff) * levelDB;
            else
                slowEnv[0] = slowRelCoeff * slowEnv[0] + (1.0f - slowRelCoeff) * levelDB;

            // LA-2A: opto element is dominated by the slow path at high GR
            // SSL: auto-release blends fast (70%) + slow (30%)
            float blend = isLA2A ? 0.60f : 0.70f;
            effectiveDB = blend * fastEnv[0] + (1.0f - blend) * slowEnv[0];
        }
        else if (model == CompressorModel::ModelDBX160)
        {
            // DBX 160: feed-forward RMS-like detection (slightly slower attack envelope)
            float rmsApprox = peak * peak;
            if (rmsApprox > fastEnv[0])
                fastEnv[0] = attackCoeff  * fastEnv[0] + (1.0f - attackCoeff)  * rmsApprox;
            else
                fastEnv[0] = releaseCoeff * fastEnv[0] + (1.0f - releaseCoeff) * rmsApprox;
            effectiveDB = juce::Decibels::gainToDecibels(std::sqrt(std::max(fastEnv[0], 1e-18f)));
        }
        else
        {
            // Standard peak follower (1176, Neve, Destressor, Manual)
            if (levelDB > fastEnv[0])
                fastEnv[0] = attackCoeff  * fastEnv[0] + (1.0f - attackCoeff)  * levelDB;
            else
                fastEnv[0] = releaseCoeff * fastEnv[0] + (1.0f - releaseCoeff) * levelDB;
            effectiveDB = fastEnv[0];
        }

        // --- Gain computer ---
        float grDB = computeGainReductionDB(effectiveDB);
        sumGR += grDB;

        const float gainLin = juce::Decibels::decibelsToGain(-grDB);

        // --- Apply gain and write output ---
        for (int ch = 0; ch < numChannels; ch++)
        {
            float wet = buffer.getSample(ch, i) * gainLin;
            wet = juce::jlimit(-2.0f, 2.0f, wet);
            float dry = dryBuffer.getSample(ch, i);
            buffer.setSample(ch, i, mix * wet + (1.0f - mix) * dry);
        }
    }

    // Smooth the display GR value: fast attack, slow release on the meter
    float avgGR  = sumGR / static_cast<float>(numSamples);
    float prevGR = displayGainReductionDB.load();
    float coeff  = (avgGR > prevGR) ? 0.1f : 0.95f;
    displayGainReductionDB.store(coeff * prevGR + (1.0f - coeff) * avgGR);
}
