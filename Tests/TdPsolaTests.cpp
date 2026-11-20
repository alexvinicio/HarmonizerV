#include <catch2/catch_test_macros.hpp>

#include "Dsp/PitchMarkDetector.h"
#include "Dsp/PitchTracker.h"
#include "Dsp/TdPsolaVoice.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <vector>

namespace
{
std::vector<float> makeRichVocalWave(double f0Hz, double fs, int numSamples)
{
    std::vector<float> out(static_cast<size_t>(numSamples), 0.0f);
    for (int i = 0; i < numSamples; ++i)
    {
        const double t = static_cast<double>(i) / fs;
        double sum = 0.0;
        for (int h = 1; h <= 12; ++h)
        {
            const double freq = h * f0Hz;
            if (freq >= fs * 0.42)
                break;
            // Broad spectral resonance around 1000 Hz to test formant shifts clearly
            const double env = 0.5 / (1.0 + std::pow((freq - 1000.0) / 500.0, 2.0)) + 0.3 / h;
            sum += env * std::sin(harmonizer::dsp::kTwoPi * freq * t);
        }
        out[static_cast<size_t>(i)] = static_cast<float>(sum * 0.25);
    }
    return out;
}

std::vector<float> runTdPsola(const std::vector<float>& input, double fs, float shiftSt, float formantSt, int& outLatency, float naturalness = 0.35f, harmonizer::dsp::LiveBufferSize bufSize = harmonizer::dsp::LiveBufferSize::Fast256)
{
    harmonizer::dsp::PitchTracker tracker;
    harmonizer::dsp::PitchMarkDetector marks;
    harmonizer::dsp::TdPsolaVoice voice;

    tracker.prepare(fs);
    marks.prepare(fs);
    voice.prepare(fs, 0, bufSize);
    outLatency = voice.getLatencySamples();

    std::vector<float> output(input.size(), 0.0f);
    for (size_t i = 0; i < input.size(); ++i)
    {
        tracker.pushSample(input[i]);
        const auto est = tracker.getEstimate();
        const float f0 = tracker.getInstantHz();
        const int64_t pos = marks.getWritePos();
        marks.pushSample(input[i], f0, est.isVoiced);
        output[i] = voice.processSample(marks, pos, f0, est.voicingConfidence, shiftSt, formantSt, 24.0f, naturalness);
    }
    return output;
}

double findStrongestHarmonicPeakHz(const std::vector<float>& sig, int startIdx, int len, double fs, double expectedF0)
{
    // Search closely around expected fundamental [0.75 * expectedF0, 1.35 * expectedF0] with fine DFT grid
    const double minF = std::max(50.0, expectedF0 * 0.75);
    const double maxF = std::min(fs * 0.4, expectedF0 * 1.35);
    double bestFreq = expectedF0;
    double bestMag = -1.0;

    const int steps = 400;
    for (int s = 0; s <= steps; ++s)
    {
        const double f = minF + (maxF - minF) * (static_cast<double>(s) / steps);
        double re = 0.0, im = 0.0;
        const double wStep = harmonizer::dsp::kTwoPi * f / fs;
        for (int n = 0; n < len; ++n)
        {
            const double win = 0.5 * (1.0 - std::cos(harmonizer::dsp::kTwoPi * n / (len - 1)));
            const double x = static_cast<double>(sig[static_cast<size_t>(startIdx + n)]) * win;
            re += x * std::cos(wStep * n);
            im -= x * std::sin(wStep * n);
        }
        const double mag = re * re + im * im;
        if (mag > bestMag)
        {
            bestMag = mag;
            bestFreq = f;
        }
    }
    return bestFreq;
}

double computeSpectralCentroidHz(const std::vector<float>& sig, int startIdx, int len, double fs)
{
    double weightedSum = 0.0;
    double magSum = 0.0;
    const double minF = 150.0;
    const double maxF = 4500.0;
    const int bins = 120;

    for (int b = 0; b <= bins; ++b)
    {
        const double f = minF + (maxF - minF) * (static_cast<double>(b) / bins);
        double re = 0.0, im = 0.0;
        const double wStep = harmonizer::dsp::kTwoPi * f / fs;
        for (int n = 0; n < len; ++n)
        {
            const double win = 0.5 * (1.0 - std::cos(harmonizer::dsp::kTwoPi * n / (len - 1)));
            const double x = static_cast<double>(sig[static_cast<size_t>(startIdx + n)]) * win;
            re += x * std::cos(wStep * n);
            im -= x * std::sin(wStep * n);
        }
        const double mag = std::sqrt(re * re + im * im);
        weightedSum += f * mag;
        magSum += mag;
    }
    return (magSum > 1.0e-9) ? (weightedSum / magSum) : 0.0;
}
} // namespace

