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
    double getMeasuredPeriodSamples() const noexcept { return measuredPeriodSamples; }

    // Finds the nearest sub-sample pitch mark to targetSample, bounded to <= maxAllowedPos.
    double findClosestMark(double targetSample, double maxAllowedPos, double fallbackPeriod) const noexcept;

    float readRawSampleCubic(double absolutePos) const noexcept;
    float readRawSampleInt(int64_t absolutePos) const noexcept;
    float readLowSampleCubic(double absolutePos) const noexcept;
    float readLowSampleInt(int64_t absolutePos) const noexcept;
    float readHighSampleInt(int64_t absolutePos) const noexcept;

private:
    void updateAdaptiveLpf(double cutoffHz) noexcept;
    void recordMark(double pos, double fallbackPeriod, bool isVoiced) noexcept;
    static float readCubicFromBuffer(const std::vector<float>& buf, int mask, int64_t writePos, int bufSize, double absolutePos) noexcept;
    static float readIntFromBuffer(const std::vector<float>& buf, int mask, int64_t writePos, int bufSize, int64_t absolutePos) noexcept;

    double fs = 48000.0;
    int bufferSize = 16384;
    int bufferMask = 16383;

    std::vector<float> rawBuffer;
    std::vector<float> lowBuffer;
    std::vector<float> highBuffer;
    std::vector<float> lpBuffer;

    // Adaptive 2nd-order Butterworth LPF for clean glottal epoch finding
    double lpB0 = 1.0, lpB1 = 0.0, lpB2 = 0.0;
    double lpA1 = 0.0, lpA2 = 0.0;
    double lpZ1 = 0.0, lpZ2 = 0.0;
    double smoothedCutoffHz = 320.0;
    double lastCoefCutoffHz = 320.0;

    // 4.5 kHz phase-complementary band-split crossover LPF
    double splitB0 = 1.0, splitB1 = 0.0, splitB2 = 0.0;
    double splitA1 = 0.0, splitA2 = 0.0;
    double splitZ1 = 0.0, splitZ2 = 0.0;

    int64_t writePos = 0;

    static constexpr int kMaxMarks = 512;
    std::array<double, kMaxMarks> marks{};
    int markHead = 0;
    int markCount = 0;
    double lastMark = -1.0;
    double measuredPeriodSamples = 218.0;
};

} // namespace harmonizer::dsp
