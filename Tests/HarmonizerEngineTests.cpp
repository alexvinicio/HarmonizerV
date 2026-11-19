#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>

#include "Dsp/HarmonizerEngine.h"

#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <vector>

using namespace harmonizer::dsp;

namespace
{
std::array<VoiceParams, kMaxVoices> makeFourActiveVoices()
{
    std::array<VoiceParams, kMaxVoices> vp{};
    vp[0] = { true, false, false, -2, -4.0f, -1.0f,  5.0f, 110.0f, 13000.0f, -0.5f, -6.0f };
    vp[1] = { true, false, false, +2, +4.0f, +0.5f,  8.0f, 130.0f, 14000.0f, +0.5f, -5.0f };
    vp[2] = { true, false, false, +4, -3.0f,  0.0f, 14.0f, 150.0f, 12500.0f, -0.8f, -7.5f };
    vp[3] = { true, false, false, +7, +3.0f, +2.0f, 19.0f, 220.0f, 15500.0f, +0.8f, -9.0f };
    return vp;
}
} // namespace

TEST_CASE("HarmonizerEngine processes variable sample rates, block sizes, and bus layouts cleanly", "[HarmonizerEngine]")
{
    const double rates[] = { 44100.0, 48000.0, 96000.0 };
    const int blockSizes[] = { 32, 64, 128, 256, 512, 1024 };

    GlobalParams gp;
    gp.engineMode = EngineMode::Live;
    gp.mix = 0.5f;
    gp.humanize = 0.25f;
    const auto vp = makeFourActiveVoices();

    for (double sr : rates)
    {
        for (int bs : blockSizes)
        {
            HarmonizerEngine engine;
            engine.prepare(sr, bs);

            std::vector<float> inL(static_cast<size_t>(bs), 0.0f);
            std::vector<float> inR(static_cast<size_t>(bs), 0.0f);
            std::vector<float> outL(static_cast<size_t>(bs), 0.0f);
            std::vector<float> outR(static_cast<size_t>(bs), 0.0f);

            for (int blk = 0; blk < 6; ++blk)
            {
                for (int i = 0; i < bs; ++i)
                {
                    const double t = static_cast<double>(blk * bs + i) / sr;
                    const float s = 0.25f * static_cast<float>(std::sin(kTwoPi * 220.0 * t));
                    inL[static_cast<size_t>(i)] = s;
                    inR[static_cast<size_t>(i)] = s;
                }

                // Mono -> Stereo
                const float* inPtr1[1] = { inL.data() };
                float* outPtr2[2] = { outL.data(), outR.data() };
                engine.processBlock(inPtr1, 1, outPtr2, 2, bs, gp, vp);

                for (int i = 0; i < bs; ++i)
                {
                    REQUIRE(std::isfinite(outL[static_cast<size_t>(i)]));
                    REQUIRE(std::isfinite(outR[static_cast<size_t>(i)]));
                }

                // Stereo -> Stereo
                const float* inPtr2[2] = { inL.data(), inR.data() };
                engine.processBlock(inPtr2, 2, outPtr2, 2, bs, gp, vp);

                // Mono -> Mono
                float* outPtr1[1] = { outL.data() };
                engine.processBlock(inPtr1, 1, outPtr1, 1, bs, gp, vp);
                for (float s : outL)
                    REQUIRE(std::isfinite(s));
            }
        }
    }
}

