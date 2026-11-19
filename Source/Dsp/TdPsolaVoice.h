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
    void prepare(double sampleRate, int voiceIdx = 0);
    void reset() noexcept;

    int getLatencySamples() const noexcept { return liveLatencySamples; }

    float processSample(const PitchMarkDetector& markDetector,
                        int64_t currentSampleIndex,
                        float currentF0Hz,
                        float voicingConfidence,
                        float targetShiftSemitones,
                        float targetFormantSemitones,
                        float glideMs,
                        float naturalness) noexcept;

private:
    struct Grain
    {
        bool active = false;
        double synthCenter = 0.0;
        double analysisCenter = 0.0;
        double halfWidth = 200.0;
        double formantRatio = 1.0;
    };

    double alignAnalysisCenterWsola(const PitchMarkDetector& markDetector,
                                    double candidateMark,
                                    double maxAllowedMark,
                                    float voicingConfidence) noexcept;

    static constexpr int kGrainPoolSize = 32;
    std::array<Grain, kGrainPoolSize> grains{};

    static constexpr int kTemplateLen = 9; // [-8..+8] step 2 samples
    std::array<float, kTemplateLen> prevTemplate{};
    bool hasPrevTemplate = false;

    double fs = 48000.0;
    int voiceIndex = 0;
    int liveLatencySamples = 512;

    double nextSynthCenter = 0.0;
    bool synthInitialized = false;

    double smoothedShiftSt = 0.0;
    double smoothedFormantRatio = 1.0;
    double formantSmoothAlpha = 0.01;
    double lastGlideMs = 28.0;
    double glideAlpha = 0.01;

    float voicedBlend = 0.0f;
    float blendAlpha = 0.01f;

    // Gentle 5 kHz LPF state for unvoiced sibilant pass-through
    float unvoicedLpState = 0.0f;
    float unvoicedLpAlpha = 0.4f;
};

} // namespace harmonizer::dsp