TEST_CASE("TdPsolaVoice latency <= 12ms and pitch shifting accuracy across intervals", "[TdPsolaVoice]")
{
    const double fs = 48000.0;
    const double f0 = 220.0;
    const auto input = makeRichVocalWave(f0, fs, 28800); // 600 ms

    const float shifts[] = { +4.0f, +7.0f, -5.0f, +12.0f, -12.0f };
    for (float st : shifts)
    {
        int latency = 0;
        const auto out = runTdPsola(input, fs, st, 0.0f, latency);

        REQUIRE(latency <= 576); // <= 12.0 ms at 48 kHz

        for (float s : out)
        {
            REQUIRE(std::isfinite(s));
            REQUIRE(std::abs(s) < 2.0f);
        }

        const double expectedHz = f0 * std::pow(2.0, st / 12.0);
        const double measuredHz = findStrongestHarmonicPeakHz(out, 12000, 8192, fs, expectedHz);
        const double errCents = 1200.0 * std::log2(measuredHz / expectedHz);

        INFO("Shift=" << st << " st, expected=" << expectedHz << " Hz, measured=" << measuredHz << " Hz, err=" << errCents << " cents");
        REQUIRE(std::abs(errCents) <= 25.0);
    }
}

TEST_CASE("TdPsolaVoice formant shift moves spectral envelope independently", "[TdPsolaVoice]")
{
    const double fs = 48000.0;
    const auto input = makeRichVocalWave(220.0, fs, 24000);

    int lat = 0;
    const auto outDown = runTdPsola(input, fs, +4.0f, -6.0f, lat);
    const auto outUp   = runTdPsola(input, fs, +4.0f, +6.0f, lat);

    const double centroidDown = computeSpectralCentroidHz(outDown, 10000, 4096, fs);
    const double centroidUp   = computeSpectralCentroidHz(outUp, 10000, 4096, fs);

    INFO("Centroid (-6st formant)=" << centroidDown << " Hz, (+6st formant)=" << centroidUp << " Hz");
    REQUIRE(centroidUp > centroidDown * 1.15);
}

TEST_CASE("TdPsolaVoice handles unvoiced noise cleanly without NaNs or clipping", "[TdPsolaVoice]")
{
    const double fs = 48000.0;
    std::vector<float> noise(16384, 0.0f);
    uint32_t rng = 0xCAFEBABEu;
    for (float& s : noise)
    {
        rng = rng * 1664525u + 1013904223u;
        s = (static_cast<float>(rng & 0xFFFFu) / 32768.0f - 1.0f) * 0.3f;
    }

    int lat = 0;
    const auto out = runTdPsola(noise, fs, +7.0f, +2.0f, lat, 0.5f);
    for (float s : out)
    {
        REQUIRE(std::isfinite(s));
        REQUIRE(std::abs(s) < 2.0f);
    }
}

TEST_CASE("TdPsolaVoice band-split preserves stability with vibrato and breath", "[TdPsolaVoice]")
{
    const double fs = 48000.0;
    const int numSamples = 24000;
    std::vector<float> vocal(static_cast<size_t>(numSamples), 0.0f);
    uint32_t rng = 0x11223344u;
    double phase = 0.0;
    for (int i = 0; i < numSamples; ++i)
    {
        const double t = static_cast<double>(i) / fs;
        const double f0 = 220.0 * (1.0 + 0.015 * std::sin(harmonizer::dsp::kTwoPi * 5.2 * t));
        phase += harmonizer::dsp::kTwoPi * f0 / fs;
        rng = rng * 1664525u + 1013904223u;
        const float breath = (static_cast<float>(rng & 0xFFFFu) / 32768.0f - 1.0f) * 0.02f;
        vocal[static_cast<size_t>(i)] = static_cast<float>(0.35 * std::sin(phase) + 0.18 * std::sin(2.0 * phase) + 0.08 * std::sin(3.0 * phase)) + breath;
    }

    int lat = 0;
    const auto out = runTdPsola(vocal, fs, +4.0f, 0.0f, lat, 0.65f);
    for (float s : out)
    {
        REQUIRE(std::isfinite(s));
        REQUIRE(std::abs(s) < 2.0f);
    }
}

