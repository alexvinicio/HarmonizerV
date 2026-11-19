#include "Dsp/PitchTracker.h"

#include <algorithm>
#include <cmath>

namespace harmonizer::dsp
{

void PitchTracker::prepare(double sampleRate)
{
    fs = std::max(8000.0, sampleRate);
    windowSize = (fs > 52000.0) ? 2048 : 1024;
    hopSize = (fs > 52000.0) ? 256 : 128;

    minLag = std::max(8, static_cast<int>(std::floor(fs / kMaxVocalHz)));
    maxLag = std::min(windowSize - 64, static_cast<int>(std::ceil(fs / kMinVocalHz)));

    ringBuffer.assign(static_cast<size_t>(windowSize), 0.0f);
    windowBuf.assign(static_cast<size_t>(windowSize), 0.0f);
    cumSq.assign(static_cast<size_t>(windowSize + 1), 0.0);
    nsdf.assign(static_cast<size_t>(maxLag + 1), 0.0f);

    // 2nd-order Butterworth HPF at 60 Hz
    const double fc = 60.0;
    const double k = std::tan(kPi * fc / fs);
    const double norm = 1.0 / (1.0 + std::sqrt(2.0) * k + k * k);
    hpB0 = norm;
    hpB1 = -2.0 * norm;
    hpB2 = norm;
    hpA1 = 2.0 * (k * k - 1.0) * norm;
    hpA2 = (1.0 - std::sqrt(2.0) * k + k * k) * norm;

    reset();
}

void PitchTracker::reset() noexcept
{
    std::fill(ringBuffer.begin(), ringBuffer.end(), 0.0f);
    std::fill(windowBuf.begin(), windowBuf.end(), 0.0f);
    std::fill(cumSq.begin(), cumSq.end(), 0.0);
    std::fill(nsdf.begin(), nsdf.end(), 0.0f);

    ringWritePos = 0;
    samplesSinceHop = 0;
    filledSamples = 0;
    hpZ1 = 0.0;
    hpZ2 = 0.0;

    medianRing = { 220.0f, 220.0f, 220.0f };
    medianCount = 0;
    medianWriteIdx = 0;
    unvoicedFrames = 0;

    currentEstimate = PitchEstimate{};
    smoothedValidHz = 220.0f;
}

void PitchTracker::pushSample(float sample) noexcept
{
    if (ringBuffer.empty())
        return;

    // Analysis-only 60 Hz HPF (Direct Form II transposed)
    const double x = static_cast<double>(sanitizeSample(sample));
    const double y = hpB0 * x + hpZ1;
    hpZ1 = hpB1 * x - hpA1 * y + hpZ2;
    hpZ2 = hpB2 * x - hpA2 * y;

    ringBuffer[static_cast<size_t>(ringWritePos)] = static_cast<float>(y);
    ringWritePos = (ringWritePos + 1) % windowSize;
    if (filledSamples < windowSize)
        ++filledSamples;

    ++samplesSinceHop;
    if (samplesSinceHop >= hopSize)
    {
        samplesSinceHop = 0;
        if (filledSamples >= windowSize)
            analyzeWindow();
    }
}

void PitchTracker::processBlock(const float* samples, int numSamples) noexcept
{
    for (int i = 0; i < numSamples; ++i)
        pushSample(samples[i]);
}

void PitchTracker::analyzeWindow() noexcept
{
    // Copy chronological window into contiguous array
    double energySum = 0.0;
    cumSq[0] = 0.0;
    for (int i = 0; i < windowSize; ++i)
    {
        const int idx = (ringWritePos + i) % windowSize;
        const float val = ringBuffer[static_cast<size_t>(idx)];
        windowBuf[static_cast<size_t>(i)] = val;
        const double sq = static_cast<double>(val) * static_cast<double>(val);
        energySum += sq;
        cumSq[static_cast<size_t>(i + 1)] = energySum;
    }

    const double rms = std::sqrt(energySum / static_cast<double>(windowSize));
    if (rms < 0.0018) // ~ -55 dBFS noise gate
    {
        ++unvoicedFrames;
        if (unvoicedFrames > 3)
            medianCount = 0;
        currentEstimate.isVoiced = false;
        currentEstimate.clarity = 0.0f;
        return;
    }

    const float* x = windowBuf.data();
    nsdf[0] = 1.0f;
    for (int tau = 1; tau <= maxLag; ++tau)
    {
        const int overlap = windowSize - tau;
        double acf = 0.0;
        for (int j = 0; j < overlap; ++j)
            acf += static_cast<double>(x[j]) * static_cast<double>(x[j + tau]);

        const double m = cumSq[static_cast<size_t>(overlap)]
                       + (cumSq[static_cast<size_t>(windowSize)] - cumSq[static_cast<size_t>(tau)]);
        nsdf[static_cast<size_t>(tau)] = (m > 1.0e-12) ? static_cast<float>((2.0 * acf) / m) : 0.0f;
    }

    // MPM key maxima picking after the first negative-going transition from lag 0
    struct KeyMax
    {
        int lag = 0;
        float val = 0.0f;
    };
    std::array<KeyMax, 64> keyMaxima{};
    int numKeyMax = 0;

    bool leftLagZeroLobe = false;
    bool inPositive = false;
    int curMaxLag = 0;
    float curMaxVal = -1.0f;

    for (int tau = 1; tau <= maxLag; ++tau)
    {
        const float v = nsdf[static_cast<size_t>(tau)];
        if (!leftLagZeroLobe)
        {
            if (v <= 0.0f)
                leftLagZeroLobe = true;
            continue;
        }

        if (v > 0.0f)
        {
            if (!inPositive)
            {
                inPositive = true;
                curMaxLag = tau;
                curMaxVal = v;
            }
            else if (v > curMaxVal)
            {
                curMaxLag = tau;
                curMaxVal = v;
            }
        }
        else if (inPositive)
        {
            if (curMaxLag >= minLag && numKeyMax < static_cast<int>(keyMaxima.size()))
                keyMaxima[static_cast<size_t>(numKeyMax++)] = { curMaxLag, curMaxVal };
            inPositive = false;
        }
    }

    if (inPositive && curMaxLag >= minLag && numKeyMax < static_cast<int>(keyMaxima.size()))
        keyMaxima[static_cast<size_t>(numKeyMax++)] = { curMaxLag, curMaxVal };

    float highestMax = 0.0f;
    for (int i = 0; i < numKeyMax; ++i)
        highestMax = std::max(highestMax, keyMaxima[static_cast<size_t>(i)].val);

    if (numKeyMax == 0 || highestMax < 0.55f)
    {
        ++unvoicedFrames;
        if (unvoicedFrames > 3)
            medianCount = 0;
        currentEstimate.clarity = highestMax;
        currentEstimate.isVoiced = false;
        return;
    }

    // Select first key maximum above 0.88 * highestMax (avoids octave errors)
    const float threshold = 0.88f * highestMax;
    int chosenLag = keyMaxima[0].lag;
    float chosenClarity = keyMaxima[0].val;
    for (int i = 0; i < numKeyMax; ++i)
    {
        if (keyMaxima[static_cast<size_t>(i)].val >= threshold)
        {
            chosenLag = keyMaxima[static_cast<size_t>(i)].lag;
            chosenClarity = keyMaxima[static_cast<size_t>(i)].val;
            break;
        }
    }

    // Parabolic interpolation around chosenLag
    double refinedLag = static_cast<double>(chosenLag);
    if (chosenLag > 1 && chosenLag < maxLag)
    {
        const double yL = nsdf[static_cast<size_t>(chosenLag - 1)];
        const double yC = nsdf[static_cast<size_t>(chosenLag)];
        const double yR = nsdf[static_cast<size_t>(chosenLag + 1)];
        const double denom = 2.0 * (yL - 2.0 * yC + yR);
        if (std::abs(denom) > 1.0e-12)
        {
            const double delta = std::clamp((yL - yR) / denom, -0.5, 0.5);
            refinedLag += delta;
        }
    }

    const float rawHz = static_cast<float>(fs / std::max(1.0, refinedLag));
    if (rawHz < kMinVocalHz || rawHz > kMaxVocalHz)
    {
        ++unvoicedFrames;
        currentEstimate.isVoiced = false;
        currentEstimate.clarity = chosenClarity;
        return;
    }

    unvoicedFrames = 0;

    // 3-frame median filter
    medianRing[static_cast<size_t>(medianWriteIdx)] = rawHz;
    medianWriteIdx = (medianWriteIdx + 1) % 3;
    if (medianCount < 3)
        ++medianCount;

    float filteredHz = rawHz;
    if (medianCount == 3)
    {
        float a = medianRing[0];
        float b = medianRing[1];
        float c = medianRing[2];
        if (a > b) std::swap(a, b);
        if (b > c) std::swap(b, c);
        if (a > b) std::swap(a, b);
        filteredHz = b;
    }

    currentEstimate.frequencyHz = filteredHz;
    currentEstimate.midiNote = hzToMidi(filteredHz);
    currentEstimate.clarity = chosenClarity;
    currentEstimate.isVoiced = true;

    smoothedValidHz = smoothedValidHz * 0.65f + filteredHz * 0.35f;
}

} // namespace harmonizer::dsp
