#pragma once

#include "Dsp/DelayLine.h"
#include "Dsp/DspConstants.h"

namespace harmonizer::dsp
{

class VoiceChannelStrip
{
public:
    void prepare(double sampleRate, int voiceIndex);
    void reset() noexcept;

    // Advances the per-voice humanize LFO and returns micro-pitch modulation in cents (+/- 6ct max)
    float computeHumanizeCents(float humanizeAmount, int numSamples) noexcept;

    void processAndAccumulate(const float* monoIn,
                              float* wetOutL,
                              float* wetOutR,
                              int numSamples,
                              const VoiceParams& params,
                              bool voiceAudible,
                              float humanizeAmount,
                              float& outVoiceRms) noexcept;

private:
    struct SvfFilter
    {
        double ic1eq = 0.0;
        double ic2eq = 0.0;
        double g = 0.1;
        double k = std::sqrt(2.0);
        double a1 = 0.0, a2 = 0.0, a3 = 0.0;

        void reset() noexcept { ic1eq = ic2eq = 0.0; }

        void setCutoff(double cutoffHz, double sampleRate) noexcept
        {
            const double fc = std::clamp(cutoffHz, 20.0, sampleRate * 0.45);
            g = std::tan(kPi * fc / sampleRate);
            k = 1.41421356237; // Butterworth Q = 0.7071
            a1 = 1.0 / (1.0 + g * (g + k));
            a2 = g * a1;
            a3 = g * a2;
        }

        void processSample(double v0, double& outLp, double& outHp) noexcept
        {
            const double v3 = v0 - ic2eq;
            const double v1 = a1 * ic1eq + a2 * v3;
            const double v2 = ic2eq + a2 * ic1eq + a3 * v3;
            ic1eq = 2.0 * v1 - ic1eq;
            ic2eq = 2.0 * v2 - ic2eq;
            outLp = v2;
            outHp = v0 - k * v1 - v2;
        }
    };

    double fs = 48000.0;
    int idx = 0;

    DelayLine delayLine;
    SvfFilter hpf;
    SvfFilter lpf;

    double lfoPhase = 0.0;
    double lfoRateHz = 0.35;
    double lfoPhasePitch = 0.0;

    float smoothedGain = 0.0f;
    float smoothedPan = 0.0f;
    float smoothedDelaySamples = 0.0f;
    double smoothedHpfHz = 80.0;
    double smoothedLpfHz = 16000.0;
    float paramSmoothAlpha = 0.01f;
};

} // namespace harmonizer::dsp
