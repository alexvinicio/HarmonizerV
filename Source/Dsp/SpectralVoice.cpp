#include "Dsp/SpectralVoice.h"
#include "Dsp/DspConstants.h"

#include <algorithm>

namespace harmonizer::dsp
{

void SpectralVoice::prepare(double sampleRate, int /*maxBlockSize*/)
{
    fs = std::max(8000.0, sampleRate);
    if (fs > 52000.0)
    {
        blockSamples = 4096;
        intervalSamples = 1024;
    }
    else
    {
        blockSamples = 2048;
        intervalSamples = 512;
    }

    stretch.configure(1, blockSamples, intervalSamples, true);
    maxChunkSamples = std::max(128, intervalSamples);
    latencySamples = stretch.inputLatency() + stretch.outputLatency();
    prepared = true;
    reset();
}

void SpectralVoice::reset() noexcept
{
    if (!prepared)
        return;
    stretch.reset();
}

void SpectralVoice::process(const float* monoIn,
                            float* monoOut,
                            int numSamples,
                            float shiftSemitones,
                            float formantSemitones,
                            float smoothedF0Hz) noexcept
{
    if (numSamples <= 0)
        return;

    if (!prepared)
    {
        std::fill(monoOut, monoOut + numSamples, 0.0f);
        return;
    }

    stretch.setTransposeSemitones(std::clamp(shiftSemitones, -24.0f, 24.0f));
    stretch.setFormantSemitones(std::clamp(formantSemitones, -12.0f, 12.0f), true);

    const float clampedF0 = std::clamp(smoothedF0Hz, kMinVocalHz, kMaxVocalHz);
    stretch.setFormantBase(std::clamp(clampedF0 / static_cast<float>(fs), 0.0005f, 0.25f));

    // Process in chunks <= intervalSamples so large host buffers stay lock-free without overflowing STFT history
    int offset = 0;
    while (offset < numSamples)
    {
        const int chunk = std::min(maxChunkSamples, numSamples - offset);
        const float* inPtr[1] = { monoIn + offset };
        float* outPtr[1] = { monoOut + offset };
        stretch.process(inPtr, chunk, outPtr, chunk);
        offset += chunk;
    }

    for (int i = 0; i < numSamples; ++i)
        monoOut[i] = sanitizeSample(monoOut[i]);
}

} // namespace harmonizer::dsp
