#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include "Dsp/PitchTracker.h"

#include <cmath>
#include <cstdint>
#include <vector>

namespace
{
std::vector<float> makeVocalTone(double f0Hz, double sampleRate, int numSamples)
{
    std::vector<float> out(static_cast<size_t>(numSamples), 0.0f);
    // Fundamental + 5 harmonics with gentle spectral tilt
    constexpr double amps[6] = { 0.45, 0.28, 0.16, 0.09, 0.05, 0.02 };
    for (int i = 0; i < numSamples; ++i)
    {
        double sum = 0.0;
        const double t = static_cast<double>(i) / sampleRate;
        for (int h = 1; h <= 6; ++h)
        {
            if (h * f0Hz < sampleRate * 0.45)
                sum += amps[h - 1] * std::sin(harmonizer::dsp::kTwoPi * (h * f0Hz) * t);
        }
        out[static_cast<size_t>(i)] = static_cast<float>(sum);
    }
    return out;
}

double centsError(double measuredHz, double expectedHz)
{
    return 1200.0 * std::log2(measuredHz / expectedHz);
}
} // namespace

TEST_CASE("PitchTracker detects vocal fundamentals accurately across sample rates", "[PitchTracker]")
{
    const double sampleRates[] = { 44100.0, 48000.0, 96000.0 };
    const double testFreqs[]   = { 85.0, 110.0, 220.0, 440.0, 660.0, 880.0 };

    for (double sr : sampleRates)
    {
        for (double f0 : testFreqs)
        {
            harmonizer::dsp::PitchTracker tracker;
            tracker.prepare(sr);

            const int numSamples = static_cast<int>(sr * 0.20);
            const auto signal = makeVocalTone(f0, sr, numSamples);

            tracker.processBlock(signal.data(), numSamples);
            const auto est = tracker.getEstimate();

            INFO("SR=" << sr << " F0=" << f0 << " detected=" << est.frequencyHz << " clarity=" << est.clarity);
            REQUIRE(est.isVoiced);
            REQUIRE(std::abs(centsError(est.frequencyHz, f0)) <= 5.0);
        }
    }
}

TEST_CASE("PitchTracker rejects white noise and high-frequency sibilance", "[PitchTracker]")
{
    harmonizer::dsp::PitchTracker tracker;
    const double sr = 48000.0;
    tracker.prepare(sr);

    const int numSamples = 16384;
    std::vector<float> noise(static_cast<size_t>(numSamples), 0.0f);
    uint32_t rng = 0x12345678u;
    float prev = 0.0f;
    for (int i = 0; i < numSamples; ++i)
    {
        rng = rng * 1664525u + 1013904223u;
        const float white = (static_cast<float>(rng & 0xFFFFu) / 32768.0f - 1.0f) * 0.25f;
        // High-pass differentiate to mimic /s/ sibilant energy
        noise[static_cast<size_t>(i)] = white - 0.85f * prev;
        prev = white;
    }

    tracker.processBlock(noise.data(), numSamples);
    const auto est = tracker.getEstimate();
    REQUIRE_FALSE(est.isVoiced);
}
