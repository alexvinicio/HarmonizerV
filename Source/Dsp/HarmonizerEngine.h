#pragma once

#include "Dsp/DelayLine.h"
#include "Dsp/DspConstants.h"
#include "Dsp/HarmonyBusFx.h"
#include "Dsp/MidiHarmonyController.h"
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
    void prepare(double sampleRate, int maxBlockSize, LiveBufferSize initialLiveBuffer = LiveBufferSize::Fast256);
    void reset() noexcept;

    void handleNoteOn(int midiNote, float velocity) noexcept { midiController.noteOn(midiNote, velocity); }
    void handleNoteOff(int midiNote) noexcept { midiController.noteOff(midiNote); }
    void handleSustainPedal(bool down) noexcept { midiController.setSustainPedal(down); }
    void handlePitchBend(float semitones) noexcept { midiController.setPitchBendSemitones(semitones); }
    void handleAllNotesOff() noexcept { midiController.allNotesOff(); }

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
    LiveBufferSize getActiveLiveBufferSize() const noexcept { return activeLiveBuffer; }

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
    MidiHarmonyController midiController;

    TdPsolaVoice inputTunerLive;
    SpectralVoice inputTunerStudio;

    std::array<TdPsolaVoice, kMaxVoices> liveVoices;
    std::array<SpectralVoice, kMaxVoices> studioVoices;
    std::array<VoiceChannelStrip, kMaxVoices> channelStrips;
    std::array<int, kMaxVoices> voiceTailRemaining{};
    HarmonyBusFx harmonyBusFx;

    DelayLine dryDelayL;
    DelayLine dryDelayR;

    std::vector<float> monoInBuf;
    std::vector<float> tunedLeadBuf;
    std::vector<float> voiceOutBuf;
    std::vector<float> wetBusL;
    std::vector<float> wetBusR;

    int liveLatencySamples = 256;
    int studioLatencySamples = 2048;
    int activeLatencySamples = 256;

    EngineMode activeMode = EngineMode::Live;
    EngineMode pendingMode = EngineMode::Live;
    LiveBufferSize activeLiveBuffer = LiveBufferSize::Fast256;
    LiveBufferSize pendingLiveBuffer = LiveBufferSize::Fast256;
    SwitchState switchState = SwitchState::Stable;
    int fadeLengthSamples = 720;
    int fadePos = 0;

    float smoothedMix = 0.45f;
    float smoothedOutGain = 1.0f;
    float smoothedCorrectionSt = 0.0f;
    float smoothedTuneBlend = 0.0f;
    float smoothedBusTrim = 1.0f;
    float smoothedBypassWet = 1.0f;
    float paramSmoothAlpha = 0.01f;
    bool firstBlockAfterReset = true;
    bool prepared = false;

    std::atomic<bool> latencyChanged{ false };
    std::atomic<int> reportedLatency{ 256 };

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
        std::atomic<bool> bypassed{ false };
        std::atomic<int> activeLatencySamples{ 256 };
        std::atomic<int> activeMode{ 0 };
        std::atomic<int> activeHarmonyMode{ 0 };
        std::atomic<float> inputTuneCorrectionCents{ 0.0f };
        std::array<std::atomic<float>, kMaxVoices> voiceRms{};
        std::array<std::atomic<float>, kMaxVoices> voiceTargetSemitones{};
        std::array<std::atomic<int>, kMaxVoices> voiceMidiNote{};
        std::array<std::atomic<bool>, kMaxVoices> voiceMidiActive{};
    } telemetry;
};

} // namespace harmonizer::dsp
