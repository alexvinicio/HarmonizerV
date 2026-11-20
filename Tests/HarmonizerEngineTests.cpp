#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>

#include "Dsp/HarmonizerEngine.h"
#include "Dsp/VoicingPresets.h"

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
    vp[0] = { true, false, false, false, 60, -2, -4.0f, -1.0f,  5.0f, 110.0f, 13000.0f, -0.5f, -6.0f, 0.0f };
    vp[1] = { true, false, false, false, 60, +2, +4.0f, +0.5f,  8.0f, 130.0f, 14000.0f, +0.5f, -5.0f, 0.0f };
    vp[2] = { true, false, false, false, 60, +4, -3.0f,  0.0f, 14.0f, 150.0f, 12500.0f, -0.8f, -7.5f, 0.0f };
    vp[3] = { true, false, false, false, 60, +7, +3.0f, +2.0f, 19.0f, 220.0f, 15500.0f, +0.8f, -9.0f, 0.0f };
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
        in[0] = 0.5f;

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
        REQUIRE(peakVal == Catch::Approx(0.5f).margin(0.01f));
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

TEST_CASE("HarmonyDeEsser keeps flat passband at 1.5 kHz and 4.8 kHz without notch distortion", "[HarmonyBusFx]")
{
    const double sr = 48000.0;
    const int n = 4800;

    auto measureRmsAt = [&](double freqHz, float amp, float deEssAmt)
    {
        HarmonyDeEsser ds;
        ds.prepare(sr);
        std::vector<float> bufL(static_cast<size_t>(n), 0.0f), bufR(static_cast<size_t>(n), 0.0f);
        for (int i = 0; i < n; ++i)
        {
            const float s = amp * static_cast<float>(std::sin(kTwoPi * freqHz * (static_cast<double>(i) / sr)));
            bufL[static_cast<size_t>(i)] = s;
            bufR[static_cast<size_t>(i)] = s;
        }
        ds.process(bufL.data(), bufR.data(), n, deEssAmt);

        double acc = 0.0;
        for (int i = n / 2; i < n; ++i)
            acc += static_cast<double>(bufL[static_cast<size_t>(i)]) * bufL[static_cast<size_t>(i)];
        return std::sqrt(acc / static_cast<double>(n / 2));
    };

    for (double f : { 300.0, 1500.0, 4800.0 })
    {
        // Moderate vocal energy below sibilance threshold must pass flat (< 0.5 dB loss)
        const double off = measureRmsAt(f, 0.02f, 0.0f);
        const double on  = measureRmsAt(f, 0.02f, 1.0f);
        const double lossDb = 20.0 * std::log10(off / std::max(1.0e-9, on));
        INFO("Freq=" << f << " Hz loss=" << lossDb << " dB");
        REQUIRE(std::abs(lossDb) < 0.5);
    }
}

TEST_CASE("HarmonizerEngine hot -3 dBFS 4-voice Mix 100% stays below 0 dBFS without hard clipping", "[HarmonizerEngine]")
{
    const double sr = 48000.0;
    const int bs = 128;
    const auto vp = makeFourActiveVoices();

    for (EngineMode mode : { EngineMode::Live, EngineMode::Studio })
    {
        HarmonizerEngine engine;
        engine.prepare(sr, bs);

        GlobalParams gp;
        gp.engineMode = mode;
        gp.mix = 1.0f;
        gp.deEssAmount = 0.35f;
        gp.spaceAmount = 0.20f;

        std::vector<float> in(bs, 0.0f), outL(bs, 0.0f), outR(bs, 0.0f);
        float maxPeak = 0.0f;
        double rmsAcc = 0.0;

        for (int blk = 0; blk < 180; ++blk)
        {
            for (int i = 0; i < bs; ++i)
            {
                const double t = static_cast<double>(blk * bs + i) / sr;
                in[static_cast<size_t>(i)] = static_cast<float>(
                    0.45 * std::sin(kTwoPi * 220.0 * t) + 0.25 * std::sin(kTwoPi * 440.0 * t)); // ~ -3 dBFS
            }
            const float* inPtr[1] = { in.data() };
            float* outPtr[2] = { outL.data(), outR.data() };
            engine.processBlock(inPtr, 1, outPtr, 2, bs, gp, vp);

            for (int i = 0; i < bs; ++i)
            {
                REQUIRE(std::isfinite(outL[static_cast<size_t>(i)]));
                REQUIRE(std::isfinite(outR[static_cast<size_t>(i)]));
                maxPeak = std::max(maxPeak, std::max(std::abs(outL[static_cast<size_t>(i)]), std::abs(outR[static_cast<size_t>(i)])));
                if (blk > 90)
                    rmsAcc += static_cast<double>(outL[static_cast<size_t>(i)]) * outL[static_cast<size_t>(i)];
            }
        }

        INFO("Mode=" << static_cast<int>(mode) << " maxPeak=" << maxPeak << " rmsAcc=" << rmsAcc);
        REQUIRE(maxPeak <= 0.99f);
        REQUIRE(rmsAcc > 0.5);
    }
}

