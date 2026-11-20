#pragma once

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>

namespace harmonizer::dsp
{

inline constexpr int kMaxVoices = 8;
inline constexpr int kExposedVoicesV1 = 4;

inline constexpr float kMinVocalHz = 75.0f;
inline constexpr float kMaxVocalHz = 1100.0f;

inline constexpr double kPi = 3.14159265358979323846;
inline constexpr double kTwoPi = 2.0 * kPi;

enum class EngineMode : int
{
    Live = 0,
    Studio = 1
};

enum class HarmonyMode : int
{
    Auto = 0,
    Midi = 1
};

enum class LiveBufferSize : int
{
    Ultra128 = 0, // ~2.7 ms @ 48 kHz (128 samples)
    Fast256  = 1, // ~5.3 ms @ 48 kHz (256 samples)
    Low384   = 2, // ~8.0 ms @ 48 kHz (384 samples)
    Safe512  = 3  // ~10.7 ms @ 48 kHz (512 samples)
};

struct VoiceParams
{
    bool enabled = false;
    bool mute = false;
    bool solo = false;
    int intervalSteps = 2;       // Diatonic steps (-14..+14)
    float detuneCents = 0.0f;    // -50..+50 cents
    float formantSemitones = 0.0f; // -12..+12 semitones
    float delayMs = 0.0f;        // 0..40 ms
    float hpfHz = 80.0f;         // 60..800 Hz
    float lpfHz = 16000.0f;      // 2000..18000 Hz
    float pan = 0.0f;            // -1..+1
    float gainDb = -6.0f;        // -60..+6 dB
};

struct GlobalParams
{
    int key = 0;                 // 0..11 (C..B)
    int scaleIndex = 0;          // 0..8
    EngineMode engineMode = EngineMode::Live;
    HarmonyMode harmonyMode = HarmonyMode::Auto;
    LiveBufferSize liveBufferSize = LiveBufferSize::Fast256;
    float mix = 0.45f;           // 0..1
    float outputGainDb = 0.0f;   // -24..+12 dB
    float humanize = 0.20f;      // 0..1
    float inputTuneAmount = 0.0f;// 0..1 pitch correction on lead input
    float glideMs = 28.0f;       // 0..150 ms pitch transition glide
    float naturalness = 0.35f;   // 0..1 formant follow + high-band vocal air
    float deEssAmount = 0.35f;   // 0..1 wet harmony bus sibilance control
    float spaceAmount = 0.18f;   // 0..1 wet harmony studio vocal space
};

struct TelemetrySnapshot
{
    float inputRms = 0.0f;
    float outputRms = 0.0f;
    float outputPeakL = 0.0f;
    float outputPeakR = 0.0f;
    float detectedHz = 0.0f;
    float detectedMidi = 60.0f;
    float voicedClarity = 0.0f;
    bool isVoiced = false;
    int activeLatencySamples = 0;
    EngineMode activeMode = EngineMode::Live;
    HarmonyMode activeHarmonyMode = HarmonyMode::Auto;
    float inputTuneCorrectionCents = 0.0f;
    std::array<float, kMaxVoices> voiceRms{};
    std::array<float, kMaxVoices> voiceTargetSemitones{};
    std::array<int, kMaxVoices> voiceMidiNote{};
    std::array<bool, kMaxVoices> voiceMidiActive{};
};

inline float semitonesToRatio(float semitones) noexcept
{
    return std::pow(2.0f, semitones / 12.0f);
}

inline float centsToRatio(float cents) noexcept
{
    return std::pow(2.0f, cents / 1200.0f);
}

inline float dbToGain(float db) noexcept
{
    if (db <= -59.9f)
        return 0.0f;
    return std::pow(10.0f, db * 0.05f);
}

inline float gainToDb(float gain) noexcept
{
    return 20.0f * std::log10(std::max(gain, 1.0e-6f));
}

inline float midiToHz(float midi) noexcept
{
    return 440.0f * std::pow(2.0f, (midi - 69.0f) / 12.0f);
}

inline float hzToMidi(float hz) noexcept
{
    if (hz <= 1.0f)
        return 60.0f;
    return 69.0f + 12.0f * std::log2(hz / 440.0f);
}

inline float cubicHermite(float ym1, float y0, float y1, float y2, float t) noexcept
{
    const float c0 = y0;
    const float c1 = 0.5f * (y1 - ym1);
    const float c2 = ym1 - 2.5f * y0 + 2.0f * y1 - 0.5f * y2;
    const float c3 = 0.5f * (y2 - ym1) + 1.5f * (y0 - y1);
    return ((c3 * t + c2) * t + c1) * t + c0;
}

inline float sanitizeSample(float x) noexcept
{
    if (!std::isfinite(x))
        return 0.0f;
    return std::clamp(x, -8.0f, 8.0f);
}

inline float softCeiling(float x) noexcept
{
    const float clean = sanitizeSample(x);
    const float a = std::abs(clean);
    if (a <= 0.80f)
        return clean;
    const float sign = (clean >= 0.0f) ? 1.0f : -1.0f;
    return sign * (0.80f + 0.18f * std::tanh((a - 0.80f) / 0.18f));
}

inline int computeLiveLatencySamples(double sampleRate, LiveBufferSize bufSize = LiveBufferSize::Fast256) noexcept
{
    const double sr = std::max(8000.0, sampleRate);
    double baseAt48k = 256.0;
    switch (bufSize)
    {
        case LiveBufferSize::Ultra128: baseAt48k = 128.0; break;
        case LiveBufferSize::Fast256:  baseAt48k = 256.0; break;
        case LiveBufferSize::Low384:   baseAt48k = 384.0; break;
        case LiveBufferSize::Safe512:  baseAt48k = 512.0; break;
        default:                       baseAt48k = 256.0; break;
    }
    return std::max(64, static_cast<int>(std::llround(sr * (baseAt48k / 48000.0))));
}

} // namespace harmonizer::dsp
