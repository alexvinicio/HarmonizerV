#include "Dsp/PitchMarkDetector.h"

#include <algorithm>
#include <cmath>
#include <limits>

namespace harmonizer::dsp
{

void PitchMarkDetector::prepare(double sampleRate)
{
    fs = std::max(8000.0, sampleRate);
    const int minSamples = static_cast<int>(std::ceil(fs * 0.4)); // >= 400 ms history
    int pow2 = 8192;
    while (pow2 < minSamples)
        pow2 <<= 1;

    bufferSize = pow2;
    bufferMask = pow2 - 1;
    rawBuffer.assign(static_cast<size_t>(bufferSize), 0.0f);
    lowBuffer.assign(static_cast<size_t>(bufferSize), 0.0f);
    highBuffer.assign(static_cast<size_t>(bufferSize), 0.0f);
    lpBuffer.assign(static_cast<size_t>(bufferSize), 0.0f);

    // Fixed 4.5 kHz band-split Butterworth LPF (low + high == raw)
    {
        const double fc = std::min(4500.0, fs * 0.40);
        const double k = std::tan(kPi * fc / fs);
        const double norm = 1.0 / (1.0 + std::sqrt(2.0) * k + k * k);
        splitB0 = k * k * norm;
        splitB1 = 2.0 * splitB0;
        splitB2 = splitB0;
        splitA1 = 2.0 * (k * k - 1.0) * norm;
        splitA2 = (1.0 - std::sqrt(2.0) * k + k * k) * norm;
    }

    reset();
}

void PitchMarkDetector::updateAdaptiveLpf(double cutoffHz) noexcept
{
    const double fc = std::clamp(cutoffHz, 120.0, std::min(1250.0, fs * 0.40));
    const double k = std::tan(kPi * fc / fs);
    const double norm = 1.0 / (1.0 + std::sqrt(2.0) * k + k * k);
    lpB0 = k * k * norm;
    lpB1 = 2.0 * lpB0;
    lpB2 = lpB0;
    lpA1 = 2.0 * (k * k - 1.0) * norm;
    lpA2 = (1.0 - std::sqrt(2.0) * k + k * k) * norm;
    lastCoefCutoffHz = fc;
}

void PitchMarkDetector::reset() noexcept
{
    std::fill(rawBuffer.begin(), rawBuffer.end(), 0.0f);
    std::fill(lowBuffer.begin(), lowBuffer.end(), 0.0f);
    std::fill(highBuffer.begin(), highBuffer.end(), 0.0f);
    std::fill(lpBuffer.begin(), lpBuffer.end(), 0.0f);

    lpZ1 = 0.0;
    lpZ2 = 0.0;
    splitZ1 = 0.0;
    splitZ2 = 0.0;

    smoothedCutoffHz = 320.0;
    updateAdaptiveLpf(smoothedCutoffHz);

    writePos = 0;
    marks.fill(0.0);
    markHead = 0;
    markCount = 0;
    lastMark = -1.0;
    measuredPeriodSamples = fs / 220.0;
}

void PitchMarkDetector::recordMark(double pos, double fallbackPeriod, bool isVoiced) noexcept
{
    if (isVoiced && lastMark >= 0.0)
    {
        const double diff = pos - lastMark;
        if (diff >= 0.68 * fallbackPeriod && diff <= 1.42 * fallbackPeriod)
        {
            const double clampedDiff = std::clamp(diff, 0.80 * fallbackPeriod, 1.20 * fallbackPeriod);
            measuredPeriodSamples = measuredPeriodSamples * 0.72 + clampedDiff * 0.28;
        }
        else
        {
            measuredPeriodSamples = measuredPeriodSamples * 0.70 + fallbackPeriod * 0.30;
        }
    }
    else
    {
        measuredPeriodSamples = measuredPeriodSamples * 0.70 + fallbackPeriod * 0.30;
    }

    marks[static_cast<size_t>(markHead)] = pos;
    markHead = (markHead + 1) % kMaxMarks;
    if (markCount < kMaxMarks)
        ++markCount;
    lastMark = pos;
}

void PitchMarkDetector::pushSample(float sample, float currentF0Hz, bool isVoiced) noexcept
{
    if (rawBuffer.empty())
        return;

    const float clampedF0 = std::clamp(currentF0Hz, kMinVocalHz, kMaxVocalHz);
    const double t0 = fs / static_cast<double>(clampedF0);

    // Track cutoff gently around 1.45 * F0 to strip vowel formants cleanly
    const double targetCut = std::clamp(static_cast<double>(clampedF0) * 1.45, 140.0, 1200.0);
    smoothedCutoffHz += (targetCut - smoothedCutoffHz) * 0.002;
    if (std::abs(smoothedCutoffHz - lastCoefCutoffHz) > lastCoefCutoffHz * 0.03)
        updateAdaptiveLpf(smoothedCutoffHz);

    const float clean = sanitizeSample(sample);
    const double x = static_cast<double>(clean);

    // Adaptive epoch-tracking LPF
    const double yLp = lpB0 * x + lpZ1;
    lpZ1 = lpB1 * x - lpA1 * yLp + lpZ2;
    lpZ2 = lpB2 * x - lpA2 * yLp;

    // 4.5 kHz band-split crossover
    const double yLow = splitB0 * x + splitZ1;
    splitZ1 = splitB1 * x - splitA1 * yLow + splitZ2;
    splitZ2 = splitB2 * x - splitA2 * yLow;

    const size_t idx = static_cast<size_t>(writePos & bufferMask);
    rawBuffer[idx] = clean;
    lowBuffer[idx] = static_cast<float>(yLow);
    highBuffer[idx] = static_cast<float>(x - yLow);
    lpBuffer[idx] = static_cast<float>(yLp);

    const int64_t curPos = writePos;
    ++writePos;

    if (!isVoiced)
    {
        if (lastMark < 0.0 || static_cast<double>(curPos) - lastMark >= t0)
            recordMark(static_cast<double>(curPos), t0, false);
        return;
    }

    if (lastMark < 0.0 || static_cast<double>(curPos) - lastMark > 2.2 * t0)
    {
        const int64_t earliestValid = std::max<int64_t>(0, writePos - bufferSize + 4);
        const int64_t searchStart = std::max(earliestValid, curPos - static_cast<int64_t>(std::llround(t0)));
        int64_t bestIdx = curPos;
        float bestVal = -std::numeric_limits<float>::max();
        for (int64_t p = searchStart; p <= curPos; ++p)
        {
            const float v = lpBuffer[static_cast<size_t>(p & bufferMask)];
            if (v > bestVal)
            {
                bestVal = v;
                bestIdx = p;
            }
        }
        recordMark(static_cast<double>(bestIdx), t0, false);
        return;
    }

    const int64_t minNext = static_cast<int64_t>(std::llround(lastMark + 0.75 * t0));
    const int64_t maxNext = static_cast<int64_t>(std::llround(lastMark + 1.25 * t0));

    if (curPos >= maxNext)
    {
        const int64_t earliestValid = std::max<int64_t>(0, writePos - bufferSize + 4);
        const int64_t sStart = std::max(earliestValid, minNext);
        const int64_t sEnd = std::min(curPos, maxNext);

        const double expected = lastMark + t0;
        const double halfSpan = std::max(1.0, 0.25 * t0);

        int64_t bestIdx = std::clamp(static_cast<int64_t>(std::llround(expected)), sStart, sEnd);
        double bestScore = -1.0e9;

        for (int64_t p = sStart; p <= sEnd; ++p)
        {
            const double val = static_cast<double>(lpBuffer[static_cast<size_t>(p & bufferMask)]);
            const double dist = std::abs(static_cast<double>(p) - expected) / halfSpan;
            const double weight = 1.0 - 0.12 * dist;
            const double score = (val >= 0.0) ? (val * weight) : val;
            if (score > bestScore)
            {
                bestScore = score;
                bestIdx = p;
            }
        }

        // Sub-sample parabolic refinement of the glottal peak
        double refinedMark = static_cast<double>(bestIdx);
        if (bestIdx > sStart && bestIdx < sEnd)
        {
            const double yL = static_cast<double>(lpBuffer[static_cast<size_t>((bestIdx - 1) & bufferMask)]);
            const double yC = static_cast<double>(lpBuffer[static_cast<size_t>(bestIdx & bufferMask)]);
            const double yR = static_cast<double>(lpBuffer[static_cast<size_t>((bestIdx + 1) & bufferMask)]);
            const double denom = 2.0 * (yL - 2.0 * yC + yR);
            if (std::abs(denom) > 1.0e-12)
                refinedMark += std::clamp((yL - yR) / denom, -0.5, 0.5);
        }

        recordMark(refinedMark, t0, true);
    }
}

double PitchMarkDetector::findClosestMark(double targetSample, double maxAllowedPos, double fallbackPeriod) const noexcept
{
    if (markCount == 0)
        return std::min(targetSample, maxAllowedPos);

    double bestMark = std::min(targetSample, maxAllowedPos);
    double bestDist = std::numeric_limits<double>::max();
    const double earliestValid = static_cast<double>(std::max<int64_t>(0, writePos - bufferSize + 8));

    for (int i = 0; i < markCount; ++i)
    {
        const int ringIdx = (markHead - 1 - i + kMaxMarks) % kMaxMarks;
        const double m = marks[static_cast<size_t>(ringIdx)];
        if (m < earliestValid)
            break;
        if (m > maxAllowedPos)
            continue;

        const double dist = std::abs(m - targetSample);
        if (dist < bestDist)
        {
            bestDist = dist;
            bestMark = m;
        }
        else if (m < targetSample)
        {
            break;
        }
    }

    const double maxAcceptableGap = 1.35 * std::max(16.0, fallbackPeriod);
    if (bestDist > maxAcceptableGap)
        return std::clamp(targetSample, earliestValid, maxAllowedPos);

    return bestMark;
}

float PitchMarkDetector::readIntFromBuffer(const std::vector<float>& buf, int mask, int64_t writePos, int bufSize, int64_t absolutePos) noexcept
{
    if (buf.empty())
        return 0.0f;
    const int64_t oldest = std::max<int64_t>(0, writePos - bufSize + 2);
    const int64_t newest = std::max<int64_t>(0, writePos - 1);
    const int64_t clamped = std::clamp(absolutePos, oldest, newest);
    return buf[static_cast<size_t>(clamped & mask)];
}

float PitchMarkDetector::readCubicFromBuffer(const std::vector<float>& buf, int mask, int64_t writePos, int bufSize, double absolutePos) noexcept
{
    if (buf.empty())
        return 0.0f;

    const double oldest = static_cast<double>(std::max<int64_t>(1, writePos - bufSize + 4));
    const double newest = static_cast<double>(std::max<int64_t>(2, writePos - 2));
    const double clamped = std::clamp(absolutePos, oldest, newest);

    const int64_t idx = static_cast<int64_t>(std::floor(clamped));
    const float frac = static_cast<float>(clamped - static_cast<double>(idx));

    const float ym1 = buf[static_cast<size_t>((idx - 1) & mask)];
    const float y0  = buf[static_cast<size_t>(idx & mask)];
    const float y1  = buf[static_cast<size_t>((idx + 1) & mask)];
    const float y2  = buf[static_cast<size_t>((idx + 2) & mask)];
    return cubicHermite(ym1, y0, y1, y2, frac);
}

float PitchMarkDetector::readRawSampleInt(int64_t absolutePos) const noexcept
{
    return readIntFromBuffer(rawBuffer, bufferMask, writePos, bufferSize, absolutePos);
}

float PitchMarkDetector::readRawSampleCubic(double absolutePos) const noexcept
{
    return readCubicFromBuffer(rawBuffer, bufferMask, writePos, bufferSize, absolutePos);
}

float PitchMarkDetector::readLowSampleInt(int64_t absolutePos) const noexcept
{
    return readIntFromBuffer(lowBuffer, bufferMask, writePos, bufferSize, absolutePos);
}

float PitchMarkDetector::readLowSampleCubic(double absolutePos) const noexcept
{
    return readCubicFromBuffer(lowBuffer, bufferMask, writePos, bufferSize, absolutePos);
}

float PitchMarkDetector::readHighSampleInt(int64_t absolutePos) const noexcept
{
    return readIntFromBuffer(highBuffer, bufferMask, writePos, bufferSize, absolutePos);
}

} // namespace harmonizer::dsp
