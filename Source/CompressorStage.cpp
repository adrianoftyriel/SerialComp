#include <JuceHeader.h>
#include "CompressorStage.h"

namespace
{
    constexpr float kLn10Over20 = 0.11512925465f;   // ln(10) / 20

    inline float toDB(float lin) noexcept   { return 20.0f * std::log10(lin); }
    inline float toGain(float dB) noexcept  { return std::exp(dB * kLn10Over20); }
}

CompressorStage::CompressorStage()
{
    updateCoefficients();
    updateShaper();
}

void CompressorStage::prepare(double sr, int samplesPerBlock)
{
    sampleRate   = sr;
    maxBlockSize = samplesPerBlock;
    prepared     = true;
    dryBuffer.setSize(2, samplesPerBlock, false, true, false);
    oversampler.initProcessing(static_cast<size_t>(samplesPerBlock));
    reset();
    updateCoefficients();
    mixApplied = 0.0f;
}

void CompressorStage::reset()
{
    for (int ch = 0; ch < 2; ch++)
    {
        fastEnv[ch] = slowEnv[ch] = kFloorDB;
        dcX[ch] = dcY[ch] = 0.0f;
    }
    dbxPower = 0.0f;
    oversampler.reset();
    displayGainReductionDB.store(0.0f);
}

