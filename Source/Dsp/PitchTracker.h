#pragma once

#include "Dsp/DspConstants.h"

#include <array>
#include <vector>

namespace harmonizer::dsp
{

struct PitchEstimate
{
    float frequencyHz = 0.0f;
    float midiNote = 60.0f;
    float clarity = 0.0f;
    float voicingConfidence = 0.0f;
    bool isVoiced = false;
};

class PitchTracker
{
public:
    void prepare(double sampleRate);
    void reset() noexcept;

    void pushSample(float sample) noexcept;
    void processBlock(const float* samples, int numSamples) noexcept;

    PitchEstimate getEstimate() const noexcept { return currentEstimate; }
    float getInstantHz() const noexcept { return instantValidHz; }
    float getSmoothedValidHz() const noexcept { return smoothedValidHz; }

private:
    void analyzeWindow() noexcept;

    double fs = 48000.0;
    int windowSize = 1024;
    int hopSize = 128;
    int minLag = 43;
    int maxLag = 640;

    std::vector<float> ringBuffer;
    std::vector<float> windowBuf;
    std::vector<double> cumSq;
    std::vector<float> nsdf;

    int ringWritePos = 0;
    int samplesSinceHop = 0;
    int filledSamples = 0;

    // 2nd-order Butterworth 60 Hz HPF (analysis path only)
    double hpB0 = 1.0, hpB1 = 0.0, hpB2 = 0.0;
    double hpA1 = 0.0, hpA2 = 0.0;
    double hpZ1 = 0.0, hpZ2 = 0.0;

    // 3-frame median filter for voiced estimates
    std::array<float, 3> medianRing{ 220.0f, 220.0f, 220.0f };
    int medianCount = 0;
    int medianWriteIdx = 0;
    int unvoicedFrames = 0;

    bool voicedState = false;
    int holdHops = 6;
    int holdCounter = 0;

    PitchEstimate currentEstimate{};
    float instantValidHz = 220.0f;
    float smoothedValidHz = 220.0f;
};

} // namespace harmonizer::dsp
