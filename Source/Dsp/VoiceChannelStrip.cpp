#include "Dsp/VoiceChannelStrip.h"

#include <algorithm>
#include <cmath>

namespace harmonizer::dsp
{

void VoiceChannelStrip::prepare(double sampleRate, int voiceIndex)
{
    fs = std::max(8000.0, sampleRate);
    idx = voiceIndex;

    const int maxDelaySamp = static_cast<int>(std::ceil(fs * 0.060)); // Up to 60 ms
    delayLine.prepare(maxDelaySamp);

    paramSmoothAlpha = static_cast<float>(1.0 - std::exp(-1.0 / (0.018 * fs)));

    // Desynchronized slow LFO rates & initial phases per voice
    lfoRateHz = 0.31 + static_cast<double>(voiceIndex) * 0.13;
    reset();
}

void VoiceChannelStrip::reset() noexcept
{
    delayLine.reset();
    hpf.reset();
    lpf.reset();

    lfoPhase = std::fmod(static_cast<double>(idx) * 1.37, kTwoPi);
    lfoPhasePitch = std::fmod(static_cast<double>(idx) * 2.19 + 0.8, kTwoPi);

    smoothedGain = 0.0f;
    smoothedPan = 0.0f;
    smoothedDelaySamples = 1.0f;
    smoothedHpfHz = 80.0;
    smoothedLpfHz = 16000.0;

    hpf.setCutoff(smoothedHpfHz, fs);
    lpf.setCutoff(smoothedLpfHz, fs);
}

float VoiceChannelStrip::computeHumanizeCents(float humanizeAmount, int numSamples) noexcept
{
    const float h = std::clamp(humanizeAmount, 0.0f, 1.0f);
    if (h <= 0.001f)
        return 0.0f;

    const double phaseInc = kTwoPi * (lfoRateHz * 1.17) * static_cast<double>(numSamples) / fs;
    lfoPhasePitch = std::fmod(lfoPhasePitch + phaseInc, kTwoPi);
    return 6.0f * h * static_cast<float>(std::sin(lfoPhasePitch));
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

    const double lfoInc = kTwoPi * lfoRateHz / fs;
    const float minDelaySamples = 1.0f;
    const float maxDelaySamples = static_cast<float>(fs * 0.050);

    double rmsAcc = 0.0;

    for (int i = 0; i < numSamples; ++i)
    {
        smoothedGain += paramSmoothAlpha * (targetGain - smoothedGain);
        smoothedPan += paramSmoothAlpha * (targetPan - smoothedPan);
        smoothedHpfHz += static_cast<double>(paramSmoothAlpha) * (targetHpf - smoothedHpfHz);
        smoothedLpfHz += static_cast<double>(paramSmoothAlpha) * (targetLpf - smoothedLpfHz);

        // Update filter coeffs periodically (every 8 samples) for smooth sweeps & low CPU
        if ((i & 7) == 0)
        {
            hpf.setCutoff(smoothedHpfHz, fs);
            lpf.setCutoff(smoothedLpfHz, fs);
        }

        lfoPhase += lfoInc;
        if (lfoPhase >= kTwoPi)
            lfoPhase -= kTwoPi;

        const float modMs = h * 3.0f * (0.5f + 0.5f * static_cast<float>(std::sin(lfoPhase)));
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

        const float filtered = sanitizeSample(static_cast<float>(lpOut) * smoothedGain);
        rmsAcc += static_cast<double>(filtered) * static_cast<double>(filtered);

        // Equal-power stereo pan: [-1..+1] -> [0..pi/2]
        const float theta = (smoothedPan + 1.0f) * static_cast<float>(kPi * 0.25);
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