TEST_CASE("HarmonizerEngine dry path matches exact reported latency in both LIVE and STUDIO modes", "[HarmonizerEngine]")
{
    const double sr = 48000.0;
    const int bs = 128;

    HarmonizerEngine engine;
    engine.prepare(sr, bs);

    GlobalParams gp;
    gp.engineMode = EngineMode::Live;
    gp.mix = 0.0f; // Pure dry delay check
    std::array<VoiceParams, kMaxVoices> vp{};

    auto measureImpulsePeakIndex = [&](int expectedLat)
    {
        const int totalSamples = expectedLat + 1024;
        std::vector<float> in(static_cast<size_t>(totalSamples), 0.0f);
        std::vector<float> out(static_cast<size_t>(totalSamples), 0.0f);
        in[0] = 1.0f;

        for (int off = 0; off < totalSamples; off += bs)
        {
            const int chunk = std::min(bs, totalSamples - off);
            const float* inPtr[1] = { in.data() + off };
            float* outPtr[1] = { out.data() + off };
            engine.processBlock(inPtr, 1, outPtr, 1, chunk, gp, vp);
        }

        int peakIdx = -1;
        float peakVal = 0.0f;
        for (int i = 0; i < totalSamples; ++i)
        {
            if (std::abs(out[static_cast<size_t>(i)]) > peakVal)
            {
                peakVal = std::abs(out[static_cast<size_t>(i)]);
                peakIdx = i;
            }
        }
        REQUIRE(peakVal == Catch::Approx(1.0f).margin(0.01f));
        REQUIRE(peakIdx == expectedLat);
    };

    measureImpulsePeakIndex(engine.getLiveLatencySamples());

    // Switch to STUDIO mode and flush mode transition (~60 ms)
    gp.engineMode = EngineMode::Studio;
    std::vector<float> silIn(bs, 0.0f), silOut(bs, 0.0f);
    for (int blk = 0; blk < 30; ++blk)
    {
        const float* inPtr[1] = { silIn.data() };
        float* outPtr[1] = { silOut.data() };
        engine.processBlock(inPtr, 1, outPtr, 1, bs, gp, vp);
    }

    int reportedStudioLat = 0;
    REQUIRE(engine.consumeLatencyChanged(reportedStudioLat));
    REQUIRE(reportedStudioLat == engine.getStudioLatencySamples());
    REQUIRE(engine.getActiveMode() == EngineMode::Studio);

    WARN("Latency @ 48 kHz: LIVE=" << engine.getLiveLatencySamples()
         << " samples (" << (engine.getLiveLatencySamples() * 1000.0 / sr) << " ms), STUDIO="
         << engine.getStudioLatencySamples() << " samples ("
         << (engine.getStudioLatencySamples() * 1000.0 / sr) << " ms)");

    measureImpulsePeakIndex(engine.getStudioLatencySamples());
}

TEST_CASE("HarmonizerEngine handles reset and processBlock safely before prepare", "[HarmonizerEngine]")
{
    HarmonizerEngine uninitEngine;
    uninitEngine.reset();

    GlobalParams gp;
    std::array<VoiceParams, kMaxVoices> vp{};
    std::array<float, 64> in{};
    std::array<float, 64> outL{};
    std::array<float, 64> outR{};
    in.fill(0.5f);
    outL.fill(1.0f);
    outR.fill(1.0f);

    const float* inPtr[1] = { in.data() };
    float* outPtr[2] = { outL.data(), outR.data() };
    uninitEngine.processBlock(inPtr, 1, outPtr, 2, 64, gp, vp);

    for (int i = 0; i < 64; ++i)
    {
        REQUIRE(outL[static_cast<size_t>(i)] == 0.0f);
        REQUIRE(outR[static_cast<size_t>(i)] == 0.0f);
    }
}

TEST_CASE("HarmonizerEngine hot switches LIVE <-> STUDIO without clicks or NaNs", "[HarmonizerEngine]")
{
    const double sr = 48000.0;
    const int bs = 128;

    HarmonizerEngine engine;
    engine.prepare(sr, bs);

    GlobalParams gp;
    gp.engineMode = EngineMode::Live;
    gp.mix = 0.5f;
    const auto vp = makeFourActiveVoices();

    std::vector<float> in(bs, 0.0f), outL(bs, 0.0f), outR(bs, 0.0f);
    float prevOutL = 0.0f;
    float maxDeltaDuringSwitch = 0.0f;
    bool observedLatencyUpdate = false;

    for (int blk = 0; blk < 60; ++blk)
    {
        if (blk == 20)
            gp.engineMode = EngineMode::Studio;
        if (blk == 45)
            gp.engineMode = EngineMode::Live;

        for (int i = 0; i < bs; ++i)
        {
            const double t = static_cast<double>(blk * bs + i) / sr;
            in[static_cast<size_t>(i)] = 0.20f * static_cast<float>(std::sin(kTwoPi * 220.0 * t));
        }

        const float* inPtr[1] = { in.data() };
        float* outPtr[2] = { outL.data(), outR.data() };
        engine.processBlock(inPtr, 1, outPtr, 2, bs, gp, vp);

        int newLat = 0;
        if (engine.consumeLatencyChanged(newLat))
            observedLatencyUpdate = true;

        for (int i = 0; i < bs; ++i)
        {
            REQUIRE(std::isfinite(outL[static_cast<size_t>(i)]));
            REQUIRE(std::isfinite(outR[static_cast<size_t>(i)]));
            if (blk >= 20)
            {
                maxDeltaDuringSwitch = std::max(maxDeltaDuringSwitch, std::abs(outL[static_cast<size_t>(i)] - prevOutL));
            }
            prevOutL = outL[static_cast<size_t>(i)];
        }
    }

    REQUIRE(observedLatencyUpdate);
    REQUIRE(maxDeltaDuringSwitch < 0.45f);
}

