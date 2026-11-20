#pragma once

#include "Dsp/DelayLine.h"
#include "Dsp/DspConstants.h"

#include <cstdint>

namespace harmonizer::dsp
{

class VoiceChannelStrip
{
public:
    void prepare(double sampleRate, int voiceIndex);
    void reset() noexcept;

    // Advances organic non-periodic drift + staggered vibrato and returns pitch modulation in cents
    float computeHumanizeCents(float humanizeAmount,
                               float vibratoDepth,
                               float voicingConfidence,
                               int numSamples) noexcept;

    float computeHumanizeCents(float humanizeAmount, int numSamples) noexcept
    {
        return computeHumanizeCents(humanizeAmount, 0.0f, 1.0f, numSamples);
    }

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
        double k = 1.41421356237;
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

    float nextRandomWalkSample(float& state, float alpha) noexcept;

    double fs = 48000.0;
    int idx = 0;

    DelayLine delayLine;
    SvfFilter hpf;
    SvfFilter lpf;

    // Dual golden-ratio incommensurate phases + organic random walk state
    double phaseA = 0.0;
    double phaseB = 0.0;
    double phasePitchA = 0.0;
    double phasePitchB = 0.0;
    double baseRateHz = 0.29;
    double vibPhase = 0.0;
    double vibRateHz = 5.2;
    double panLfoPhase = 0.0;
    double panLfoRateHz = 0.25;

    uint32_t rngState = 0x12345678u;
    float walkPitch = 0.0f;
    float walkDelay = 0.0f;
    float walkGain = 0.0f;
    float walkAlphaBlock = 0.01f;
    float walkAlphaSample = 0.0001f;

    float smoothedGain = 0.0f;
    float smoothedPan = 0.0f;
    float smoothedDelaySamples = 1.0f;
    double smoothedHpfHz = 80.0;
    double smoothedLpfHz = 16000.0;
    float paramSmoothAlpha = 0.01f;
};

} // namespace harmonizer::dsp
