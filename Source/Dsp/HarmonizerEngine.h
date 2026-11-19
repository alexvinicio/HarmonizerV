#pragma once

#include "Dsp/DelayLine.h"
#include "Dsp/DspConstants.h"
#include "Dsp/PitchMarkDetector.h"
#include "Dsp/PitchTracker.h"
#include "Dsp/ScaleHarmonizer.h"
#include "Dsp/SpectralVoice.h"
#include "Dsp/TdPsolaVoice.h"
#include "Dsp/VoiceChannelStrip.h"

#include <array>
#include <atomic>
#include <vector>

namespace harmonizer::dsp
{

class HarmonizerEngine
{
public:
    void prepare(double sampleRate, int maxBlockSize);
    void reset() noexcept;

    void processBlock(const float* const* inChannels,
                      int numInChannels,
                      float* const* outChannels,
                      int numOutChannels,
                      int numSamples,
                      const GlobalParams& globalParams,
                      const std::array<VoiceParams, kMaxVoices>& voiceParams) noexcept;

    int getActiveLatencySamples() const noexcept { return activeLatencySamples; }
    int getLiveLatencySamples() const noexcept { return liveLatencySamples; }
    int getStudioLatencySamples() const noexcept { return studioLatencySamples; }
    EngineMode getActiveMode() const noexcept { return activeMode; }

    bool consumeLatencyChanged(int& outNewLatencySamples) noexcept;
    TelemetrySnapshot getTelemetry() const noexcept;

private:
    void processSubBlock(const float* const* inChannels,
                         int inOffset,
                         int numInChannels,
                         float* const* outChannels,
                         int outOffset,
                         int numOutChannels,
                         int numSamples,
                         const GlobalParams& globalParams,
                         const std::array<VoiceParams, kMaxVoices>& voiceParams) noexcept;

    enum class SwitchState
    {
        Stable = 0,
        FadingOut,
        FadingIn
    };

    double fs = 48000.0;
    int maxChunkSize = 512;

    PitchTracker pitchTracker;
    ScaleHarmonizer scaleHarmonizer;
    PitchMarkDetector pitchMarkDetector;

    std::array<TdPsolaVoice, kMaxVoices> liveVoices;
    std::array<SpectralVoice, kMaxVoices> studioVoices;
    std::array<VoiceChannelStrip, kMaxVoices> channelStrips;
    std::array<int, kMaxVoices> voiceTailRemaining{};

    DelayLine dryDelayL;
    DelayLine dryDelayR;

    std::vector<float> monoInBuf;
    std::vector<float> voiceOutBuf;
    std::vector<float> wetBusL;
    std::vector<float> wetBusR;

    int liveLatencySamples = 512;
    int studioLatencySamples = 2048;
    int activeLatencySamples = 512;

    EngineMode activeMode = EngineMode::Live;
    EngineMode pendingMode = EngineMode::Live;
    SwitchState switchState = SwitchState::Stable;
    int fadeLengthSamples = 720;
    int fadePos = 0;

    float smoothedMix = 0.45f;
    float smoothedOutGain = 1.0f;
    float paramSmoothAlpha = 0.01f;
    bool firstBlockAfterReset = true;
    bool prepared = false;

    std::atomic<bool> latencyChanged{ false };
    std::atomic<int> reportedLatency{ 512 };

    struct AtomicTelemetry
    {
        std::atomic<float> inputRms{ 0.0f };
        std::atomic<float> outputRms{ 0.0f };
        std::atomic<float> outputPeakL{ 0.0f };
        std::atomic<float> outputPeakR{ 0.0f };
        std::atomic<float> detectedHz{ 0.0f };
        std::atomic<float> detectedMidi{ 60.0f };
        std::atomic<float> voicedClarity{ 0.0f };
        std::atomic<bool> isVoiced{ false };
        std::atomic<int> activeLatencySamples{ 512 };
        std::atomic<int> activeMode{ 0 };
        std::array<std::atomic<float>, kMaxVoices> voiceRms{};
        std::array<std::atomic<float>, kMaxVoices> voiceTargetSemitones{};
    } telemetry;
};

} // namespace harmonizer::dsp
