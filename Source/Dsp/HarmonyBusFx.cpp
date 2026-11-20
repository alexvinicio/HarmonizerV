#include "Dsp/HarmonyBusFx.h"

#include <algorithm>
#include <cmath>

namespace harmonizer::dsp
{

// ============================================================================
// HarmonyDeEsser
// ============================================================================

void HarmonyDeEsser::prepare(double sampleRate)
{
    fs = std::max(8000.0, sampleRate);
    scBand.setParams(6800.0, 1.0, fs);
    const double splitCut = std::min(3800.0, fs * 0.40);
    splitAlpha = 1.0 - std::exp(-kTwoPi * splitCut / fs);

    attackAlpha = static_cast<float>(1.0 - std::exp(-1.0 / (0.003 * fs)));
    releaseAlpha = static_cast<float>(1.0 - std::exp(-1.0 / (0.055 * fs)));
    broadAlpha = static_cast<float>(1.0 - std::exp(-1.0 / (0.015 * fs)));
    reset();
}

void HarmonyDeEsser::reset() noexcept
{
    scBand.reset();
    splitLpL = 0.0;
    splitLpR = 0.0;
    envSib = 0.0f;
    envBroad = 0.0f;
    smoothedGain = 1.0f;
    lastReductionDb = 0.0f;
}

void HarmonyDeEsser::process(float* wetL, float* wetR, int numSamples, float deEssAmount) noexcept
{
    const float amount = std::clamp(deEssAmount, 0.0f, 1.0f);
    if (amount <= 0.001f || numSamples <= 0)
    {
        smoothedGain = 1.0f;
        lastReductionDb = 0.0f;
        return;
    }

    // Max high-band attenuation up to -12 dB at amount = 1.0
    const float minGain = dbToGain(-12.0f * amount);
    float minBlockGain = 1.0f;

    for (int i = 0; i < numSamples; ++i)
    {
        const double xL = static_cast<double>(wetL[i]);
        const double xR = static_cast<double>(wetR[i]);
        const double mono = 0.5 * (xL + xR);

        double dummyLp = 0.0, bp = 0.0, dummyHp = 0.0;
        scBand.process(mono, dummyLp, bp, dummyHp);

        const float absSib = static_cast<float>(std::abs(bp));
        const float absBroad = static_cast<float>(std::abs(mono));

        envSib += ((absSib > envSib) ? attackAlpha : releaseAlpha) * (absSib - envSib);
        envBroad += broadAlpha * (absBroad - envBroad);

        // Sibilance triggers when HF energy exceeds broadband vocal body ratio + floor
        const float thresh = std::max(0.012f, envBroad * (0.48f - 0.22f * amount));
        const float excess = std::max(0.0f, envSib - thresh);
        const float targetGain = std::max(minGain, 1.0f / (1.0f + excess * (12.0f * amount)));

        const float gainAlpha = (targetGain < smoothedGain) ? attackAlpha : releaseAlpha;
        smoothedGain += gainAlpha * (targetGain - smoothedGain);
        minBlockGain = std::min(minBlockGain, smoothedGain);

        // Complementary 1-pole high-shelf split: lp + hp == x exactly, zero notch/phase artifact
        splitLpL += splitAlpha * (xL - splitLpL);
        splitLpR += splitAlpha * (xR - splitLpR);
        const double hpL = xL - splitLpL;
        const double hpR = xR - splitLpR;

        wetL[i] = sanitizeSample(static_cast<float>(splitLpL + hpL * static_cast<double>(smoothedGain)));
        wetR[i] = sanitizeSample(static_cast<float>(splitLpR + hpR * static_cast<double>(smoothedGain)));
    }

    lastReductionDb = std::clamp(-gainToDb(minBlockGain), 0.0f, 12.0f);
}

// ============================================================================
// VocalSpace
// ============================================================================

void VocalSpace::prepare(double sampleRate)
{
    fs = std::max(8000.0, sampleRate);

    inHpAlpha = static_cast<float>(1.0 - std::exp(-kTwoPi * 180.0 / fs));
    inLpAlpha = static_cast<float>(1.0 - std::exp(-kTwoPi * std::min(6500.0, fs * 0.4) / fs));
    dampAlpha = static_cast<float>(1.0 - std::exp(-kTwoPi * std::min(5200.0, fs * 0.4) / fs));
    mixAlpha = static_cast<float>(1.0 - std::exp(-1.0 / (0.020 * fs)));

    diffA.prepare(fs, 4.7, 0.50f);
    diffB.prepare(fs, 7.1, 0.45f);

    constexpr std::array<double, kNumFdnLines> kBaseMs = { 19.3, 27.1, 37.7, 49.9 };
    constexpr std::array<double, kNumFdnLines> kModRatesHz = { 0.37, 0.51, 0.63, 0.79 };

    for (int i = 0; i < kNumFdnLines; ++i)
    {
        fdnBaseSamples[static_cast<size_t>(i)] = static_cast<float>(fs * kBaseMs[static_cast<size_t>(i)] * 0.001);
        const int cap = static_cast<int>(std::ceil(fdnBaseSamples[static_cast<size_t>(i)])) + static_cast<int>(std::ceil(fs * 0.004)) + 16;
        fdnDelays[static_cast<size_t>(i)].prepare(cap);
        modInc[static_cast<size_t>(i)] = kTwoPi * kModRatesHz[static_cast<size_t>(i)] / fs;
    }

    reset();
}

void VocalSpace::reset() noexcept
{
    inHpState = 0.0f;
    inLpState = 0.0f;
    diffA.reset();
    diffB.reset();
    for (int i = 0; i < kNumFdnLines; ++i)
    {
        fdnDelays[static_cast<size_t>(i)].reset();
        fdnLpState[static_cast<size_t>(i)] = 0.0f;
        modPhase[static_cast<size_t>(i)] = static_cast<double>(i) * 1.57;
    }
    smoothedWetMix = 0.0f;
}

void VocalSpace::process(float* wetL, float* wetR, int numSamples, float spaceAmount) noexcept
{
    const float targetMix = std::clamp(spaceAmount, 0.0f, 1.0f) * 0.42f;
    if (targetMix <= 0.001f && smoothedWetMix <= 0.0005f)
        return;

    const float modDepthSamp = static_cast<float>(fs * 0.00035); // +/- 0.35 ms lush tail
    constexpr float kFeedback = 0.72f;

    for (int i = 0; i < numSamples; ++i)
    {
        smoothedWetMix += mixAlpha * (targetMix - smoothedWetMix);

        const float monoIn = 0.5f * (wetL[i] + wetR[i]);

        // Pre-condition reverb send: warm band-limited vocal reflection
        inHpState += inHpAlpha * (monoIn - inHpState);
        const float hpOut = monoIn - inHpState;
        inLpState += inLpAlpha * (hpOut - inLpState);

        const float diffused = diffB.process(diffA.process(inLpState));

        std::array<float, kNumFdnLines> taps{};
        for (int c = 0; c < kNumFdnLines; ++c)
        {
            modPhase[static_cast<size_t>(c)] += modInc[static_cast<size_t>(c)];
            if (modPhase[static_cast<size_t>(c)] >= kTwoPi)
                modPhase[static_cast<size_t>(c)] -= kTwoPi;

            const float dSamp = fdnBaseSamples[static_cast<size_t>(c)]
                              + modDepthSamp * static_cast<float>(std::sin(modPhase[static_cast<size_t>(c)]));
            const float rawTap = fdnDelays[static_cast<size_t>(c)].readCubic(dSamp);

            // Gentle HF air damping inside feedback loop
            fdnLpState[static_cast<size_t>(c)] += dampAlpha * (rawTap - fdnLpState[static_cast<size_t>(c)]);
            taps[static_cast<size_t>(c)] = fdnLpState[static_cast<size_t>(c)];
        }

        // Unitary 4x4 Hadamard mixing matrix
        const float h0 = 0.5f * (taps[0] + taps[1] + taps[2] + taps[3]);
        const float h1 = 0.5f * (taps[0] - taps[1] + taps[2] - taps[3]);
        const float h2 = 0.5f * (taps[0] + taps[1] - taps[2] - taps[3]);
        const float h3 = 0.5f * (taps[0] - taps[1] - taps[2] + taps[3]);

        fdnDelays[0].push(diffused * 0.5f + h0 * kFeedback);
        fdnDelays[1].push(diffused * 0.5f + h1 * kFeedback);
        fdnDelays[2].push(-diffused * 0.5f + h2 * kFeedback);
        fdnDelays[3].push(-diffused * 0.5f + h3 * kFeedback);

        const float spaceL = 0.5f * (taps[0] + taps[2]);
        const float spaceR = 0.5f * (taps[1] + taps[3]);

        wetL[i] = sanitizeSample(wetL[i] + smoothedWetMix * spaceL);
        wetR[i] = sanitizeSample(wetR[i] + smoothedWetMix * spaceR);
    }
}

// ============================================================================
// HarmonyBusFx
// ============================================================================

void HarmonyBusFx::prepare(double sampleRate)
{
    deEsser.prepare(sampleRate);
    vocalSpace.prepare(sampleRate);
}

void HarmonyBusFx::reset() noexcept
{
    deEsser.reset();
    vocalSpace.reset();
}

void HarmonyBusFx::process(float* wetL, float* wetR, int numSamples, float deEssAmount, float spaceAmount) noexcept
{
    deEsser.process(wetL, wetR, numSamples, deEssAmount);
    vocalSpace.process(wetL, wetR, numSamples, spaceAmount);
}

} // namespace harmonizer::dsp