void CompressorStage::setModel(CompressorModel m)
{
    if (m == model) return;
    model = m;

    // Fixed per-model knee width.
    switch (model)
    {
        case CompressorModel::Model1176:       kneeDB = 0.0f; break;  // hard knee, FET
        case CompressorModel::ModelLA2A:       kneeDB = 6.0f; break;  // very soft, optical
        case CompressorModel::ModelDestressor: kneeDB = 2.0f; break;
        case CompressorModel::ModelDBX160:     kneeDB = 0.0f; break;  // hard knee, VCA
        case CompressorModel::ModelNeve:       kneeDB = 3.0f; break;
        case CompressorModel::ModelSSL:        kneeDB = 2.0f; break;
        default:                               kneeDB = 2.0f; break;
    }
    updateShaper();
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

void CompressorStage::setSaturation(float s)
{
    s = juce::jlimit(0.0f, 1.0f, s);
    if (s == saturationAmount) return;
    saturationAmount = s;
    updateShaper();
}

void CompressorStage::updateCoefficients()
{
    if (sampleRate <= 0.0) return;
    attackCoeff  = makeCoeff(std::max(attackMs,  0.001f), sampleRate);
    releaseCoeff = makeCoeff(std::max(releaseMs,  1.0f),  sampleRate);
    slowAtkCoeff = makeCoeff(80.0f,   sampleRate);
    slowRelCoeff = makeCoeff(2500.0f, sampleRate);
    dcCoeff      = 1.0f - juce::MathConstants<float>::twoPi * 5.0f / static_cast<float>(sampleRate);
}

// ---------------------------------------------------------------------------
// Input saturation — model character, peak-normalised (x = ±1 maps to ≈ ±1).
// Called only when the amount or model changes, never per sample.
// ---------------------------------------------------------------------------
void CompressorStage::updateShaper()
{
    // Per-model: maximum drive, maximum bias in the drive domain (even-harmonic
    // asymmetry), shape.
    float maxDrive = 0.0f, maxBias = 0.0f;
    bool  atan_    = false;

    switch (model)
    {
        case CompressorModel::Model1176:       maxDrive = 9.0f;  maxBias = 0.35f; break; // FET
        case CompressorModel::ModelLA2A:       maxDrive = 5.5f;  maxBias = 0.25f; atan_ = true; break; // tube/opto
        case CompressorModel::ModelDestressor: maxDrive = 7.0f;  maxBias = 0.20f; break;
        case CompressorModel::ModelDBX160:     maxDrive = 3.5f;  maxBias = 0.0f;  break; // clean VCA
        case CompressorModel::ModelNeve:       maxDrive = 6.5f;  maxBias = 0.45f; break; // transformer
        case CompressorModel::ModelSSL:        maxDrive = 4.5f;  maxBias = 0.15f; break;
        default: break;
    }

    const bool wasActive = saturationActive;
    saturationActive = saturationAmount > 0.001f && maxDrive > 0.0f;

    if (saturationActive && ! wasActive)
    {
        oversampler.reset();
        for (int ch = 0; ch < 2; ch++) dcX[ch] = dcY[ch] = 0.0f;
    }

    shaper.useAtan = atan_;
    shaper.drive   = saturationAmount * maxDrive;
    shaper.bias    = saturationAmount * maxBias;

    auto f = [atan_](float v) { return atan_ ? std::atan(v) : std::tanh(v); };
    shaper.offset = f(shaper.bias);
    const float posTop = f(shaper.drive + shaper.bias) - shaper.offset;
    const float negTop = shaper.offset - f(shaper.bias - shaper.drive);
    const float top    = std::max(posTop, negTop);
    shaper.invNorm = top > 1.0e-6f ? 1.0f / top : 1.0f;
}

// ---------------------------------------------------------------------------
// Saturation runs at 2x the sample rate to keep the harmonics it generates
// from folding back as aliasing, followed by a 5 Hz DC blocker for the bias.
// The oversampler adds a few samples of latency to the wet path only.
// ---------------------------------------------------------------------------
void CompressorStage::saturateBlock(juce::AudioBuffer<float>& buffer, int numChannels, int numSamples)
{
    const auto s = shaper;

    for (int start = 0; start < numSamples; start += maxBlockSize)
    {
        const int n = juce::jmin(maxBlockSize, numSamples - start);

        juce::dsp::AudioBlock<float> block(buffer);
        block = block.getSubsetChannelBlock(0, static_cast<size_t>(numChannels))
                     .getSubBlock(static_cast<size_t>(start), static_cast<size_t>(n));

        auto up = oversampler.processSamplesUp(block);
        const size_t upN = up.getNumSamples();

        for (int ch = 0; ch < numChannels; ch++)
        {
            float* d = up.getChannelPointer(static_cast<size_t>(ch));
            if (s.useAtan)
                for (size_t i = 0; i < upN; i++)
                    d[i] = (std::atan(s.drive * d[i] + s.bias) - s.offset) * s.invNorm;
            else
                for (size_t i = 0; i < upN; i++)
                    d[i] = (std::tanh(s.drive * d[i] + s.bias) - s.offset) * s.invNorm;
        }

        oversampler.processSamplesDown(block);
    }

    if (s.bias > 0.0f)
    {
        for (int ch = 0; ch < numChannels; ch++)
        {
            float* d = buffer.getWritePointer(ch);
            float x1 = dcX[ch], y1 = dcY[ch];
            for (int i = 0; i < numSamples; i++)
            {
                const float x = d[i];
                y1 = x - x1 + dcCoeff * y1;
                x1 = x;
                d[i] = y1;
            }
            dcX[ch] = x1;
            dcY[ch] = y1;
        }
    }
}

void CompressorStage::decayDisplay(float target) noexcept
{
    const float prev  = displayGainReductionDB.load();
    const float coeff = (target > prev) ? 0.1f : 0.95f;
    displayGainReductionDB.store(coeff * prev + (1.0f - coeff) * target);
}

// ---------------------------------------------------------------------------
// Main processing
// Order: saturation → level detect → gain compute → gain apply → dry/wet mix
// ---------------------------------------------------------------------------
void CompressorStage::processBlock(juce::AudioBuffer<float>& buffer)
{
    if (!prepared) return;

    const int numSamples  = buffer.getNumSamples();
    const int numChannels = juce::jmin(buffer.getNumChannels(), 2);
    if (numSamples == 0 || numChannels == 0) return;

    const float mixTarget = enabled ? mix : 0.0f;

    // Fully bypassed: leave the signal alone, park the detectors, let the
    // meter fall back to zero.
    if (mixTarget == 0.0f && mixApplied == 0.0f)
    {
        for (int ch = 0; ch < 2; ch++) fastEnv[ch] = slowEnv[ch] = kFloorDB;
        dbxPower = 0.0f;
        decayDisplay(0.0f);
        return;
    }

    // The scratch buffer is sized in prepare(); never reallocate on the audio thread.
    if (numSamples > dryBuffer.getNumSamples() || numChannels > dryBuffer.getNumChannels())
        return;

    const float mixStart = mixApplied;
    const bool  needDry  = mixStart < 1.0f || mixTarget < 1.0f;
    if (needDry)
        for (int ch = 0; ch < numChannels; ch++)
            dryBuffer.copyFrom(ch, 0, buffer, ch, 0, numSamples);

    if (saturationActive)
        saturateBlock(buffer, numChannels, numSamples);

    const bool  isLA2A = (model == CompressorModel::ModelLA2A);
    const bool  isSSL  = (model == CompressorModel::ModelSSL);
    const bool  isDBX  = (model == CompressorModel::ModelDBX160);
    const bool  dualTC = isLA2A || isSSL;
    const float blend  = isLA2A ? 0.60f : 0.70f;   // fast/slow weighting

    // Gain-computer constants, hoisted out of the sample loop.
    const float slope    = 1.0f - 1.0f / ratio;
    const float halfKnee = kneeDB * 0.5f;
    const float kneeLow  = threshold - halfKnee;
    const float invKnee2 = kneeDB > 0.0f ? slope / (2.0f * kneeDB) : 0.0f;

    const float mixStep = (mixTarget - mixStart) / static_cast<float>(numSamples);

    float* ch0 = buffer.getWritePointer(0);
    float* ch1 = numChannels > 1 ? buffer.getWritePointer(1) : nullptr;
    const float* dry0 = needDry ? dryBuffer.getReadPointer(0) : nullptr;
    const float* dry1 = (needDry && ch1) ? dryBuffer.getReadPointer(1) : nullptr;

    float fast = fastEnv[0], slow = slowEnv[0], power = dbxPower;
    float sumGR = 0.0f;
    float m = mixStart;

    for (int i = 0; i < numSamples; i++)
    {
        // Stereo-linked level detection on the (saturated) signal
        float peak = std::abs(ch0[i]);
        if (ch1) peak = std::max(peak, std::abs(ch1[i]));

        float effectiveDB;

        if (isDBX)
        {
            // DBX 160: feed-forward RMS-like detection (slightly slower attack envelope)
            const float p2 = peak * peak;
            power = (p2 > power ? attackCoeff : releaseCoeff) * power
                  + (1.0f - (p2 > power ? attackCoeff : releaseCoeff)) * p2;
            effectiveDB = 10.0f * std::log10(std::max(power, 1.0e-12f));
        }
        else
        {
            const float levelDB = toDB(std::max(peak, 1.0e-6f));

            const float fc = levelDB > fast ? attackCoeff : releaseCoeff;
            fast = fc * fast + (1.0f - fc) * levelDB;

            if (dualTC)
            {
                // LA-2A: slow optical element blended with a fast path
                // SSL: auto-release = fast+slow blend at 70/30
                const float sc = levelDB > slow ? slowAtkCoeff : slowRelCoeff;
                slow = sc * slow + (1.0f - sc) * levelDB;
                effectiveDB = blend * fast + (1.0f - blend) * slow;
            }
            else
            {
                effectiveDB = fast;
            }
        }

        // --- Gain computer (soft knee) ---
        float grDB = 0.0f;
        if (effectiveDB > kneeLow)
        {
            const float excess = effectiveDB - threshold;
            if (kneeDB > 0.0f && excess < halfKnee)
            {
                const float x = excess + halfKnee;
                grDB = invKnee2 * x * x;
            }
            else
            {
                grDB = excess * slope;
            }
        }
        sumGR += grDB;

        // --- Apply gain and dry/wet mix ---
        m += mixStep;
        const float g = grDB > 0.0f ? toGain(-grDB) : 1.0f;

        if (needDry)
        {
            const float dm = 1.0f - m;
            ch0[i] = m * (ch0[i] * g) + dm * dry0[i];
            if (ch1) ch1[i] = m * (ch1[i] * g) + dm * dry1[i];
        }
        else
        {
            ch0[i] *= g;
            if (ch1) ch1[i] *= g;
        }
    }

    fastEnv[0] = fast;  slowEnv[0] = slow;  dbxPower = power;
    mixApplied = mixTarget;

    decayDisplay(sumGR / static_cast<float>(numSamples));
}