TEST_CASE("HarmonizerEngine 4-voice LIVE mode CPU benchmark", "[HarmonizerEngine][benchmark]")
{
    const double sr = 48000.0;
    const int bs = 128;
    const int totalSamples = 96000; // 2.0 seconds of audio

    HarmonizerEngine engine;
    engine.prepare(sr, bs);

    GlobalParams gp;
    gp.engineMode = EngineMode::Live;
    gp.mix = 0.5f;
    gp.humanize = 0.25f;
    const auto vp = makeFourActiveVoices();

    std::vector<float> in(static_cast<size_t>(totalSamples), 0.0f);
    std::vector<float> outL(bs, 0.0f), outR(bs, 0.0f);
    for (int i = 0; i < totalSamples; ++i)
    {
        const double t = static_cast<double>(i) / sr;
        in[static_cast<size_t>(i)] = 0.25f * static_cast<float>(
            0.6 * std::sin(kTwoPi * 220.0 * t) + 0.3 * std::sin(kTwoPi * 440.0 * t));
    }

    const auto start = std::chrono::steady_clock::now();
    for (int off = 0; off < totalSamples; off += bs)
    {
        const float* inPtr[1] = { in.data() + off };
        float* outPtr[2] = { outL.data(), outR.data() };
        engine.processBlock(inPtr, 1, outPtr, 2, bs, gp, vp);
    }
    const auto elapsedMs = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - start).count();
    const double cpuPct = (elapsedMs / 2000.0) * 100.0;

    WARN("4-Voice LIVE mode processed 2000ms audio in " << elapsedMs << " ms (" << cpuPct << "% real-time CPU)");
#ifdef NDEBUG
    REQUIRE(elapsedMs < 500.0); // < 25% real-time in Release
#else
    REQUIRE(elapsedMs < 2000.0);
#endif
}

TEST_CASE("HarmonizerEngine STUDIO mode glides smoothly on melody step at 100% wet", "[HarmonizerEngine]")
{
    const double sr = 48000.0;
    const int bs = 128;

    HarmonizerEngine engine;
    engine.prepare(sr, bs);

    GlobalParams gp;
    gp.engineMode = EngineMode::Studio;
    gp.mix = 1.0f; // Pure wet harmonies
    gp.glideMs = 35.0f;
    gp.naturalness = 0.4f;
    gp.deEssAmount = 0.35f;
    gp.spaceAmount = 0.20f;
    auto vp = makeFourActiveVoices();

    std::vector<float> in(bs, 0.0f), outL(bs, 0.0f), outR(bs, 0.0f);
    float prevL = 0.0f;
    float maxJump = 0.0f;
    double wetEnergy = 0.0;

    // 1.2s total stepping C4 (261.63 Hz) -> D4 (293.66 Hz) -> E4 (329.63 Hz)
    const double freqs[3] = { 261.6256, 293.6648, 329.6276 };
    double phase = 0.0;
    for (int blk = 0; blk < 450; ++blk)
    {
        const double f0 = freqs[std::min(2, blk / 150)];
        for (int i = 0; i < bs; ++i)
        {
            phase += kTwoPi * f0 / sr;
            in[static_cast<size_t>(i)] = static_cast<float>(0.25 * std::sin(phase) + 0.12 * std::sin(2.0 * phase));
        }

        const float* inPtr[1] = { in.data() };
        float* outPtr[2] = { outL.data(), outR.data() };
        engine.processBlock(inPtr, 1, outPtr, 2, bs, gp, vp);

        for (int i = 0; i < bs; ++i)
        {
            REQUIRE(std::isfinite(outL[static_cast<size_t>(i)]));
            REQUIRE(std::isfinite(outR[static_cast<size_t>(i)]));
            if (blk > 60)
            {
                maxJump = std::max(maxJump, std::abs(outL[static_cast<size_t>(i)] - prevL));
                wetEnergy += static_cast<double>(outL[static_cast<size_t>(i)]) * outL[static_cast<size_t>(i)];
            }
            prevL = outL[static_cast<size_t>(i)];
        }
    }

    REQUIRE(wetEnergy > 1.0);
    REQUIRE(maxJump < 0.45f);
}

