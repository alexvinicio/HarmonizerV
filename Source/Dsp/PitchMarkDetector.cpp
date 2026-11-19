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
    lpBuffer.assign(static_cast<size_t>(bufferSize), 0.0f);

    // 2nd-order Butterworth LPF at 1100 Hz
    const double fc = std::min(1100.0, fs * 0.4);
    const double k = std::tan(kPi * fc / fs);
    const double norm = 1.0 / (1.0 + std::sqrt(2.0) * k + k * k);
    lpB0 = k * k * norm;
    lpB1 = 2.0 * lpB0;
    lpB2 = lpB0;
    lpA1 = 2.0 * (k * k - 1.0) * norm;
    lpA2 = (1.0 - std::sqrt(2.0) * k + k * k) * norm;

    reset();
}

void PitchMarkDetector::reset() noexcept
{
    std::fill(rawBuffer.begin(), rawBuffer.end(), 0.0f);
    std::fill(lpBuffer.begin(), lpBuffer.end(), 0.0f);
    lpZ1 = 0.0;
    lpZ2 = 0.0;
    writePos = 0;
    marks.fill(0);
    markHead = 0;
    markCount = 0;
    lastMark = -1;
}

void PitchMarkDetector::recordMark(int64_t pos) noexcept
{
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

    const float clean = sanitizeSample(sample);
    const double x = static_cast<double>(clean);
    const double y = lpB0 * x + lpZ1;
    lpZ1 = lpB1 * x - lpA1 * y + lpZ2;
    lpZ2 = lpB2 * x - lpA2 * y;

    const size_t idx = static_cast<size_t>(writePos & bufferMask);
    rawBuffer[idx] = clean;
    lpBuffer[idx] = static_cast<float>(y);

    const int64_t curPos = writePos;
    ++writePos;

    const float clampedF0 = std::clamp(currentF0Hz, kMinVocalHz, kMaxVocalHz);
    const double t0 = fs / static_cast<double>(clampedF0);

    if (!isVoiced)
    {
        if (lastMark < 0 || curPos - lastMark >= static_cast<int64_t>(std::llround(t0)))
            recordMark(curPos);
        return;
    }

    if (lastMark < 0 || curPos - lastMark > static_cast<int64_t>(std::llround(2.2 * t0)))
    {
        // Re-seed on local maximum inside the most recent pitch period
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
        recordMark(bestIdx);
        return;
    }

    const int64_t minNext = lastMark + static_cast<int64_t>(std::llround(0.75 * t0));
    const int64_t maxNext = lastMark + static_cast<int64_t>(std::llround(1.25 * t0));

    if (curPos >= maxNext)
    {
        const int64_t earliestValid = std::max<int64_t>(0, writePos - bufferSize + 4);
        const int64_t sStart = std::max(earliestValid, minNext);
        const int64_t sEnd = std::min(curPos, maxNext);

        const double expected = static_cast<double>(lastMark) + t0;
        const double halfSpan = std::max(1.0, 0.25 * t0);

        int64_t bestIdx = static_cast<int64_t>(std::llround(expected));
        bestIdx = std::clamp(bestIdx, sStart, sEnd);
        double bestScore = -1.0e9;

        for (int64_t p = sStart; p <= sEnd; ++p)
        {
            const double val = static_cast<double>(lpBuffer[static_cast<size_t>(p & bufferMask)]);
            const double dist = std::abs(static_cast<double>(p) - expected) / halfSpan;
            const double weight = 1.0 - 0.15 * dist;
            const double score = (val >= 0.0) ? (val * weight) : val;
            if (score > bestScore)
            {
                bestScore = score;
                bestIdx = p;
            }
        }
        recordMark(bestIdx);
    }
}

int64_t PitchMarkDetector::findClosestMark(int64_t targetSample, int64_t maxAllowedPos, double fallbackPeriod) const noexcept
{
    if (markCount == 0)
        return std::min(targetSample, maxAllowedPos);

    int64_t bestMark = std::min(targetSample, maxAllowedPos);
    int64_t bestDist = std::numeric_limits<int64_t>::max();
    const int64_t earliestValid = std::max<int64_t>(0, writePos - bufferSize + 8);

    // Walk newest to oldest; marks are monotonic increasing
    for (int i = 0; i < markCount; ++i)
    {
        const int ringIdx = (markHead - 1 - i + kMaxMarks) % kMaxMarks;
        const int64_t m = marks[static_cast<size_t>(ringIdx)];
        if (m < earliestValid)
            break;
        if (m > maxAllowedPos)
            continue;

        const int64_t dist = std::abs(m - targetSample);
        if (dist < bestDist)
        {
            bestDist = dist;
            bestMark = m;
        }
        else if (m < targetSample)
        {
            break; // Moving further away into the past
        }
    }

    const int64_t maxAcceptableGap = static_cast<int64_t>(std::llround(1.35 * std::max(16.0, fallbackPeriod)));
    if (bestDist > maxAcceptableGap)
        return std::clamp(targetSample, earliestValid, maxAllowedPos);

    return bestMark;
}

float PitchMarkDetector::readRawSampleInt(int64_t absolutePos) const noexcept
{
    if (rawBuffer.empty())
        return 0.0f;
    const int64_t oldest = std::max<int64_t>(0, writePos - bufferSize + 2);
    const int64_t newest = std::max<int64_t>(0, writePos - 1);
    const int64_t clamped = std::clamp(absolutePos, oldest, newest);
    return rawBuffer[static_cast<size_t>(clamped & bufferMask)];
}

float PitchMarkDetector::readRawSampleCubic(double absolutePos) const noexcept
{
    if (rawBuffer.empty())
        return 0.0f;

    const double oldest = static_cast<double>(std::max<int64_t>(1, writePos - bufferSize + 4));
    const double newest = static_cast<double>(std::max<int64_t>(2, writePos - 2));
    const double clamped = std::clamp(absolutePos, oldest, newest);

    const int64_t idx = static_cast<int64_t>(std::floor(clamped));
    const float frac = static_cast<float>(clamped - static_cast<double>(idx));

    const float ym1 = rawBuffer[static_cast<size_t>((idx - 1) & bufferMask)];
    const float y0  = rawBuffer[static_cast<size_t>(idx & bufferMask)];
    const float y1  = rawBuffer[static_cast<size_t>((idx + 1) & bufferMask)];
    const float y2  = rawBuffer[static_cast<size_t>((idx + 2) & bufferMask)];
    return cubicHermite(ym1, y0, y1, y2, frac);
}

} // namespace harmonizer::dsp
