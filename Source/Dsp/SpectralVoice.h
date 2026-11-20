#pragma once

#include "signalsmith-stretch/signalsmith-stretch.h"

namespace harmonizer::dsp
{

class SpectralVoice
{
public:
    void prepare(double sampleRate, int maxBlockSize);
    void reset() noexcept;

    int getLatencySamples() const noexcept { return latencySamples; }

    void process(const float* monoIn,
                 float* monoOut,
                 int numSamples,
                 float targetShiftSemitones,
                 float targetFormantSemitones,
                 float f0Hz,
                 bool isVoiced,
                 float glideMs,
                 float naturalness) noexcept;

private:
    signalsmith::stretch::SignalsmithStretch<float> stretch;
    double fs = 48000.0;
    int blockSamples = 4096;
    int intervalSamples = 768;
    int latencySamples = 4096;
    int maxChunkSamples = 256;
    bool prepared = false;

    float smoothedShiftSt = 0.0f;
    float smoothedFormantSt = 0.0f;
    float smoothedF0Hz = 220.0f;
    bool firstProcessAfterReset = true;
};

} // namespace harmonizer::dsp
