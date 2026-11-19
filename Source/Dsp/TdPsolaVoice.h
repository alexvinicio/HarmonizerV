#pragma once

#include "Dsp/DspConstants.h"
#include "Dsp/PitchMarkDetector.h"

#include <array>
#include <cstdint>

namespace harmonizer::dsp
{

class TdPsolaVoice
{
public:
    void prepare(double sampleRate);
    void reset() noexcept;

    int getLatencySamples() const noexcept { return liveLatencySamples; }

    float processSample(const PitchMarkDetector& markDetector,
                        int64_t currentSampleIndex,
                        float currentF0Hz,
                        bool isVoiced,
                        float targetShiftSemitones,
                        float targetFormantSemitones) noexcept;

private:
    struct Grain
    {
        bool active = false;
        double synthCenter = 0.0;
        double analysisCenter = 0.0;
        double halfWidth = 200.0;
        double formantRatio = 1.0;
    };

    static constexpr int kGrainPoolSize = 32;
    std::array<Grain, kGrainPoolSize> grains{};

    double fs = 48000.0;
    int liveLatencySamples = 512;

    double nextSynthCenter = 0.0;
    bool synthInitialized = false;

    double smoothedPitchRatio = 1.0;
    double smoothedFormantRatio = 1.0;
    double smoothAlpha = 0.01;

    float voicedBlend = 0.0f;
    float blendAlpha = 0.01f;

    // Gentle 5 kHz LPF state for unvoiced sibilant pass-through
    float unvoicedLpState = 0.0f;
    float unvoicedLpAlpha = 0.4f;
};

} // namespace harmonizer::dsp
