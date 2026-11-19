#include "Dsp/TdPsolaVoice.h"

#include <algorithm>
#include <cmath>

namespace harmonizer::dsp
{

void TdPsolaVoice::prepare(double sampleRate)
{
    fs = std::max(8000.0, sampleRate);
    liveLatencySamples = computeLiveLatencySamples(fs);

    // 12 ms ratio smoothing, 5 ms voiced/unvoiced crossfade
    smoothAlpha = 1.0 - std::exp(-1.0 / (0.012 * fs));
    blendAlpha = static_cast<float>(1.0 - std::exp(-1.0 / (0.005 * fs)));

    const double lpCut = std::min(5000.0, fs * 0.42);
    unvoicedLpAlpha = static_cast<float>(1.0 - std::exp(-kTwoPi * lpCut / fs));

    reset();
}

void TdPsolaVoice::reset() noexcept
{
    for (auto& g : grains)
        g = Grain{};

    nextSynthCenter = 0.0;
    synthInitialized = false;
    smoothedPitchRatio = 1.0;
    smoothedFormantRatio = 1.0;
    voicedBlend = 0.0f;
    unvoicedLpState = 0.0f;
}

float TdPsolaVoice::processSample(const PitchMarkDetector& markDetector,
                                  int64_t currentSampleIndex,
                                  float currentF0Hz,
                                  bool isVoiced,
                                  float targetShiftSemitones,
                                  float targetFormantSemitones) noexcept
{
    const double targetPitchRatio = std::clamp(static_cast<double>(semitonesToRatio(targetShiftSemitones)), 0.25, 4.0);
    const double targetFormantRatio = std::clamp(static_cast<double>(semitonesToRatio(targetFormantSemitones)), 0.5, 2.0);

    smoothedPitchRatio += smoothAlpha * (targetPitchRatio - smoothedPitchRatio);
    smoothedFormantRatio += smoothAlpha * (targetFormantRatio - smoothedFormantRatio);

    const float clampedF0 = std::clamp(currentF0Hz, kMinVocalHz, kMaxVocalHz);
    const double t0 = fs / static_cast<double>(clampedF0);
    const double tOut = std::clamp(t0 / smoothedPitchRatio, 12.0, fs / 30.0);
    // Keep analysis grain width tied to source period T0 (not T_out) so pitch-down
    // preserves glottal-rate modulation and generates accurate subharmonics (-5st, -12st).
    const double halfWidth = std::clamp(t0 * 1.05, 16.0, static_cast<double>(liveLatencySamples) * 1.35);

    if (!synthInitialized)
    {
        nextSynthCenter = static_cast<double>(currentSampleIndex);
        synthInitialized = true;
    }

    // Catch up if synthesis schedule ever fell behind due to sudden period expansion
    if (nextSynthCenter < static_cast<double>(currentSampleIndex) - halfWidth)
        nextSynthCenter = static_cast<double>(currentSampleIndex);

    const int64_t maxAllowedMark = std::max<int64_t>(0, currentSampleIndex - 8);

    while (nextSynthCenter - halfWidth <= static_cast<double>(currentSampleIndex))
    {
        const int64_t targetAnalysis = static_cast<int64_t>(std::llround(nextSynthCenter)) - liveLatencySamples;
        const int64_t analysisCenter = markDetector.findClosestMark(targetAnalysis, maxAllowedMark, t0);

        for (auto& g : grains)
        {
            if (!g.active)
            {
                g.active = true;
                g.synthCenter = nextSynthCenter;
                g.analysisCenter = static_cast<double>(analysisCenter);
                g.halfWidth = halfWidth;
                g.formantRatio = smoothedFormantRatio;
                break;
            }
        }

        nextSynthCenter += tOut;
    }

    double grainSum = 0.0;
    double winSum = 0.0;
    const double curN = static_cast<double>(currentSampleIndex);
    const double maxReadPos = static_cast<double>(std::max<int64_t>(2, currentSampleIndex - 2));

    for (auto& g : grains)
    {
        if (!g.active)
            continue;

        const double d = curN - g.synthCenter;
        if (d >= g.halfWidth)
        {
            g.active = false;
            continue;
        }
        if (d <= -g.halfWidth)
            continue;

        const double w = 0.5 * (1.0 + std::cos(kPi * d / g.halfWidth));
        const double readPos = std::min(maxReadPos, g.analysisCenter + d * g.formantRatio);
        const double s = static_cast<double>(markDetector.readRawSampleCubic(readPos));

        grainSum += s * w;
        winSum += w;
    }

    const double downShiftComp = (tOut > halfWidth) ? std::min(1.45, std::sqrt(tOut / halfWidth)) : 1.0;
    const float voicedOut = (winSum > 1.0e-4)
        ? static_cast<float>((grainSum / std::max(winSum, 0.90)) * downShiftComp)
        : 0.0f;

    // Unvoiced / sibilant soft-ducked filtered branch
    const float delayedRaw = markDetector.readRawSampleInt(currentSampleIndex - liveLatencySamples);
    unvoicedLpState += unvoicedLpAlpha * (delayedRaw - unvoicedLpState);
    const float unvoicedOut = 0.25f * unvoicedLpState; // -12 dB gentle consonant presence

    const float targetBlend = isVoiced ? 1.0f : 0.0f;
    voicedBlend += blendAlpha * (targetBlend - voicedBlend);

    return sanitizeSample(voicedBlend * voicedOut + (1.0f - voicedBlend) * unvoicedOut);
}

} // namespace harmonizer::dsp