TEST_CASE("TdPsolaVoice supports all LiveBufferSize latency options cleanly", "[TdPsolaVoice][LiveBuffer]")
{
    using harmonizer::dsp::LiveBufferSize;
    const double fs = 48000.0;
    const double f0 = 220.0;
    const auto input = makeRichVocalWave(f0, fs, 28800);

    struct BufferCase
    {
        LiveBufferSize size;
        int expectedLatency;
        double maxErrCents;
    };

    const BufferCase cases[] = {
        { LiveBufferSize::Ultra128, 128, 40.0 },
        { LiveBufferSize::Fast256,  256, 25.0 },
        { LiveBufferSize::Low384,   384, 25.0 },
        { LiveBufferSize::Safe512,  512, 25.0 },
    };

    const float shifts[] = { +4.0f, +7.0f, -5.0f, +12.0f, -12.0f };
    for (const auto& bc : cases)
    {
        for (float st : shifts)
        {
            int latency = 0;
            const auto out = runTdPsola(input, fs, st, 0.0f, latency, 0.35f, bc.size);
            REQUIRE(latency == bc.expectedLatency);

            for (float s : out)
            {
                REQUIRE(std::isfinite(s));
                REQUIRE(std::abs(s) < 2.0f);
            }

            const double expectedHz = f0 * std::pow(2.0, st / 12.0);
            const double measuredHz = findStrongestHarmonicPeakHz(out, 12000, 8192, fs, expectedHz);
            const double errCents = 1200.0 * std::log2(measuredHz / expectedHz);
            INFO("Buffer=" << bc.expectedLatency << " Shift=" << st << "st err=" << errCents << " cents");
            REQUIRE(std::abs(errCents) <= bc.maxErrCents);
        }
    }
}

TEST_CASE("TdPsolaVoice low vocals (< 120 Hz) in Ultra128 and Fast256 preserve pitch, formants, and continuity", "[TdPsolaVoice][LowVocal]")
{
    using harmonizer::dsp::LiveBufferSize;
    const double fs = 48000.0;

    // 1) Low 90 Hz voice pitch accuracy in Ultra128 and Fast256
    {
        const double f0 = 90.0;
        const auto input = makeRichVocalWave(f0, fs, 32000);
        const struct { LiveBufferSize size; int lat; } configs[] = {
            { LiveBufferSize::Ultra128, 128 },
            { LiveBufferSize::Fast256,  256 }
        };
        const float shifts[] = { +4.0f, +7.0f, -5.0f };

        for (const auto& cfg : configs)
        {
            for (float st : shifts)
            {
                int lat = 0;
                const auto out = runTdPsola(input, fs, st, 0.0f, lat, 0.35f, cfg.size);
                REQUIRE(lat == cfg.lat);
                for (float s : out)
                {
                    REQUIRE(std::isfinite(s));
                    REQUIRE(std::abs(s) < 2.0f);
                }

                const double expectedHz = f0 * std::pow(2.0, st / 12.0);
                const double measuredHz = findStrongestHarmonicPeakHz(out, 12000, 12000, fs, expectedHz);
                const double errCents = 1200.0 * std::log2(measuredHz / expectedHz);
                INFO("LowF0=90Hz Buffer=" << cfg.lat << " Shift=" << st << "st err=" << errCents << " cents");
                REQUIRE(std::abs(errCents) <= 25.0);
            }
        }
    }

    // 2) Formant independence on low 95 Hz voice in Fast256
    {
        const auto input = makeRichVocalWave(95.0, fs, 28800);
        int lat = 0;
        const auto outDown = runTdPsola(input, fs, +4.0f, -6.0f, lat, 0.35f, LiveBufferSize::Fast256);
        const auto outUp   = runTdPsola(input, fs, +4.0f, +6.0f, lat, 0.35f, LiveBufferSize::Fast256);

        const double centroidDown = computeSpectralCentroidHz(outDown, 12000, 4096, fs);
        const double centroidUp   = computeSpectralCentroidHz(outUp, 12000, 4096, fs);
        INFO("Low95Hz Centroid(-6st)=" << centroidDown << " Hz, (+6st)=" << centroidUp << " Hz");
        REQUIRE(centroidUp > centroidDown * 1.15);
    }

    // 3) Continuity on 85 Hz voice (-5 st) in Ultra128 (zero silent grain truncation gaps)
    {
        const auto input = makeRichVocalWave(85.0, fs, 28800);
        int lat = 0;
        const auto out = runTdPsola(input, fs, -5.0f, 0.0f, lat, 0.35f, LiveBufferSize::Ultra128);

        double minWinRms = 1.0e9;
        double sumWinRms = 0.0;
        int numWins = 0;
        for (int start = 12000; start + 256 <= 24000; start += 256)
        {
            double acc = 0.0;
            for (int n = 0; n < 256; ++n)
            {
                const double v = out[static_cast<size_t>(start + n)];
                acc += v * v;
            }
            const double rms = std::sqrt(acc / 256.0);
            minWinRms = std::min(minWinRms, rms);
            sumWinRms += rms;
            ++numWins;
        }
        const double avgWinRms = sumWinRms / std::max(1, numWins);
        INFO("Low85Hz Ultra128 minWinRms=" << minWinRms << " avgWinRms=" << avgWinRms);
        REQUIRE(avgWinRms > 0.01);
        REQUIRE(minWinRms > 0.25 * avgWinRms);
    }
}
