#include "Dsp/SpectralVoice.h"
#include "Dsp/DspConstants.h"

#include <algorithm>
#include <cmath>

namespace harmonizer::dsp
{

void SpectralVoice::prepare(double sampleRate, int /*maxBlockSize*/)
{
    fs = std::max(8000.0, sampleRate);
    if (fs > 52000.0)
    {
        blockSamples = 8192;
        intervalSamples = 1536;
    }
    else
    {
        blockSamples = 4096;
        intervalSamples = 768;
    }

    stretch.configure(1, blockSamples, intervalSamples, true);
    maxChunkSamples = 256;
    latencySamples = stretch.inputLatency() + stretch.outputLatency();
    prepared = true;
    reset();
}

void SpectralVoice::reset() noexcept
{
    if (!prepared)
        return;
    stretch.reset();
    smoothedShiftSt = 0.0f;
    smoothedFormantSt = 0.0f;
    smoothedF0Hz = 220.0f;
    firstProcessAfterReset = true;
}

void SpectralVoice::process(const float* monoIn,
                            float* monoOut,
                            int numSamples,
                            float targetShiftSemitones,
                            float targetFormantSemitones,
                            float f0Hz,
                            bool isVoiced,
                            float glideMs,
                            float naturalness) noexcept
{
    if (numSamples <= 0)
        return;

    if (!prepared)
    {
        std::fill(monoOut, monoOut + numSamples, 0.0f);
        return;
    }

    const float clampedTargetShift = std::clamp(targetShiftSemitones, -24.0f, 24.0f);
    const float nat = std::clamp(naturalness, 0.0f, 1.0f);

    if (isVoiced && f0Hz >= kMinVocalHz && f0Hz <= kMaxVocalHz)
    {
        if (firstProcessAfterReset)
            smoothedF0Hz = f0Hz;
        else
            smoothedF0Hz += 0.25f * (f0Hz - smoothedF0Hz);
    }

    if (firstProcessAfterReset)
    {
        smoothedShiftSt = clampedTargetShift;
        smoothedFormantSt = std::clamp(targetFormantSemitones + nat * 0.20f * smoothedShiftSt, -12.0f, 12.0f);
        firstProcessAfterReset = false;
    }

    const double clampedGlideSec = std::max(0.004, static_cast<double>(glideMs) * 0.001);
    const float formantAlpha = static_cast<float>(1.0 - std::exp(-static_cast<double>(maxChunkSamples) / (0.015 * fs)));

    // Keep upper vocal breath natural & un-warped above tonalityLimitHz
    const float tonalityLimitHz = 14000.0f - nat * 7200.0f; // 14 kHz -> 6.8 kHz
    const float tonalityLimitNorm = (nat > 0.02f)
        ? std::clamp(tonalityLimitHz / static_cast<float>(fs), 0.04f, 0.40f)
        : 0.0f;

    // Process in <= 256-sample sub-chunks for smooth glide and bounded STFT input steps
    int offset = 0;
    while (offset < numSamples)
    {
        const int chunk = std::min(maxChunkSamples, numSamples - offset);
        const float chunkAlpha = static_cast<float>(1.0 - std::exp(-static_cast<double>(chunk) / (clampedGlideSec * fs)));
        smoothedShiftSt += chunkAlpha * (clampedTargetShift - smoothedShiftSt);

        const float targetFormant = std::clamp(targetFormantSemitones + nat * 0.20f * smoothedShiftSt, -12.0f, 12.0f);
        smoothedFormantSt += formantAlpha * (targetFormant - smoothedFormantSt);

        stretch.setTransposeSemitones(smoothedShiftSt, tonalityLimitNorm);
        stretch.setFormantSemitones(smoothedFormantSt, true);
        stretch.setFormantBase(std::clamp(smoothedF0Hz / static_cast<float>(fs), 0.0005f, 0.25f));

        const float* inPtr[1] = { monoIn + offset };
        float* outPtr[1] = { monoOut + offset };
        stretch.process(inPtr, chunk, outPtr, chunk);
        offset += chunk;
    }

    for (int i = 0; i < numSamples; ++i)
        monoOut[i] = sanitizeSample(monoOut[i]);
}

} // namespace harmonizer::dsp
