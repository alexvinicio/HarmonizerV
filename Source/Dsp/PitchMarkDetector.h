#pragma once

#include "Dsp/DspConstants.h"

#include <array>
#include <cstdint>
#include <vector>

namespace harmonizer::dsp
{

class PitchMarkDetector
{
public:
    void prepare(double sampleRate);
    void reset() noexcept;

    void pushSample(float sample, float currentF0Hz, bool isVoiced) noexcept;

    int64_t getWritePos() const noexcept { return writePos; }

    // Finds the nearest pitch mark to targetSample, bounded to <= maxAllowedPos.
    int64_t findClosestMark(int64_t targetSample, int64_t maxAllowedPos, double fallbackPeriod) const noexcept;

    float readRawSampleCubic(double absolutePos) const noexcept;
    float readRawSampleInt(int64_t absolutePos) const noexcept;

private:
    void recordMark(int64_t pos) noexcept;

    double fs = 48000.0;
    int bufferSize = 16384;
    int bufferMask = 16383;

    std::vector<float> rawBuffer;
    std::vector<float> lpBuffer;

    // 2nd-order Butterworth 1100 Hz LPF for robust glottal epoch finding
    double lpB0 = 1.0, lpB1 = 0.0, lpB2 = 0.0;
    double lpA1 = 0.0, lpA2 = 0.0;
    double lpZ1 = 0.0, lpZ2 = 0.0;

    int64_t writePos = 0;

    static constexpr int kMaxMarks = 512;
    std::array<int64_t, kMaxMarks> marks{};
    int markHead = 0;
    int markCount = 0;
    int64_t lastMark = -1;
};

} // namespace harmonizer::dsp
