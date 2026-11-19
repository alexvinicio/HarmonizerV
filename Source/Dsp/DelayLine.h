#pragma once

#include "Dsp/DspConstants.h"

#include <algorithm>
#include <vector>

namespace harmonizer::dsp
{

class DelayLine
{
public:
    void prepare(int maxDelay)
    {
        maxDelaySamples = std::max(8, maxDelay);
        int pow2 = 16;
        while (pow2 < maxDelaySamples + 8)
            pow2 <<= 1;

        buffer.assign(static_cast<size_t>(pow2), 0.0f);
        mask = pow2 - 1;
        writeIndex = 0;
    }

    void reset() noexcept
    {
        std::fill(buffer.begin(), buffer.end(), 0.0f);
        writeIndex = 0;
    }

    void push(float sample) noexcept
    {
        if (buffer.empty())
            return;
        buffer[static_cast<size_t>(writeIndex)] = sample;
        writeIndex = (writeIndex + 1) & mask;
    }

    float readInt(int delaySamples) const noexcept
    {
        if (buffer.empty())
            return 0.0f;
        const int clamped = std::clamp(delaySamples, 0, maxDelaySamples);
        const int idx = (writeIndex - 1 - clamped) & mask;
        return buffer[static_cast<size_t>(idx)];
    }

    float readCubic(float delaySamples) const noexcept
    {
        if (buffer.empty())
            return 0.0f;

        const float clamped = std::clamp(delaySamples, 1.0f, static_cast<float>(maxDelaySamples));
        const int intDelay = static_cast<int>(clamped);
        const float frac = clamped - static_cast<float>(intDelay);

        const float ym1 = readInt(intDelay - 1);
        const float y0 = readInt(intDelay);
        const float y1 = readInt(intDelay + 1);
        const float y2 = readInt(intDelay + 2);
        return cubicHermite(ym1, y0, y1, y2, frac);
    }

private:
    std::vector<float> buffer;
    int mask = 0;
    int writeIndex = 0;
    int maxDelaySamples = 8;
};

} // namespace harmonizer::dsp
