#include "Dsp/VoiceChannelStrip.h"

#include <algorithm>
#include <cmath>

namespace harmonizer::dsp
{

void VoiceChannelStrip::prepare(double sampleRate, int voiceIndex)
{
    fs = std::max(8000.0, sampleRate);
    idx = voiceIndex;

    const int maxDelaySamp = static_cast<int>(std::ceil(fs * 0.065)); // Up to 65 ms
    delayLine.prepare(maxDelaySamp);

    paramSmoothAlpha = static_cast<float>(1.0 - std::exp(-1.0 / (0.018 * fs)));
    walkAlphaSample = static_cast<float>(1.0 - std::exp(-kTwoPi * 0.35 / fs));
    walkAlphaBlock = static_cast<float>(1.0 - std::exp(-kTwoPi * 0.35 * 128.0 / fs));

    baseRateHz = 0.23 + static_cast<double>(voiceIndex) * 0.09;
    vibRateHz = 5.0 + static_cast<double>(voiceIndex) * 0.16;
    panLfoRateHz = 0.21 + static_cast<double>(voiceIndex) * 0.06;
    reset();
}

void VoiceChannelStrip::reset() noexcept
{
    delayLine.reset();
    hpf.reset();
    lpf.reset();

    phaseA = std::fmod(static_cast<double>(idx) * 1.37, kTwoPi);
    phaseB = std::fmod(static_cast<double>(idx) * 2.51 + 0.7, kTwoPi);
    phasePitchA = std::fmod(static_cast<double>(idx) * 2.19 + 0.8, kTwoPi);
    phasePitchB = std::fmod(static_cast<double>(idx) * 3.13 + 1.4, kTwoPi);
    vibPhase = std::fmod(static_cast<double>(idx) * 1.93 + 0.4, kTwoPi);
    panLfoPhase = std::fmod(static_cast<double>(idx) * 2.37 + 1.1, kTwoPi);

    rngState = 0x9E3779B9u + static_cast<uint32_t>(idx * 2654435761u);
    walkPitch = 0.0f;
    walkDelay = 0.0f;
    walkGain = 0.0f;

    smoothedGain = 0.0f;
    smoothedPan = 0.0f;
    smoothedDelaySamples = 1.0f;
    smoothedHpfHz = 80.0;
    smoothedLpfHz = 16000.0;

    hpf.setCutoff(smoothedHpfHz, fs);
    lpf.setCutoff(smoothedLpfHz, fs);
}

float VoiceChannelStrip::nextRandomWalkSample(float& state, float alpha) noexcept
{
    rngState = rngState * 1664525u + 1013904223u;
    const float white = (static_cast<float>(rngState >> 16) / 32768.0f) - 1.0f;
    state += alpha * (white - state);
    return std::clamp(state * 2.4f, -1.0f, 1.0f);
}

float VoiceChannelStrip::computeHumanizeCents(float humanizeAmount,
                                               float vibratoDepth,
                                               float voicingConfidence,
                                               int numSamples) noexcept
{
    const float h = std::clamp(humanizeAmount, 0.0f, 1.0f);
    const float vib = std::clamp(vibratoDepth, 0.0f, 1.0f);
    const float voiced = std::clamp(voicingConfidence, 0.0f, 1.0f);
    if (h <= 0.001f && vib <= 0.001f)
        return 0.0f;

    constexpr double kPhi = 1.61803398875;
    const double dt = static_cast<double>(numSamples) / fs;

    float humanizeCents = 0.0f;
    if (h > 0.001f)
    {
        phasePitchA = std::fmod(phasePitchA + kTwoPi * (baseRateHz * 1.13) * dt, kTwoPi);
        phasePitchB = std::fmod(phasePitchB + kTwoPi * (baseRateHz * 1.13 * kPhi) * dt, kTwoPi);

        const float osc = 0.55f * static_cast<float>(std::sin(phasePitchA))
                        + 0.45f * static_cast<float>(std::sin(phasePitchB));
        const float rnd = nextRandomWalkSample(walkPitch, walkAlphaBlock);
        humanizeCents = 7.0f * h * (0.65f * osc + 0.35f * rnd);
    }

    float vibCents = 0.0f;
    if (vib > 0.001f)
    {
        vibPhase = std::fmod(vibPhase + kTwoPi * vibRateHz * dt, kTwoPi);
        vibCents = 30.0f * vib * voiced * static_cast<float>(std::sin(vibPhase));
    }

    return humanizeCents + vibCents;
}

void VoiceChannelStrip::processAndAccumulate(const float* monoIn,
                                             float* wetOutL,
                                             float* wetOutR,
                                             int numSamples,
                                             const VoiceParams& params,
                                             bool voiceAudible,
                                             float humanizeAmount,
                                             float& outVoiceRms) noexcept
{
    const float targetGain = voiceAudible ? dbToGain(params.gainDb) : 0.0f;
    const float targetPan = std::clamp(params.pan, -1.0f, 1.0f);
    const float h = std::clamp(humanizeAmount, 0.0f, 1.0f);

    const float baseDelayMs = std::clamp(params.delayMs, 0.0f, 40.0f);
    const double targetHpf = std::clamp(static_cast<double>(params.hpfHz), 40.0, 1200.0);
    const double targetLpf = std::clamp(static_cast<double>(params.lpfHz), 1500.0, 19500.0);

    constexpr double kPhi = 1.61803398875;
    const double incA = kTwoPi * baseRateHz / fs;
    const double incB = kTwoPi * (baseRateHz * kPhi) / fs;
    const double incPan = kTwoPi * panLfoRateHz / fs;
    const float minDelaySamples = 1.0f;
    const float maxDelaySamples = static_cast<float>(fs * 0.055);

    double rmsAcc = 0.0;

    for (int i = 0; i < numSamples; ++i)
    {
        smoothedGain += paramSmoothAlpha * (targetGain - smoothedGain);
        smoothedPan += paramSmoothAlpha * (targetPan - smoothedPan);
        smoothedHpfHz += static_cast<double>(paramSmoothAlpha) * (targetHpf - smoothedHpfHz);
        smoothedLpfHz += static_cast<double>(paramSmoothAlpha) * (targetLpf - smoothedLpfHz);

        if ((i & 7) == 0)
        {
            hpf.setCutoff(smoothedHpfHz, fs);
            lpf.setCutoff(smoothedLpfHz, fs);
        }

        phaseA += incA;
        if (phaseA >= kTwoPi)
            phaseA -= kTwoPi;
        phaseB += incB;
        if (phaseB >= kTwoPi)
            phaseB -= kTwoPi;
        panLfoPhase += incPan;
        if (panLfoPhase >= kTwoPi)
            panLfoPhase -= kTwoPi;

        const float osc = 0.5f + 0.28f * static_cast<float>(std::sin(phaseA))
                               + 0.22f * static_cast<float>(std::sin(phaseB));
        const float rndDelay = 0.5f + 0.5f * nextRandomWalkSample(walkDelay, walkAlphaSample);
        const float modMs = h * 4.5f * (0.65f * osc + 0.35f * rndDelay);

        const float targetDelaySamples = std::clamp(
            minDelaySamples + (baseDelayMs + modMs) * static_cast<float>(fs * 0.001),
            minDelaySamples,
            maxDelaySamples);

        smoothedDelaySamples += paramSmoothAlpha * (targetDelaySamples - smoothedDelaySamples);

        delayLine.push(monoIn[i]);
        const float delayed = delayLine.readCubic(smoothedDelaySamples);

        double dummyLp = 0.0;
        double hpOut = 0.0;
        hpf.processSample(static_cast<double>(delayed), dummyLp, hpOut);

        double lpOut = 0.0;
        double dummyHp = 0.0;
        lpf.processSample(hpOut, lpOut, dummyHp);

        // Subtle organic breath (+/- 10% gain drift when humanize > 0)
        const float breathGain = 1.0f + 0.10f * h * nextRandomWalkSample(walkGain, walkAlphaSample);
        const float filtered = sanitizeSample(static_cast<float>(lpOut) * smoothedGain * breathGain);
        rmsAcc += static_cast<double>(filtered) * static_cast<double>(filtered);

        // Equal-power stereo pan + subtle organic stereo drift: [-1..+1] -> [0..pi/2]
        const float panMotion = 0.18f * h * static_cast<float>(std::sin(panLfoPhase));
        const float effectivePan = std::clamp(smoothedPan + panMotion, -1.0f, 1.0f);
        const float theta = (effectivePan + 1.0f) * static_cast<float>(kPi * 0.25);
        const float gL = std::cos(theta);
        const float gR = std::sin(theta);

        wetOutL[i] += filtered * gL;
        wetOutR[i] += filtered * gR;
    }

    outVoiceRms = (numSamples > 0)
        ? static_cast<float>(std::sqrt(rmsAcc / static_cast<double>(numSamples)))
        : 0.0f;
}

} // namespace harmonizer::dsp
