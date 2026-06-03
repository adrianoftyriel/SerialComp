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
    // Pre-allocate so processBlock never calls new/delete on the audio thread.
    dryBuffer.setSize(2, samplesPerBlock, false, true, false);
    reset();
    updateCoefficients();
}

void CompressorStage::reset()
{
    for (int ch = 0; ch < 2; ch++)
    {
        fastEnv[ch] = 0.0f;
        slowEnv[ch] = 0.0f;
    }
    displayGainReductionDB.store(0.0f);
}

void CompressorStage::setModel(CompressorModel m)
{
    model = m;
}

void CompressorStage::setAttack(float ms)
{
    attackMs = ms;
    updateCoefficients();
}

void CompressorStage::setRelease(float ms)
{
    releaseMs = ms;
    updateCoefficients();
}

void CompressorStage::updateCoefficients()
{
    if (sampleRate <= 0.0) return;
    attackCoeff  = makeCoeff(std::max(attackMs,  0.001f), sampleRate);
    releaseCoeff = makeCoeff(std::max(releaseMs,  1.0f),  sampleRate);
    slowAtkCoeff = makeCoeff(100.0f,  sampleRate);  // ~100 ms slow attack
    slowRelCoeff = makeCoeff(2000.0f, sampleRate);  // ~2 s slow release tail
}

// ---------------------------------------------------------------------------
// Gain computer: returns gain reduction in dB (positive = reduction)
// ---------------------------------------------------------------------------
float CompressorStage::computeGainReductionDB(float levelDB) const noexcept
{
    const float halfKnee = kneeDB * 0.5f;
    const float excess   = levelDB - threshold;

    if (excess < -halfKnee)
        return 0.0f;

    if (kneeDB > 0.0f && excess < halfKnee)
    {
        // Soft-knee region
        float x = excess + halfKnee;
        return (1.0f - 1.0f / ratio) * x * x / (2.0f * kneeDB);
    }

    return excess * (1.0f - 1.0f / ratio);
}

// ---------------------------------------------------------------------------
// Subtle harmonic saturation per model character
// All functions are safe at typical audio levels (±1).
// ---------------------------------------------------------------------------
float CompressorStage::applySaturation(float s) const noexcept
{
    switch (model)
    {
        case CompressorModel::Model1176:
            // FET: mild tanh limiting (odd harmonics)
            return std::tanh(s * 1.06f) / 1.06f;

        case CompressorModel::ModelLA2A:
            // Opto/tube: soft cubic (dominantly 3rd harmonic, warm)
            return s - 0.07f * s * s * s;

        case CompressorModel::ModelNeve:
            // Transformer: asymmetric saturation (2nd harmonic flavour)
            return s + 0.005f * s * s - 0.003f * s * s * s;

        case CompressorModel::ModelDestressor:
            // British Mode: blend of tanh + 2nd harmonic
            return std::tanh(s * 1.04f) / 1.04f
                 + 0.004f * s * s * std::copysign(1.0f, s);

        default:
            return s;
    }
}

// ---------------------------------------------------------------------------
// Main processing
// ---------------------------------------------------------------------------
void CompressorStage::processBlock(juce::AudioBuffer<float>& buffer)
{
    if (!enabled || !prepared) return;

    const int numSamples  = buffer.getNumSamples();
    const int numChannels = juce::jmin(buffer.getNumChannels(), 2);
    if (numSamples == 0 || numChannels == 0) return;

    const float inGainLin    = juce::Decibels::decibelsToGain(inputGainDB);
    const float makeupLin    = juce::Decibels::decibelsToGain(makeupGainDB);

    // Use pre-allocated dry buffer – reallocates only if host changed block size
    // (avoidReallocating=true keeps the existing allocation when it already fits)
    dryBuffer.setSize(numChannels, numSamples, false, false, true);
    for (int ch = 0; ch < numChannels; ch++)
        dryBuffer.copyFrom(ch, 0, buffer, ch, 0, numSamples);

    float sumGR = 0.0f;

    const bool isLA2A = (model == CompressorModel::ModelLA2A);
    const bool isSSL  = (model == CompressorModel::ModelSSL);
    const bool dualTC = isLA2A || isSSL;

    for (int i = 0; i < numSamples; i++)
    {
        // Stereo-linked peak detection on input-gained signal
        float peak = 0.0f;
        for (int ch = 0; ch < numChannels; ch++)
            peak = std::max(peak, std::abs(buffer.getSample(ch, i)) * inGainLin);

        float levelDB = juce::Decibels::gainToDecibels(std::max(peak, 1e-9f));

        // --- Envelope followers ---
        float effectiveDB;

        if (dualTC)
        {
            // Dual time-constant for LA-2A (optical) and SSL (auto-release)
            if (levelDB > fastEnv[0])
                fastEnv[0] = attackCoeff  * fastEnv[0] + (1.0f - attackCoeff)  * levelDB;
            else
                fastEnv[0] = releaseCoeff * fastEnv[0] + (1.0f - releaseCoeff) * levelDB;

            if (levelDB > slowEnv[0])
                slowEnv[0] = slowAtkCoeff * slowEnv[0] + (1.0f - slowAtkCoeff) * levelDB;
            else
                slowEnv[0] = slowRelCoeff * slowEnv[0] + (1.0f - slowRelCoeff) * levelDB;

            // LA-2A: fast-dominant blend; SSL auto-release is more even
            float blend = isLA2A ? 0.65f : 0.70f;
            effectiveDB = blend * fastEnv[0] + (1.0f - blend) * slowEnv[0];
        }
        else
        {
            if (levelDB > fastEnv[0])
                fastEnv[0] = attackCoeff  * fastEnv[0] + (1.0f - attackCoeff)  * levelDB;
            else
                fastEnv[0] = releaseCoeff * fastEnv[0] + (1.0f - releaseCoeff) * levelDB;

            effectiveDB = fastEnv[0];
        }

        // --- Gain computer ---
        float grDB = computeGainReductionDB(effectiveDB);
        sumGR += grDB;

        // Total gain applied to wet signal: input + reduction + makeup
        float totalLin = inGainLin
                       * juce::Decibels::decibelsToGain(-grDB)
                       * makeupLin;

        // --- Apply to each channel ---
        for (int ch = 0; ch < numChannels; ch++)
        {
            float wet = buffer.getSample(ch, i) * totalLin;
            wet = applySaturation(wet);
            wet = juce::jlimit(-2.0f, 2.0f, wet);  // prevent extreme blowup
            float dry = dryBuffer.getSample(ch, i);
            buffer.setSample(ch, i, mix * wet + (1.0f - mix) * dry);
        }
    }

    // Update meter display with a block-rate smoothing
    float avgGR  = sumGR / static_cast<float>(numSamples);
    float prevGR = displayGainReductionDB.load();
    // Fast attack, slow release on the meter
    float coeff  = (avgGR > prevGR) ? 0.1f : 0.95f;
    displayGainReductionDB.store(coeff * prevGR + (1.0f - coeff) * avgGR);
}