TEST_CASE("HarmonyDeEsser attenuates 7 kHz sibilance while preserving 300 Hz body", "[HarmonyBusFx]")
{
    const double sr = 48000.0;
    const int n = 4800;

    auto measureRmsAtFreq = [&](double freqHz, float deEssAmt)
    {
        HarmonyDeEsser ds;
        ds.prepare(sr);
        std::vector<float> bufL(static_cast<size_t>(n), 0.0f), bufR(static_cast<size_t>(n), 0.0f);
        for (int i = 0; i < n; ++i)
        {
            const float s = 0.35f * static_cast<float>(std::sin(kTwoPi * freqHz * (static_cast<double>(i) / sr)));
            bufL[static_cast<size_t>(i)] = s;
            bufR[static_cast<size_t>(i)] = s;
        }
        ds.process(bufL.data(), bufR.data(), n, deEssAmt);

        double acc = 0.0;
        for (int i = n / 2; i < n; ++i)
            acc += static_cast<double>(bufL[static_cast<size_t>(i)]) * bufL[static_cast<size_t>(i)];
        return std::sqrt(acc / static_cast<double>(n / 2));
    };

    const double bodyOff = measureRmsAtFreq(300.0, 0.0f);
    const double bodyOn  = measureRmsAtFreq(300.0, 1.0f);
    const double bodyLossDb = 20.0 * std::log10(bodyOff / std::max(1.0e-9, bodyOn));

    const double sibOff = measureRmsAtFreq(7000.0, 0.0f);
    const double sibOn  = measureRmsAtFreq(7000.0, 1.0f);
    const double sibReductionDb = 20.0 * std::log10(sibOff / std::max(1.0e-9, sibOn));

    INFO("300 Hz loss=" << bodyLossDb << " dB, 7 kHz reduction=" << sibReductionDb << " dB");
    REQUIRE(bodyLossDb < 0.5);
    REQUIRE(sibReductionDb >= 4.0);
}

TEST_CASE("VocalSpace generates smooth decaying stereo tail", "[HarmonyBusFx]")
{
    const double sr = 48000.0;
    VocalSpace space;
    space.prepare(sr);

    const int n = 48000; // 1 second
    std::vector<float> bufL(static_cast<size_t>(n), 0.0f), bufR(static_cast<size_t>(n), 0.0f);
    for (int i = 0; i < 400; ++i)
    {
        const float s = 0.4f * static_cast<float>(std::sin(kTwoPi * 440.0 * (static_cast<double>(i) / sr)));
        bufL[static_cast<size_t>(i)] = s;
        bufR[static_cast<size_t>(i)] = s;
    }

    space.process(bufL.data(), bufR.data(), n, 0.6f);

    double earlyTail = 0.0, lateTail = 0.0;
    for (int i = 2400; i < 7200; ++i)
        earlyTail += bufL[static_cast<size_t>(i)] * bufL[static_cast<size_t>(i)] + bufR[static_cast<size_t>(i)] * bufR[static_cast<size_t>(i)];
    for (int i = 36000; i < 48000; ++i)
        lateTail += bufL[static_cast<size_t>(i)] * bufL[static_cast<size_t>(i)] + bufR[static_cast<size_t>(i)] * bufR[static_cast<size_t>(i)];

    REQUIRE(earlyTail > 1.0e-4);
    REQUIRE(lateTail < earlyTail * 0.25);
    for (float s : bufL)
        REQUIRE(std::isfinite(s));
    for (float s : bufR)
        REQUIRE(std::isfinite(s));
}
