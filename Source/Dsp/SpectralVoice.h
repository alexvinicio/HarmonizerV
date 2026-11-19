#pragma once

#include "signalsmith-stretch/signalsmith-stretch.h"

#include <vector>

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
                 float shiftSemitones,
                 float formantSemitones,
                 float smoothedF0Hz) noexcept;

private:
    signalsmith::stretch::SignalsmithStretch<float> stretch;
    double fs = 48000.0;
    int blockSamples = 2048;
    int intervalSamples = 512;
    int latencySamples = 2048;
    int maxChunkSamples = 512;
    bool prepared = false;
};

} // namespace harmonizer::dsp