TEST_CASE("HarmonizerEngine input pitch correction pulls flat A3 toward 220 Hz", "[HarmonizerEngine][Tune]")
{
    const double sr = 48000.0;
    const int bs = 128;

    HarmonizerEngine engine;
    engine.prepare(sr, bs);

    GlobalParams gp;
    gp.key = 0;        // C Major (contains A3 = 220 Hz)
    gp.scaleIndex = 0; // Major
    gp.engineMode = EngineMode::Live;
    gp.mix = 0.0f;     // Listen to corrected Lead only
    gp.inputTuneAmount = 1.0f;
    std::array<VoiceParams, kMaxVoices> vp{};

    const double flatA3Hz = 220.0 * std::pow(2.0, -35.0 / 1200.0); // ~215.60 Hz (-35 cents)
    const int totalSamples = 188 * bs; // 24064 samples (~501 ms, exact multiple of bs)
    std::vector<float> out(static_cast<size_t>(totalSamples), 0.0f);
    std::vector<float> in(bs, 0.0f), outL(bs, 0.0f), outR(bs, 0.0f);

    for (int off = 0; off < totalSamples; off += bs)
    {
        const int chunk = std::min(bs, totalSamples - off);
        for (int i = 0; i < chunk; ++i)
        {
            const double t = static_cast<double>(off + i) / sr;
            in[static_cast<size_t>(i)] = static_cast<float>(0.30 * std::sin(kTwoPi * flatA3Hz * t) + 0.14 * std::sin(kTwoPi * 2.0 * flatA3Hz * t));
        }
        const float* inPtr[1] = { in.data() };
        float* outPtr[2] = { outL.data(), outR.data() };
        engine.processBlock(inPtr, 1, outPtr, 2, chunk, gp, vp);
        for (int i = 0; i < chunk; ++i)
            out[static_cast<size_t>(off + i)] = outL[static_cast<size_t>(i)];
    }

    // Measure peak frequency on the last 8192 samples via fine DFT around 220 Hz
    const int startIdx = 14000;
    const int len = 8192;
    double bestFreq = 215.6;
    double bestMag = -1.0;
    for (int step = 0; step <= 240; ++step)
    {
        const double f = 205.0 + 30.0 * (static_cast<double>(step) / 240.0);
        double re = 0.0, im = 0.0;
        const double wStep = kTwoPi * f / sr;
        for (int n = 0; n < len; ++n)
        {
            const double win = 0.5 * (1.0 - std::cos(kTwoPi * n / (len - 1)));
            const double x = static_cast<double>(out[static_cast<size_t>(startIdx + n)]) * win;
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

    const double errCents = 1200.0 * std::log2(bestFreq / 220.0);
    INFO("Input=215.6 Hz (-35ct), Corrected=" << bestFreq << " Hz (err vs 220Hz=" << errCents << " cents)");
    REQUIRE(std::abs(errCents) <= 12.0);
}

TEST_CASE("MidiHarmonyController and HarmonizerEngine MIDI mode allocate and harmonize chords", "[MidiHarmony]")
{
    SECTION("MidiHarmonyController handles allocation, sustain pedal, pitch bend, and release")
    {
        MidiHarmonyController ctrl;
        ctrl.prepare(48000.0, 4);

        ctrl.noteOn(64, 0.8f); // E4
        ctrl.noteOn(67, 0.9f); // G4
        ctrl.advanceBlock(512);

        REQUIRE(ctrl.hasAnyActiveNotes());
        REQUIRE(ctrl.getSlot(0).midiNote == 64);
        REQUIRE(ctrl.getSlot(1).midiNote == 67);
        REQUIRE(ctrl.getSlot(0).gateEnvelope > 0.5f);

        // Sustain holds released note
        ctrl.setSustainPedal(true);
        ctrl.noteOff(64);
        ctrl.advanceBlock(512);
        REQUIRE(ctrl.getSlot(0).isActive());

        // Releasing sustain releases slot smoothly
        ctrl.setSustainPedal(false);
        ctrl.advanceBlock(512);
        REQUIRE_FALSE(ctrl.getSlot(0).isActive());
        REQUIRE(ctrl.getSlot(1).isActive());

        ctrl.setPitchBendSemitones(1.5f);
        REQUIRE(ctrl.getPitchBendSemitones() == Catch::Approx(1.5f));

        ctrl.allNotesOff();
        REQUIRE_FALSE(ctrl.hasAnyActiveNotes());
    }

    SECTION("HarmonizerEngine MIDI mode is silent without MIDI notes and sounds when notes held")
    {
        const double sr = 48000.0;
        const int bs = 128;

        HarmonizerEngine engine;
        engine.prepare(sr, bs);

        GlobalParams gp;
        gp.harmonyMode = HarmonyMode::Midi;
        gp.engineMode = EngineMode::Live;
        gp.mix = 1.0f;
        gp.spaceAmount = 0.0f;
        const auto vp = makeFourActiveVoices();

        std::vector<float> in(bs, 0.0f), outL(bs, 0.0f), outR(bs, 0.0f);
        double silentEnergy = 0.0;

        for (int blk = 0; blk < 20; ++blk)
        {
            for (int i = 0; i < bs; ++i)
            {
                const double t = static_cast<double>(blk * bs + i) / sr;
                in[static_cast<size_t>(i)] = static_cast<float>(0.30 * std::sin(kTwoPi * 261.6256 * t)); // C4
            }
            const float* inPtr[1] = { in.data() };
            float* outPtr[2] = { outL.data(), outR.data() };
            engine.processBlock(inPtr, 1, outPtr, 2, bs, gp, vp);
            for (float s : outL)
                silentEnergy += static_cast<double>(s) * s;
        }
        REQUIRE(silentEnergy < 1.0e-5);

        // Play E4 (64) and G4 (67)
        engine.handleNoteOn(64, 0.9f);
        engine.handleNoteOn(67, 0.9f);

        double chordEnergy = 0.0;
        for (int blk = 20; blk < 70; ++blk)
        {
            for (int i = 0; i < bs; ++i)
            {
                const double t = static_cast<double>(blk * bs + i) / sr;
                in[static_cast<size_t>(i)] = static_cast<float>(0.30 * std::sin(kTwoPi * 261.6256 * t)); // C4
            }
            const float* inPtr[1] = { in.data() };
            float* outPtr[2] = { outL.data(), outR.data() };
            engine.processBlock(inPtr, 1, outPtr, 2, bs, gp, vp);
            for (float s : outL)
                chordEnergy += static_cast<double>(s) * s;
        }

        const auto snap = engine.getTelemetry();
        REQUIRE(chordEnergy > 0.2);
        REQUIRE(snap.activeHarmonyMode == HarmonyMode::Midi);
        REQUIRE(snap.voiceMidiActive[0]);
        REQUIRE(snap.voiceMidiActive[1]);
        REQUIRE(snap.voiceMidiNote[0] == 64);
        REQUIRE(snap.voiceMidiNote[1] == 67);
    }
}

TEST_CASE("HarmonizerEngine hot switches across all LIVE buffer sizes with exact latency alignment and no clicks", "[HarmonizerEngine][LiveBuffer]")
{
    const double sr = 48000.0;
    const int bs = 128;

    HarmonizerEngine engine;
    engine.prepare(sr, bs, LiveBufferSize::Safe512);
    REQUIRE(engine.getActiveLatencySamples() == 512);

    GlobalParams gp;
    gp.engineMode = EngineMode::Live;
    gp.liveBufferSize = LiveBufferSize::Safe512;
    gp.mix = 0.5f;
    const auto vp = makeFourActiveVoices();

    const LiveBufferSize sequence[] = {
        LiveBufferSize::Fast256,
        LiveBufferSize::Ultra128,
        LiveBufferSize::Low384,
        LiveBufferSize::Safe512
    };
    const int expectedLatencies[] = { 256, 128, 384, 512 };

    std::vector<float> in(bs, 0.0f), outL(bs, 0.0f), outR(bs, 0.0f);
    float prevOutL = 0.0f;
    float maxDelta = 0.0f;
    int sampleCursor = 0;

    for (size_t step = 0; step < 4; ++step)
    {
        gp.liveBufferSize = sequence[step];
        for (int blk = 0; blk < 20; ++blk)
        {
            for (int i = 0; i < bs; ++i)
            {
                const double t = static_cast<double>(sampleCursor++) / sr;
                in[static_cast<size_t>(i)] = 0.22f * static_cast<float>(std::sin(kTwoPi * 220.0 * t));
            }
            const float* inPtr[1] = { in.data() };
            float* outPtr[2] = { outL.data(), outR.data() };
            engine.processBlock(inPtr, 1, outPtr, 2, bs, gp, vp);

            for (int i = 0; i < bs; ++i)
            {
                REQUIRE(std::isfinite(outL[static_cast<size_t>(i)]));
                REQUIRE(std::isfinite(outR[static_cast<size_t>(i)]));
                if (step > 0 || blk > 2)
                    maxDelta = std::max(maxDelta, std::abs(outL[static_cast<size_t>(i)] - prevOutL));
                prevOutL = outL[static_cast<size_t>(i)];
            }
        }

        int newLat = 0;
        REQUIRE(engine.consumeLatencyChanged(newLat));
        REQUIRE(newLat == expectedLatencies[step]);
        REQUIRE(engine.getActiveLatencySamples() == expectedLatencies[step]);
        REQUIRE(engine.getActiveLiveBufferSize() == sequence[step]);
    }

    REQUIRE(maxDelta < 0.45f);

    for (size_t i = 0; i < 4; ++i)
    {
        HarmonizerEngine dryEngine;
        dryEngine.prepare(sr, bs, sequence[i]);
        REQUIRE(dryEngine.getActiveLatencySamples() == expectedLatencies[i]);

        GlobalParams dryGp;
        dryGp.engineMode = EngineMode::Live;
        dryGp.liveBufferSize = sequence[i];
        dryGp.mix = 0.0f;
        std::array<VoiceParams, kMaxVoices> emptyVp{};

        const int total = expectedLatencies[i] + 512;
        std::vector<float> dryIn(static_cast<size_t>(total), 0.0f);
        std::vector<float> dryOut(static_cast<size_t>(total), 0.0f);
        dryIn[0] = 0.5f;

        for (int off = 0; off < total; off += bs)
        {
            const int chunk = std::min(bs, total - off);
            const float* inPtr[1] = { dryIn.data() + off };
            float* outPtr[1] = { dryOut.data() + off };
            dryEngine.processBlock(inPtr, 1, outPtr, 1, chunk, dryGp, emptyVp);
        }

        int peakIdx = -1;
        float peakVal = 0.0f;
        for (int n = 0; n < total; ++n)
        {
            if (std::abs(dryOut[static_cast<size_t>(n)]) > peakVal)
            {
                peakVal = std::abs(dryOut[static_cast<size_t>(n)]);
                peakIdx = n;
            }
        }
        REQUIRE(peakVal == Catch::Approx(0.5f).margin(0.01f));
        REQUIRE(peakIdx == expectedLatencies[i]);
    }
}

TEST_CASE("HarmonizerEngine click-free Bypass passes exact latency-aligned dry audio", "[HarmonizerEngine][Bypass]")
{
    const double sr = 48000.0;
    const int bs = 128;

    HarmonizerEngine engine;
    engine.prepare(sr, bs, LiveBufferSize::Fast256);
    const int lat = engine.getActiveLatencySamples();
    REQUIRE(lat == 256);

    GlobalParams gp;
    gp.engineMode = EngineMode::Live;
    gp.mix = 1.0f;            // 100% wet when active
    gp.outputGainDb = 6.0f;   // Boosted wet output to verify Bypass ignores wet/output gain
    gp.bypass = true;
    const auto vp = makeFourActiveVoices();

    const int totalSamples = 4096;
    std::vector<float> in(static_cast<size_t>(totalSamples), 0.0f);
    std::vector<float> outL(static_cast<size_t>(totalSamples), 0.0f);
    std::vector<float> outR(static_cast<size_t>(totalSamples), 0.0f);
    for (int i = 0; i < totalSamples; ++i)
    {
        const double t = static_cast<double>(i) / sr;
        in[static_cast<size_t>(i)] = 0.25f * static_cast<float>(std::sin(kTwoPi * 220.0 * t));
    }

    for (int off = 0; off < totalSamples; off += bs)
    {
        const float* inPtr[1] = { in.data() + off };
        float* outPtr[2] = { outL.data() + off, outR.data() + off };
        engine.processBlock(inPtr, 1, outPtr, 2, bs, gp, vp);
    }

    for (int i = lat + 64; i < totalSamples; ++i)
    {
        REQUIRE(outL[static_cast<size_t>(i)] == Catch::Approx(in[static_cast<size_t>(i - lat)]).margin(1.0e-5f));
        REQUIRE(outR[static_cast<size_t>(i)] == Catch::Approx(in[static_cast<size_t>(i - lat)]).margin(1.0e-5f));
    }
    REQUIRE(engine.getTelemetry().bypassed);

    // Toggle Bypass off -> on smoothly without clicks or NaNs
    float prevL = outL.back();
    float maxDelta = 0.0f;
    std::vector<float> blkIn(bs, 0.0f), blkOutL(bs, 0.0f), blkOutR(bs, 0.0f);

    for (int blk = 0; blk < 40; ++blk)
    {
        gp.bypass = (blk >= 20);
        for (int i = 0; i < bs; ++i)
        {
            const double t = static_cast<double>(totalSamples + blk * bs + i) / sr;
            blkIn[static_cast<size_t>(i)] = 0.25f * static_cast<float>(std::sin(kTwoPi * 220.0 * t));
        }
        const float* inPtr[1] = { blkIn.data() };
        float* outPtr[2] = { blkOutL.data(), blkOutR.data() };
        engine.processBlock(inPtr, 1, outPtr, 2, bs, gp, vp);

        for (int i = 0; i < bs; ++i)
        {
            REQUIRE(std::isfinite(blkOutL[static_cast<size_t>(i)]));
            REQUIRE(std::isfinite(blkOutR[static_cast<size_t>(i)]));
            maxDelta = std::max(maxDelta, std::abs(blkOutL[static_cast<size_t>(i)] - prevL));
            prevL = blkOutL[static_cast<size_t>(i)];
        }
    }

    REQUIRE(maxDelta < 0.45f);
}

TEST_CASE("HarmonizerEngine 8-voice bank, MIDI 8-note polyphony, and 8V->4V voice limit transitions", "[HarmonizerEngine][8Voices]")
{
    const double sr = 48000.0;
    const int bs = 128;

    HarmonizerEngine engine;
    engine.prepare(sr, bs, LiveBufferSize::Fast256);

    GlobalParams gp;
    gp.engineMode = EngineMode::Live;
    gp.activeVoiceBankCount = 8;
    gp.mix = 1.0f;

    std::array<VoiceParams, kMaxVoices> vp{};
    const int intervals[8] = { -7, -4, -2, +2, +4, +6, +7, +9 };
    const float pans[8] = { -0.8f, -0.6f, -0.35f, -0.15f, +0.15f, +0.35f, +0.6f, +0.8f };
    for (int v = 0; v < kMaxVoices; ++v)
    {
        vp[static_cast<size_t>(v)] = {
            true, false, false, false, 60, intervals[v],
            0.0f, 0.0f, static_cast<float>(3 + v * 3), 100.0f, 14000.0f, pans[v], -6.5f, 0.2f
        };
    }

    std::vector<float> in(bs, 0.0f), outL(bs, 0.0f), outR(bs, 0.0f);
    float maxPeak = 0.0f;
    for (int blk = 0; blk < 40; ++blk)
    {
        for (int i = 0; i < bs; ++i)
        {
            const double t = static_cast<double>(blk * bs + i) / sr;
            in[static_cast<size_t>(i)] = 0.35f * static_cast<float>(std::sin(kTwoPi * 220.0 * t));
        }
        const float* inPtr[1] = { in.data() };
        float* outPtr[2] = { outL.data(), outR.data() };
        engine.processBlock(inPtr, 1, outPtr, 2, bs, gp, vp);

        for (int i = 0; i < bs; ++i)
        {
            REQUIRE(std::isfinite(outL[static_cast<size_t>(i)]));
            REQUIRE(std::isfinite(outR[static_cast<size_t>(i)]));
            maxPeak = std::max(maxPeak, std::max(std::abs(outL[static_cast<size_t>(i)]), std::abs(outR[static_cast<size_t>(i)])));
        }
    }
    REQUIRE(maxPeak <= 0.99f);
    REQUIRE(engine.getTelemetry().activeVoiceBankCount == 8);

    // 8-note MIDI chord allocation + clean shrink 8V -> 4V
    gp.harmonyMode = HarmonyMode::Midi;
    const int notes[8] = { 60, 62, 64, 65, 67, 69, 71, 72 };
    for (int n : notes)
        engine.handleNoteOn(n, 0.85f);

    for (int blk = 0; blk < 12; ++blk)
    {
        const float* inPtr[1] = { in.data() };
        float* outPtr[2] = { outL.data(), outR.data() };
        engine.processBlock(inPtr, 1, outPtr, 2, bs, gp, vp);
    }

    auto snap8 = engine.getTelemetry();
    for (int v = 0; v < 8; ++v)
        REQUIRE(snap8.voiceMidiActive[static_cast<size_t>(v)]);

    gp.activeVoiceBankCount = 4;
    for (int blk = 0; blk < 16; ++blk)
    {
        const float* inPtr[1] = { in.data() };
        float* outPtr[2] = { outL.data(), outR.data() };
        engine.processBlock(inPtr, 1, outPtr, 2, bs, gp, vp);
    }
    auto snap4 = engine.getTelemetry();
    REQUIRE(snap4.activeVoiceBankCount == 4);
    for (int v = 4; v < 8; ++v)
        REQUIRE_FALSE(snap4.voiceMidiActive[static_cast<size_t>(v)]);
}

TEST_CASE("HarmonizerEngine 8-voice LIVE mode CPU benchmark", "[HarmonizerEngine][benchmark][8Voices]")
{
    const double sr = 48000.0;
    const int bs = 128;
    const int totalSamples = 96000; // 2.0 s

    HarmonizerEngine engine;
    engine.prepare(sr, bs, LiveBufferSize::Fast256);

    GlobalParams gp;
    gp.engineMode = EngineMode::Live;
    gp.activeVoiceBankCount = 8;
    gp.mix = 0.5f;
    gp.humanize = 0.25f;

    std::array<VoiceParams, kMaxVoices> vp{};
    for (int v = 0; v < kMaxVoices; ++v)
        vp[static_cast<size_t>(v)] = { true, false, false, false, 60, v - 3, 0.0f, 0.0f, 8.0f, 120.0f, 13500.0f, 0.0f, -7.0f, 0.2f };

    std::vector<float> in(static_cast<size_t>(totalSamples), 0.0f);
    std::vector<float> outL(bs, 0.0f), outR(bs, 0.0f);
    for (int i = 0; i < totalSamples; ++i)
    {
        const double t = static_cast<double>(i) / sr;
        in[static_cast<size_t>(i)] = 0.25f * static_cast<float>(std::sin(kTwoPi * 220.0 * t));
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
    WARN("8-Voice LIVE mode processed 2000ms audio in " << elapsedMs << " ms (" << cpuPct << "% real-time CPU)");
#ifdef NDEBUG
    REQUIRE(elapsedMs < 1100.0); // < 55% real-time for 8 simultaneous voices
#else
    REQUIRE(elapsedMs < 3500.0);
#endif
}

TEST_CASE("HarmonizerEngine Fixed Pedal Note stays locked on C4 while input steps C4 -> E4", "[HarmonizerEngine][FixedNote]")
{
    const double sr = 48000.0;
    const int bs = 128;

    HarmonizerEngine engine;
    engine.prepare(sr, bs, LiveBufferSize::Safe512);

    GlobalParams gp;
    gp.engineMode = EngineMode::Live;
    gp.liveBufferSize = LiveBufferSize::Safe512;
    gp.mix = 1.0f;
    gp.spaceAmount = 0.0f;
    gp.humanize = 0.0f;
    gp.glideMs = 15.0f;

    std::array<VoiceParams, kMaxVoices> vp{};
    vp[0] = { true, false, false, true, 60, 0, 0.0f, 0.0f, 0.0f, 60.0f, 18000.0f, 0.0f, 0.0f, 0.0f };

    const int blocksPerNote = 180; // ~480 ms
    std::vector<float> out(static_cast<size_t>(2 * blocksPerNote * bs), 0.0f);
    std::vector<float> in(bs, 0.0f), outL(bs, 0.0f), outR(bs, 0.0f);
    double phase = 0.0;

    for (int blk = 0; blk < 2 * blocksPerNote; ++blk)
    {
        const double f0 = (blk < blocksPerNote) ? 261.6256 : 329.6276; // C4 -> E4
        for (int i = 0; i < bs; ++i)
        {
            phase += kTwoPi * f0 / sr;
            in[static_cast<size_t>(i)] = static_cast<float>(0.30 * std::sin(phase) + 0.12 * std::sin(2.0 * phase));
        }
        const float* inPtr[1] = { in.data() };
        float* outPtr[2] = { outL.data(), outR.data() };
        engine.processBlock(inPtr, 1, outPtr, 2, bs, gp, vp);
        for (int i = 0; i < bs; ++i)
            out[static_cast<size_t>(blk * bs + i)] = outL[static_cast<size_t>(i)];
    }

    // Measure peak frequency on second half (input is E4 = 329.63 Hz, pedal voice must hold C4 = 261.63 Hz)
    const int startIdx = blocksPerNote * bs + 8000;
    const int len = 8192;
    double bestFreq = 261.63;
    double bestMag = -1.0;
    for (int step = 0; step <= 260; ++step)
    {
        const double f = 220.0 + 140.0 * (static_cast<double>(step) / 260.0);
        double re = 0.0, im = 0.0;
        const double wStep = kTwoPi * f / sr;
        for (int n = 0; n < len; ++n)
        {
            const double win = 0.5 * (1.0 - std::cos(kTwoPi * n / (len - 1)));
            const double x = static_cast<double>(out[static_cast<size_t>(startIdx + n)]) * win;
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

    const double errCents = 1200.0 * std::log2(bestFreq / 261.6256);
    INFO("Input=329.63 Hz (E4), Pedal output=" << bestFreq << " Hz (err vs C4=" << errCents << " cents)");
    REQUIRE(std::abs(errCents) <= 35.0);
}

TEST_CASE("VoiceChannelStrip vibrato produces organic pitch modulation and VoicingPresets validate cleanly", "[Vibrato][VoicingPresets]")
{
    VoiceChannelStrip strip;
    strip.prepare(48000.0, 0);

    // Zero humanize + zero vibrato -> 0 cents
    for (int b = 0; b < 50; ++b)
        REQUIRE(strip.computeHumanizeCents(0.0f, 0.0f, 1.0f, 128) == 0.0f);

    // Zero humanize + full vibrato -> swings +/- ~30 cents
    float maxAbsVib = 0.0f;
    for (int b = 0; b < 200; ++b)
        maxAbsVib = std::max(maxAbsVib, std::abs(strip.computeHumanizeCents(0.0f, 1.0f, 1.0f, 128)));
    REQUIRE(maxAbsVib >= 20.0f);
    REQUIRE(maxAbsVib <= 31.0f);

    // Validate VoicingPresets ASCII names and computePedalMidiNote bounds
    for (const auto& vp : kVoicingPresets)
    {
        for (const char* p = vp.name; *p != '\0'; ++p)
            REQUIRE(static_cast<unsigned char>(*p) < 128);
    }
    for (int key = 0; key < 12; ++key)
    {
        const int note = computePedalMidiNote(key, 0);
        REQUIRE(note >= 48);
        REQUIRE(note <= 72);
    }
}

TEST_CASE("HarmonizerEngine telemetry reports De-Ess gain reduction, TUNE cents, and zeros both in bypass", "[HarmonizerEngine][Telemetry]")
{
    const double sr = 48000.0;

    // 1) Direct HarmonyDeEsser gain reduction telemetry
    {
        HarmonyDeEsser ds;
        ds.prepare(sr);
        const int n = 4800;
        std::vector<float> bufL(static_cast<size_t>(n), 0.0f), bufR(static_cast<size_t>(n), 0.0f);

        auto fillTone = [&](double freqHz)
        {
            for (int i = 0; i < n; ++i)
            {
                const float s = 0.35f * static_cast<float>(std::sin(kTwoPi * freqHz * (static_cast<double>(i) / sr)));
                bufL[static_cast<size_t>(i)] = s;
                bufR[static_cast<size_t>(i)] = s;
            }
        };

        fillTone(7000.0);
        ds.process(bufL.data(), bufR.data(), n, 1.0f);
        REQUIRE(ds.getLastReductionDb() >= 3.5f);

        ds.reset();
        fillTone(300.0);
        ds.process(bufL.data(), bufR.data(), n, 1.0f);
        REQUIRE(ds.getLastReductionDb() < 0.5f);

        fillTone(7000.0);
        ds.process(bufL.data(), bufR.data(), n, 0.0f);
        REQUIRE(ds.getLastReductionDb() == 0.0f);
    }

    // 2) Full engine telemetry for TUNE cents, De-Ess GR, and clean zeroing in bypass
    {
        const int bs = 128;
        HarmonizerEngine engine;
        engine.prepare(sr, bs);

        GlobalParams gp;
        gp.key = 0; // C Major
        gp.scaleIndex = 0;
        gp.engineMode = EngineMode::Live;
        gp.mix = 0.5f;
        gp.inputTuneAmount = 1.0f;
        gp.deEssAmount = 1.0f;
        gp.spaceAmount = 0.0f;

        std::array<VoiceParams, kMaxVoices> vp{};
        vp[0] = { true, false, false, false, 60, 0, 0.0f, 0.0f, 0.0f, 60.0f, 18000.0f, 0.0f, 0.0f, 0.0f };

        std::vector<float> in(bs, 0.0f), outL(bs, 0.0f), outR(bs, 0.0f);
        const double flatA3Hz = 220.0 * std::pow(2.0, -35.0 / 1200.0);

        for (int blk = 0; blk < 90; ++blk)
        {
            for (int i = 0; i < bs; ++i)
            {
                const double t = static_cast<double>(blk * bs + i) / sr;
                in[static_cast<size_t>(i)] = static_cast<float>(0.30 * std::sin(kTwoPi * flatA3Hz * t));
            }
            const float* inPtr[1] = { in.data() };
            float* outPtr[2] = { outL.data(), outR.data() };
            engine.processBlock(inPtr, 1, outPtr, 2, bs, gp, vp);
        }
        REQUIRE(engine.getTelemetry().inputTuneCorrectionCents > 15.0f);

        // Drive bright 7 kHz sibilant input with 100% wet Unison harmony to trigger De-Esser GR
        gp.mix = 1.0f;
        gp.naturalness = 0.0f; // Allow full HF through Unison grain body
        for (int blk = 0; blk < 60; ++blk)
        {
            for (int i = 0; i < bs; ++i)
            {
                const double t = static_cast<double>(blk * bs + i) / sr;
                in[static_cast<size_t>(i)] = static_cast<float>(0.35 * std::sin(kTwoPi * 7000.0 * t));
            }
            const float* inPtr[1] = { in.data() };
            float* outPtr[2] = { outL.data(), outR.data() };
            engine.processBlock(inPtr, 1, outPtr, 2, bs, gp, vp);
        }
        REQUIRE(engine.getTelemetry().deEssReductionDb > 1.0f);

        // Engage bypass -> both meters must drop cleanly to 0.0f
        gp.bypass = true;
        for (int blk = 0; blk < 4; ++blk)
        {
            const float* inPtr[1] = { in.data() };
            float* outPtr[2] = { outL.data(), outR.data() };
            engine.processBlock(inPtr, 1, outPtr, 2, bs, gp, vp);
        }
        const auto bypassSnap = engine.getTelemetry();
        REQUIRE(bypassSnap.deEssReductionDb == 0.0f);
        REQUIRE(bypassSnap.inputTuneCorrectionCents == 0.0f);
    }
}
