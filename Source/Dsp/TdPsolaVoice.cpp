#include "Dsp/TdPsolaVoice.h"

#include <algorithm>
#include <cmath>

namespace harmonizer::dsp
{

void TdPsolaVoice::prepare(double sampleRate, int voiceIdx)
{
    fs = std::max(8000.0, sampleRate);
    voiceIndex = voiceIdx;
    liveLatencySamples = computeLiveLatencySamples(fs);

    formantSmoothAlpha = 1.0 - std::exp(-1.0 / (0.014 * fs));
    lastGlideMs = 28.0;
    glideAlpha = 1.0 - std::exp(-1.0 / (0.028 * fs));
    blendAlpha = static_cast<float>(1.0 - std::exp(-1.0 / (0.012 * fs)));

    const double lpCut = std::min(5000.0, fs * 0.42);
    unvoicedLpAlpha = static_cast<float>(1.0 - std::exp(-kTwoPi * lpCut / fs));

    reset();
}

void TdPsolaVoice::reset() noexcept
{
    for (auto& g : grains)
        g = Grain{};

    prevTemplate.fill(0.0f);
    hasPrevTemplate = false;

    nextSynthCenter = 0.0;
    synthInitialized = false;
    smoothedShiftSt = 0.0;
    smoothedFormantRatio = 1.0;
    voicedBlend = 0.0f;
    unvoicedLpState = 0.0f;
}

double TdPsolaVoice::alignAnalysisCenterWsola(const PitchMarkDetector& markDetector,
                                               double candidateMark,
                                               double maxAllowedMark,
                                               float voicingConfidence) noexcept
{
    if (voicingConfidence < 0.25f)
    {
        hasPrevTemplate = false;
        return std::min(candidateMark, maxAllowedMark);
    }

    double bestCenter = std::min(candidateMark, maxAllowedMark);

    if (hasPrevTemplate)
    {
        // Search +/- 5 samples around candidateMark for best waveform phase coherence
        constexpr int kSearchRadius = 5;
        std::array<double, 2 * kSearchRadius + 1> scores{};
        double bestScore = -1.0e18;
        int bestOffset = 0;

        for (int k = -kSearchRadius; k <= kSearchRadius; ++k)
        {
            const double testCenter = candidateMark + static_cast<double>(k);
            if (testCenter > maxAllowedMark)
            {
                scores[static_cast<size_t>(k + kSearchRadius)] = -1.0e18;
                continue;
            }

            double dot = 0.0;
            double normSq = 1.0e-9;
            for (int t = 0; t < kTemplateLen; ++t)
            {
                const int offset = (t - 4) * 2; // -8, -6, ..., +8
                const float s = markDetector.readLowSampleCubic(testCenter + static_cast<double>(offset));
                dot += static_cast<double>(s) * static_cast<double>(prevTemplate[static_cast<size_t>(t)]);
                normSq += static_cast<double>(s) * static_cast<double>(s);
            }

            // Slight preference for staying close to the glottal mark center
            const double proxPenalty = 1.0 - 0.015 * static_cast<double>(std::abs(k));
            const double score = (dot / std::sqrt(normSq)) * proxPenalty;
            scores[static_cast<size_t>(k + kSearchRadius)] = score;

            if (score > bestScore)
            {
                bestScore = score;
                bestOffset = k;
            }
        }

        double refinedOffset = static_cast<double>(bestOffset);
        const int idx = bestOffset + kSearchRadius;
        if (idx > 0 && idx < 2 * kSearchRadius)
        {
            const double sL = scores[static_cast<size_t>(idx - 1)];
            const double sC = scores[static_cast<size_t>(idx)];
            const double sR = scores[static_cast<size_t>(idx + 1)];
            if (sL > -1.0e12 && sR > -1.0e12)
            {
                const double denom = 2.0 * (sL - 2.0 * sC + sR);
                if (std::abs(denom) > 1.0e-12)
                    refinedOffset += std::clamp((sL - sR) / denom, -0.5, 0.5);
            }
        }

        bestCenter = std::min(maxAllowedMark, candidateMark + refinedOffset);
    }

    for (int t = 0; t < kTemplateLen; ++t)
    {
        const int offset = (t - 4) * 2;
        prevTemplate[static_cast<size_t>(t)] = markDetector.readLowSampleCubic(bestCenter + static_cast<double>(offset));
    }
    hasPrevTemplate = true;

    return bestCenter;
}

float TdPsolaVoice::processSample(const PitchMarkDetector& markDetector,
                                  int64_t currentSampleIndex,
                                  float currentF0Hz,
                                  float voicingConfidence,
                                  float targetShiftSemitones,
                                  float targetFormantSemitones,
                                  float glideMs,
                                  float naturalness) noexcept
{
    const double clampedGlide = std::clamp(static_cast<double>(glideMs), 4.0, 200.0);
    if (std::abs(clampedGlide - lastGlideMs) > 0.5)
    {
        lastGlideMs = clampedGlide;
        glideAlpha = 1.0 - std::exp(-1.0 / (0.001 * clampedGlide * fs));
    }

    const double clampedShift = std::clamp(static_cast<double>(targetShiftSemitones), -24.0, 24.0);
    smoothedShiftSt += glideAlpha * (clampedShift - smoothedShiftSt);
    const double pitchRatio = std::clamp(static_cast<double>(semitonesToRatio(static_cast<float>(smoothedShiftSt))), 0.25, 4.0);

    const float nat = std::clamp(naturalness, 0.0f, 1.0f);
    // Natural vocal register formant-follow (~22% of pitch shift at full naturalness)
    const double effectiveFormantSt = std::clamp(
        static_cast<double>(targetFormantSemitones) + static_cast<double>(nat) * 0.22 * smoothedShiftSt,
        -12.0, 12.0);
    const double targetFormantRatio = std::clamp(static_cast<double>(semitonesToRatio(static_cast<float>(effectiveFormantSt))), 0.5, 2.0);
    smoothedFormantRatio += formantSmoothAlpha * (targetFormantRatio - smoothedFormantRatio);

    const float clampedF0 = std::clamp(currentF0Hz, kMinVocalHz, kMaxVocalHz);
    const double trackerT0 = fs / static_cast<double>(clampedF0);
    const double instT0 = markDetector.getMeasuredPeriodSamples();
    const double t0 = (voicingConfidence > 0.25f)
        ? std::clamp(instT0, trackerT0 * 0.80, trackerT0 * 1.20)
        : trackerT0;

    const double tOut = std::clamp(t0 / pitchRatio, 12.0, fs / 30.0);
    const double formantMargin = std::max(1.0, smoothedFormantRatio);
    const double maxSafeHalfW = static_cast<double>(liveLatencySamples) * 0.92 / std::max(1.0, formantMargin - 0.10);
    const double halfWidth = std::clamp(t0 * 1.05, 16.0, std::max(64.0, maxSafeHalfW));

    if (!synthInitialized)
    {
        nextSynthCenter = static_cast<double>(currentSampleIndex);
        synthInitialized = true;
    }

    if (nextSynthCenter < static_cast<double>(currentSampleIndex) - halfWidth)
        nextSynthCenter = static_cast<double>(currentSampleIndex);

    const double maxAllowedMark = static_cast<double>(std::max<int64_t>(0, currentSampleIndex - 8));

    while (nextSynthCenter - halfWidth <= static_cast<double>(currentSampleIndex))
    {
        const double targetAnalysis = nextSynthCenter - static_cast<double>(liveLatencySamples);
        const double closestMark = markDetector.findClosestMark(targetAnalysis, maxAllowedMark, t0);
        const double analysisCenter = alignAnalysisCenterWsola(markDetector, closestMark, maxAllowedMark, voicingConfidence);

        for (auto& g : grains)
        {
            if (!g.active)
            {
                g.active = true;
                g.synthCenter = nextSynthCenter;
                g.analysisCenter = analysisCenter;
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

        // Band-split body: blend pure <4.5kHz body with full raw when naturalness is low
        const double lowSample = static_cast<double>(markDetector.readLowSampleCubic(readPos));
        const double rawSample = static_cast<double>(markDetector.readRawSampleCubic(readPos));
        const double s = rawSample + static_cast<double>(nat) * (lowSample - rawSample);

        grainSum += s * w;
        winSum += w;
    }

    const double downShiftComp = (tOut > halfWidth) ? std::min(1.45, std::sqrt(tOut / halfWidth)) : 1.0;
    const double diff = winSum - 0.85;
    const double smoothDenom = 0.5 * (winSum + 0.85 + std::sqrt(diff * diff + 0.06));
    const float bodyOut = (winSum > 1.0e-4)
        ? static_cast<float>((grainSum / smoothDenom) * downShiftComp)
        : 0.0f;

    // Clean decorrelated air (>4.5 kHz) pass-through prevents robotic consonant buzz
    const int64_t airOffset = static_cast<int64_t>(voiceIndex * 19);
    const float cleanAir = markDetector.readHighSampleInt(currentSampleIndex - liveLatencySamples - airOffset);
    const float voicedOut = bodyOut + (0.50f * nat) * cleanAir;

    // Soft-ducked unvoiced consonant pass-through
    const float delayedRaw = markDetector.readRawSampleInt(currentSampleIndex - liveLatencySamples);
    unvoicedLpState += unvoicedLpAlpha * (delayedRaw - unvoicedLpState);
    const float unvoicedOut = 0.25f * unvoicedLpState;

    const float targetBlend = std::clamp(voicingConfidence, 0.0f, 1.0f);
    voicedBlend += blendAlpha * (targetBlend - voicedBlend);

    return sanitizeSample(voicedBlend * voicedOut + (1.0f - voicedBlend) * unvoicedOut);
}

} // namespace harmonizer::dsp
