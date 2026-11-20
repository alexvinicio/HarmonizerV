#pragma once

#include "Dsp/DelayLine.h"
#include "Dsp/DspConstants.h"

#include <array>

namespace harmonizer::dsp
{

class HarmonyDeEsser
{
public:
    void prepare(double sampleRate);
    void reset() noexcept;

    void process(float* wetL, float* wetR, int numSamples, float deEssAmount) noexcept;
    float getLastReductionDb() const noexcept { return lastReductionDb; }

private:
    struct SvfBand
    {
        double ic1eq = 0.0;
        double ic2eq = 0.0;
        double g = 0.1;
        double k = 1.0;
        double a1 = 0.0, a2 = 0.0, a3 = 0.0;

        void reset() noexcept { ic1eq = ic2eq = 0.0; }

        void setParams(double freqHz, double q, double sampleRate) noexcept
        {
            const double fc = std::clamp(freqHz, 100.0, sampleRate * 0.42);
            g = std::tan(kPi * fc / sampleRate);
            k = 1.0 / std::max(0.25, q);
            a1 = 1.0 / (1.0 + g * (g + k));
            a2 = g * a1;
            a3 = g * a2;
        }

        void process(double v0, double& lp, double& bp, double& hp) noexcept
        {
            const double v3 = v0 - ic2eq;
            const double v1 = a1 * ic1eq + a2 * v3;
            const double v2 = ic2eq + a2 * ic1eq + a3 * v3;
            ic1eq = 2.0 * v1 - ic1eq;
            ic2eq = 2.0 * v2 - ic2eq;
            lp = v2;
            bp = v1;
            hp = v0 - k * v1 - v2;
        }
    };

    double fs = 48000.0;
    SvfBand scBand;
    double splitLpL = 0.0;
    double splitLpR = 0.0;
    double splitAlpha = 0.4;

    float envSib = 0.0f;
    float envBroad = 0.0f;
    float attackAlpha = 0.05f;
    float releaseAlpha = 0.005f;
    float broadAlpha = 0.01f;
    float smoothedGain = 1.0f;
    float lastReductionDb = 0.0f;
};

class VocalSpace
{
public:
    void prepare(double sampleRate);
    void reset() noexcept;

    void process(float* wetL, float* wetR, int numSamples, float spaceAmount) noexcept;

private:
    struct AllpassDiffuser
    {
        DelayLine delay;
        int delaySamples = 128;
        float coeff = 0.5f;

        void prepare(double sampleRate, double delayMs, float g)
        {
            delaySamples = std::max(8, static_cast<int>(std::llround(sampleRate * delayMs * 0.001)));
            coeff = g;
            delay.prepare(delaySamples + 16);
        }

        void reset() noexcept { delay.reset(); }

        float process(float x) noexcept
        {
            const float d = delay.readInt(delaySamples);
            const float v = x - coeff * d;
            delay.push(v);
            return d + coeff * v;
        }
    };

    double fs = 48000.0;

    // 1-pole input conditioning HPF (~180 Hz) and LPF (~6.5 kHz)
    float inHpState = 0.0f;
    float inHpAlpha = 0.02f;
    float inLpState = 0.0f;
    float inLpAlpha = 0.5f;

    AllpassDiffuser diffA;
    AllpassDiffuser diffB;

    static constexpr int kNumFdnLines = 4;
    std::array<DelayLine, kNumFdnLines> fdnDelays;
    std::array<float, kNumFdnLines> fdnBaseSamples{};
    std::array<float, kNumFdnLines> fdnLpState{};
    std::array<double, kNumFdnLines> modPhase{};
    std::array<double, kNumFdnLines> modInc{};

    float dampAlpha = 0.5f;
    float smoothedWetMix = 0.0f;
    float mixAlpha = 0.01f;
};

class HarmonyBusFx
{
public:
    void prepare(double sampleRate);
    void reset() noexcept;

    void process(float* wetL, float* wetR, int numSamples, float deEssAmount, float spaceAmount) noexcept;
    float getDeEssReductionDb() const noexcept { return deEsser.getLastReductionDb(); }

private:
    HarmonyDeEsser deEsser;
    VocalSpace vocalSpace;
};

} // namespace harmonizer::dsp
