#include "Dsp/HarmonizerEngine.h"

#include <algorithm>
#include <cmath>

namespace harmonizer::dsp
{

void HarmonizerEngine::prepare(double sampleRate, int maxBlockSize)
{
    fs = std::max(8000.0, sampleRate);
    maxChunkSize = std::max(64, maxBlockSize);

    pitchTracker.prepare(fs);
    scaleHarmonizer.reset();
    pitchMarkDetector.prepare(fs);
    midiController.prepare(fs, kExposedVoicesV1);

    inputTunerLive.prepare(fs, kMaxVoices);
    inputTunerStudio.prepare(fs, maxChunkSize);

    for (int v = 0; v < kMaxVoices; ++v)
    {
        liveVoices[static_cast<size_t>(v)].prepare(fs, v);
        studioVoices[static_cast<size_t>(v)].prepare(fs, maxChunkSize);
        channelStrips[static_cast<size_t>(v)].prepare(fs, v);
        voiceTailRemaining[static_cast<size_t>(v)] = 0;
    }
    harmonyBusFx.prepare(fs);

    liveLatencySamples = liveVoices[0].getLatencySamples();
    studioLatencySamples = studioVoices[0].getLatencySamples();
    activeLatencySamples = (activeMode == EngineMode::Live) ? liveLatencySamples : studioLatencySamples;

    const int maxLat = std::max(liveLatencySamples, studioLatencySamples);
    dryDelayL.prepare(maxLat + maxChunkSize + 256);
    dryDelayR.prepare(maxLat + maxChunkSize + 256);

    monoInBuf.assign(static_cast<size_t>(maxChunkSize), 0.0f);
    tunedLeadBuf.assign(static_cast<size_t>(maxChunkSize), 0.0f);
    voiceOutBuf.assign(static_cast<size_t>(maxChunkSize), 0.0f);
    wetBusL.assign(static_cast<size_t>(maxChunkSize), 0.0f);
    wetBusR.assign(static_cast<size_t>(maxChunkSize), 0.0f);

    fadeLengthSamples = std::max(64, static_cast<int>(std::llround(fs * 0.015)));
    paramSmoothAlpha = static_cast<float>(1.0 - std::exp(-1.0 / (0.015 * fs)));
    prepared = true;

    reset();

    reportedLatency.store(activeLatencySamples, std::memory_order_relaxed);
    latencyChanged.store(true, std::memory_order_release);
}

void HarmonizerEngine::reset() noexcept
{
    if (!prepared)
        return;

    pitchTracker.reset();
    scaleHarmonizer.reset();
    pitchMarkDetector.reset();
    midiController.reset();

    inputTunerLive.reset();
    inputTunerStudio.reset();

    for (int v = 0; v < kMaxVoices; ++v)
    {
        liveVoices[static_cast<size_t>(v)].reset();
        studioVoices[static_cast<size_t>(v)].reset();
        channelStrips[static_cast<size_t>(v)].reset();
        voiceTailRemaining[static_cast<size_t>(v)] = 0;
    }

    dryDelayL.reset();
    dryDelayR.reset();
    harmonyBusFx.reset();

    switchState = SwitchState::Stable;
    fadePos = 0;
    smoothedMix = 0.45f;
    smoothedOutGain = 1.0f;
    smoothedCorrectionSt = 0.0f;
    smoothedTuneBlend = 0.0f;
    smoothedBusTrim = 1.0f;
    firstBlockAfterReset = true;

    telemetry.inputRms.store(0.0f, std::memory_order_relaxed);
    telemetry.outputRms.store(0.0f, std::memory_order_relaxed);
    telemetry.outputPeakL.store(0.0f, std::memory_order_relaxed);
    telemetry.outputPeakR.store(0.0f, std::memory_order_relaxed);
    telemetry.detectedHz.store(0.0f, std::memory_order_relaxed);
    telemetry.isVoiced.store(false, std::memory_order_relaxed);
    telemetry.inputTuneCorrectionCents.store(0.0f, std::memory_order_relaxed);
    for (int v = 0; v < kMaxVoices; ++v)
    {
        telemetry.voiceRms[static_cast<size_t>(v)].store(0.0f, std::memory_order_relaxed);
        telemetry.voiceMidiNote[static_cast<size_t>(v)].store(-1, std::memory_order_relaxed);
        telemetry.voiceMidiActive[static_cast<size_t>(v)].store(false, std::memory_order_relaxed);
    }
}

bool HarmonizerEngine::consumeLatencyChanged(int& outNewLatencySamples) noexcept
{
    if (latencyChanged.exchange(false, std::memory_order_acq_rel))
    {
        outNewLatencySamples = reportedLatency.load(std::memory_order_acquire);
        return true;
    }
    return false;
}

TelemetrySnapshot HarmonizerEngine::getTelemetry() const noexcept
{
    TelemetrySnapshot snap;
    snap.inputRms = telemetry.inputRms.load(std::memory_order_relaxed);
    snap.outputRms = telemetry.outputRms.load(std::memory_order_relaxed);
    snap.outputPeakL = telemetry.outputPeakL.load(std::memory_order_relaxed);
    snap.outputPeakR = telemetry.outputPeakR.load(std::memory_order_relaxed);
    snap.detectedHz = telemetry.detectedHz.load(std::memory_order_relaxed);
    snap.detectedMidi = telemetry.detectedMidi.load(std::memory_order_relaxed);
    snap.voicedClarity = telemetry.voicedClarity.load(std::memory_order_relaxed);
    snap.isVoiced = telemetry.isVoiced.load(std::memory_order_relaxed);
    snap.activeLatencySamples = telemetry.activeLatencySamples.load(std::memory_order_relaxed);
    snap.activeMode = static_cast<EngineMode>(telemetry.activeMode.load(std::memory_order_relaxed));
    snap.activeHarmonyMode = static_cast<HarmonyMode>(telemetry.activeHarmonyMode.load(std::memory_order_relaxed));
    snap.inputTuneCorrectionCents = telemetry.inputTuneCorrectionCents.load(std::memory_order_relaxed);
    for (int v = 0; v < kMaxVoices; ++v)
    {
        snap.voiceRms[static_cast<size_t>(v)] = telemetry.voiceRms[static_cast<size_t>(v)].load(std::memory_order_relaxed);
        snap.voiceTargetSemitones[static_cast<size_t>(v)] = telemetry.voiceTargetSemitones[static_cast<size_t>(v)].load(std::memory_order_relaxed);
        snap.voiceMidiNote[static_cast<size_t>(v)] = telemetry.voiceMidiNote[static_cast<size_t>(v)].load(std::memory_order_relaxed);
        snap.voiceMidiActive[static_cast<size_t>(v)] = telemetry.voiceMidiActive[static_cast<size_t>(v)].load(std::memory_order_relaxed);
    }
    return snap;
}

void HarmonizerEngine::processBlock(const float* const* inChannels,
                                   int numInChannels,
                                   float* const* outChannels,
                                   int numOutChannels,
                                   int numSamples,
                                   const GlobalParams& globalParams,
                                   const std::array<VoiceParams, kMaxVoices>& voiceParams) noexcept
{
    if (numSamples <= 0 || numInChannels <= 0 || numOutChannels <= 0 || inChannels == nullptr || outChannels == nullptr)
        return;

    if (!prepared)
    {
        for (int c = 0; c < numOutChannels; ++c)
        {
            if (outChannels[c] != nullptr)
                std::fill(outChannels[c], outChannels[c] + numSamples, 0.0f);
        }
        return;
    }

    int offset = 0;
    while (offset < numSamples)
    {
        const int chunk = std::min(maxChunkSize, numSamples - offset);

        if (switchState == SwitchState::Stable && globalParams.engineMode != activeMode)
        {
            pendingMode = globalParams.engineMode;
            switchState = SwitchState::FadingOut;
            fadePos = 0;
        }

        int subLen = chunk;
        if (switchState == SwitchState::FadingOut || switchState == SwitchState::FadingIn)
        {
            const int remainingFade = std::max(1, fadeLengthSamples - fadePos);
            subLen = std::min(chunk, remainingFade);
        }

        processSubBlock(inChannels, offset, numInChannels,
                        outChannels, offset, numOutChannels,
                        subLen, globalParams, voiceParams);

        if (switchState == SwitchState::FadingOut && fadePos >= fadeLengthSamples)
        {
            activeMode = pendingMode;
            activeLatencySamples = (activeMode == EngineMode::Live) ? liveLatencySamples : studioLatencySamples;

            inputTunerLive.reset();
            inputTunerStudio.reset();
            for (int v = 0; v < kMaxVoices; ++v)
            {
                if (activeMode == EngineMode::Live)
                    liveVoices[static_cast<size_t>(v)].reset();
                else
                    studioVoices[static_cast<size_t>(v)].reset();
                channelStrips[static_cast<size_t>(v)].reset();
            }
            dryDelayL.reset();
            dryDelayR.reset();
            harmonyBusFx.reset();

            reportedLatency.store(activeLatencySamples, std::memory_order_release);
            latencyChanged.store(true, std::memory_order_release);

            switchState = SwitchState::FadingIn;
            fadePos = 0;
        }
        else if (switchState == SwitchState::FadingIn && fadePos >= fadeLengthSamples)
        {
            switchState = SwitchState::Stable;
            fadePos = 0;
        }

        offset += subLen;
    }
}

void HarmonizerEngine::processSubBlock(const float* const* inChannels,
                                      int inOffset,
                                      int numInChannels,
                                      float* const* outChannels,
                                      int outOffset,
                                      int numOutChannels,
                                      int numSamples,
                                      const GlobalParams& globalParams,
                                      const std::array<VoiceParams, kMaxVoices>& voiceParams) noexcept
{
    double inRmsAcc = 0.0;
    for (int i = 0; i < numSamples; ++i)
    {
        const float inL = sanitizeSample(inChannels[0][inOffset + i]);
        const float inR = (numInChannels >= 2) ? sanitizeSample(inChannels[1][inOffset + i]) : inL;
        const float mono = (numInChannels >= 2) ? 0.5f * (inL + inR) : inL;

        monoInBuf[static_cast<size_t>(i)] = mono;
        inRmsAcc += static_cast<double>(mono) * static_cast<double>(mono);

        dryDelayL.push(inL);
        dryDelayR.push(inR);
    }

    // Clear wet accumulator bus
    std::fill(wetBusL.begin(), wetBusL.begin() + numSamples, 0.0f);
    std::fill(wetBusR.begin(), wetBusR.begin() + numSamples, 0.0f);

    // Update pitch tracker & shared pitch-mark buffer once per block
    pitchTracker.processBlock(monoInBuf.data(), numSamples);
    const PitchEstimate pitchEst = pitchTracker.getEstimate();
    const float instantF0 = pitchTracker.getInstantHz();
    const float validF0 = pitchTracker.getSmoothedValidHz();

    const int64_t markBasePos = pitchMarkDetector.getWritePos();
    for (int i = 0; i < numSamples; ++i)
        pitchMarkDetector.pushSample(monoInBuf[static_cast<size_t>(i)], instantF0, pitchEst.isVoiced);

    const ScaleType scale = static_cast<ScaleType>(std::clamp(globalParams.scaleIndex, 0, static_cast<int>(ScaleType::NumScales) - 1));

    // Input Pitch Correction (zero extra latency, zero double-shifting)
    const float tuneAmt = std::clamp(globalParams.inputTuneAmount, 0.0f, 1.0f);
    const float quantizedMidi = ScaleHarmonizer::quantizeToScaleMidi(pitchEst.midiNote, globalParams.key, scale);
    const float rawErrSt = (pitchEst.isVoiced && pitchEst.voicingConfidence > 0.35f)
        ? std::clamp(quantizedMidi - pitchEst.midiNote, -1.5f, 1.5f)
        : 0.0f;

    const float retuneMs = 80.0f - 68.0f * tuneAmt;
    const float corrAlpha = static_cast<float>(1.0 - std::exp(-static_cast<double>(numSamples) / (0.001 * static_cast<double>(retuneMs) * fs)));
    smoothedCorrectionSt += corrAlpha * (rawErrSt * tuneAmt - smoothedCorrectionSt);

    const float targetTuneBlend = (tuneAmt > 0.005f) ? std::clamp(tuneAmt / 0.05f, 0.0f, 1.0f) : 0.0f;
    const bool runInputTuner = (tuneAmt > 0.005f || smoothedTuneBlend > 0.001f);

    if (runInputTuner)
    {
        if (activeMode == EngineMode::Live)
        {
            for (int i = 0; i < numSamples; ++i)
            {
                tunedLeadBuf[static_cast<size_t>(i)] = inputTunerLive.processSample(
                    pitchMarkDetector,
                    markBasePos + i,
                    instantF0,
                    pitchEst.voicingConfidence,
                    smoothedCorrectionSt,
                    0.0f,
                    retuneMs,
                    globalParams.naturalness);
            }
        }
        else
        {
            inputTunerStudio.process(
                monoInBuf.data(),
                tunedLeadBuf.data(),
                numSamples,
                smoothedCorrectionSt,
                0.0f,
                validF0,
                pitchEst.isVoiced,
                retuneMs,
                globalParams.naturalness);
        }
    }

    // Advance polyphonic MIDI state
    midiController.advanceBlock(numSamples);
    const bool isMidiMode = (globalParams.harmonyMode == HarmonyMode::Midi);
    const float activeInputMidi = pitchEst.isVoiced ? pitchEst.midiNote : hzToMidi(instantF0);

    bool anySolo = false;
    for (int v = 0; v < kMaxVoices; ++v)
    {
        const auto& vp = voiceParams[static_cast<size_t>(v)];
        if (isMidiMode)
        {
            const auto& slot = midiController.getSlot(v);
            if ((slot.isActive() || slot.gateEnvelope > 0.001f) && vp.solo)
            {
                anySolo = true;
                break;
            }
        }
        else if (vp.enabled && vp.solo)
        {
            anySolo = true;
            break;
        }
    }

    const int tailHoldSamples = static_cast<int>(std::ceil(fs * 0.04));
    int activeVoiceCount = 0;

    for (int v = 0; v < kMaxVoices; ++v)
    {
        const auto& vp = voiceParams[static_cast<size_t>(v)];
        const auto& midiSlot = midiController.getSlot(v);
        const bool midiActiveOrTail = midiSlot.isActive() || midiSlot.gateEnvelope > 0.001f;

        const bool audible = isMidiMode
            ? (midiActiveOrTail && (anySolo ? vp.solo : !vp.mute))
            : (vp.enabled && (anySolo ? vp.solo : !vp.mute));

        if (isMidiMode ? (midiSlot.isActive() && audible) : audible)
            ++activeVoiceCount;

        if (audible)
            voiceTailRemaining[static_cast<size_t>(v)] = tailHoldSamples;
        else if (voiceTailRemaining[static_cast<size_t>(v)] > 0)
            voiceTailRemaining[static_cast<size_t>(v)] = std::max(0, voiceTailRemaining[static_cast<size_t>(v)] - numSamples);

        float baseShiftSt = 0.0f;
        if (isMidiMode)
        {
            if (midiSlot.midiNote >= 0)
            {
                const float targetMidi = static_cast<float>(midiSlot.midiNote) + midiController.getPitchBendSemitones();
                baseShiftSt = std::clamp(targetMidi - activeInputMidi + vp.detuneCents * 0.01f, -24.0f, 24.0f);
            }
            telemetry.voiceMidiNote[static_cast<size_t>(v)].store(midiSlot.midiNote, std::memory_order_relaxed);
            telemetry.voiceMidiActive[static_cast<size_t>(v)].store(midiSlot.isActive(), std::memory_order_relaxed);
        }
        else
        {
            baseShiftSt = scaleHarmonizer.computeShiftSemitones(
                pitchEst.midiNote, pitchEst.isVoiced, globalParams.key, scale, vp.intervalSteps, vp.detuneCents)
                + smoothedCorrectionSt;
            telemetry.voiceMidiNote[static_cast<size_t>(v)].store(-1, std::memory_order_relaxed);
            telemetry.voiceMidiActive[static_cast<size_t>(v)].store(false, std::memory_order_relaxed);
        }

        const float humanizeCents = channelStrips[static_cast<size_t>(v)].computeHumanizeCents(globalParams.humanize, numSamples);
        const float totalShiftSt = std::clamp(baseShiftSt + humanizeCents * 0.01f, -24.0f, 24.0f);

        telemetry.voiceTargetSemitones[static_cast<size_t>(v)].store(baseShiftSt, std::memory_order_relaxed);

        if (!audible && voiceTailRemaining[static_cast<size_t>(v)] == 0)
        {
            telemetry.voiceRms[static_cast<size_t>(v)].store(0.0f, std::memory_order_relaxed);
            continue;
        }

        if (activeMode == EngineMode::Live)
        {
            auto& liveVoice = liveVoices[static_cast<size_t>(v)];
            for (int i = 0; i < numSamples; ++i)
            {
                voiceOutBuf[static_cast<size_t>(i)] = liveVoice.processSample(
                    pitchMarkDetector,
                    markBasePos + i,
                    instantF0,
                    pitchEst.voicingConfidence,
                    totalShiftSt,
                    vp.formantSemitones,
                    globalParams.glideMs,
                    globalParams.naturalness);
            }
        }
        else
        {
            studioVoices[static_cast<size_t>(v)].process(
                monoInBuf.data(),
                voiceOutBuf.data(),
                numSamples,
                totalShiftSt,
                vp.formantSemitones,
                validF0,
                pitchEst.isVoiced,
                globalParams.glideMs,
                globalParams.naturalness);
        }

        if (isMidiMode)
        {
            const float velGain = 0.45f + 0.55f * midiSlot.velocity;
            const float envGain = midiSlot.gateEnvelope * velGain;
            for (int i = 0; i < numSamples; ++i)
                voiceOutBuf[static_cast<size_t>(i)] *= envGain;
        }

        float voiceRms = 0.0f;
        channelStrips[static_cast<size_t>(v)].processAndAccumulate(
            voiceOutBuf.data(),
            wetBusL.data(),
            wetBusR.data(),
            numSamples,
            vp,
            audible,
            globalParams.humanize,
            voiceRms);

        telemetry.voiceRms[static_cast<size_t>(v)].store(voiceRms, std::memory_order_relaxed);
    }

    // Automatic active-voice headroom scaling so multi-voice Mix 100% stays clean without clipping
    const float targetBusTrim = 1.0f / std::sqrt(static_cast<float>(std::max(1, activeVoiceCount)));
    const float trimAlpha = static_cast<float>(1.0 - std::exp(-static_cast<double>(numSamples) / (0.025 * fs)));
    if (firstBlockAfterReset)
        smoothedBusTrim = targetBusTrim;
    else
        smoothedBusTrim += trimAlpha * (targetBusTrim - smoothedBusTrim);

    for (int i = 0; i < numSamples; ++i)
    {
        wetBusL[static_cast<size_t>(i)] *= smoothedBusTrim;
        wetBusR[static_cast<size_t>(i)] *= smoothedBusTrim;
    }

    harmonyBusFx.process(wetBusL.data(), wetBusR.data(), numSamples,
                         globalParams.deEssAmount, globalParams.spaceAmount);

    const float targetMix = std::clamp(globalParams.mix, 0.0f, 1.0f);
    const float targetOutGain = dbToGain(std::clamp(globalParams.outputGainDb, -24.0f, 12.0f));

    if (firstBlockAfterReset)
    {
        smoothedMix = targetMix;
        smoothedOutGain = targetOutGain;
        smoothedTuneBlend = targetTuneBlend;
        firstBlockAfterReset = false;
    }

    double outRmsAcc = 0.0;
    float peakL = 0.0f;
    float peakR = 0.0f;

    for (int i = 0; i < numSamples; ++i)
    {
        smoothedMix += paramSmoothAlpha * (targetMix - smoothedMix);
        smoothedOutGain += paramSmoothAlpha * (targetOutGain - smoothedOutGain);
        smoothedTuneBlend += paramSmoothAlpha * (targetTuneBlend - smoothedTuneBlend);

        float switchGain = 1.0f;
        if (switchState == SwitchState::FadingOut)
        {
            switchGain = 1.0f - static_cast<float>(fadePos) / static_cast<float>(fadeLengthSamples);
            ++fadePos;
        }
        else if (switchState == SwitchState::FadingIn)
        {
            switchGain = static_cast<float>(fadePos) / static_cast<float>(fadeLengthSamples);
            ++fadePos;
        }

        // Read time-aligned dry sample matching exact current engine latency
        const int dryDelayTap = std::max(0, activeLatencySamples + (numSamples - 1 - i));
        const float rawDryL = dryDelayL.readInt(dryDelayTap);
        const float rawDryR = dryDelayR.readInt(dryDelayTap);

        const float tunedSample = runInputTuner ? tunedLeadBuf[static_cast<size_t>(i)] : rawDryL;
        const float leadL = rawDryL + smoothedTuneBlend * (tunedSample - rawDryL);
        const float leadR = rawDryR + smoothedTuneBlend * (tunedSample - rawDryR);

        const float dryWeight = 1.0f - smoothedMix;
        const float wetWeight = smoothedMix;

        const float outL = softCeiling((dryWeight * leadL + wetWeight * wetBusL[static_cast<size_t>(i)]) * smoothedOutGain * switchGain);
        const float outR = softCeiling((dryWeight * leadR + wetWeight * wetBusR[static_cast<size_t>(i)]) * smoothedOutGain * switchGain);

        if (numOutChannels >= 2)
        {
            outChannels[0][outOffset + i] = outL;
            outChannels[1][outOffset + i] = outR;
        }
        else
        {
            outChannels[0][outOffset + i] = 0.5f * (outL + outR);
        }

        peakL = std::max(peakL, std::abs(outL));
        peakR = std::max(peakR, std::abs(outR));
        outRmsAcc += 0.5 * (static_cast<double>(outL) * outL + static_cast<double>(outR) * outR);
    }

    const float inRms = static_cast<float>(std::sqrt(inRmsAcc / static_cast<double>(numSamples)));
    const float outRms = static_cast<float>(std::sqrt(outRmsAcc / static_cast<double>(numSamples)));

    telemetry.inputRms.store(inRms, std::memory_order_relaxed);
    telemetry.outputRms.store(outRms, std::memory_order_relaxed);
    telemetry.outputPeakL.store(peakL, std::memory_order_relaxed);
    telemetry.outputPeakR.store(peakR, std::memory_order_relaxed);
    telemetry.detectedHz.store(pitchEst.isVoiced ? pitchEst.frequencyHz : 0.0f, std::memory_order_relaxed);
    telemetry.detectedMidi.store(pitchEst.midiNote, std::memory_order_relaxed);
    telemetry.voicedClarity.store(pitchEst.clarity, std::memory_order_relaxed);
    telemetry.isVoiced.store(pitchEst.isVoiced, std::memory_order_relaxed);
    telemetry.activeLatencySamples.store(activeLatencySamples, std::memory_order_relaxed);
    telemetry.activeMode.store(static_cast<int>(activeMode), std::memory_order_relaxed);
    telemetry.activeHarmonyMode.store(static_cast<int>(globalParams.harmonyMode), std::memory_order_relaxed);
    telemetry.inputTuneCorrectionCents.store(smoothedCorrectionSt * 100.0f, std::memory_order_relaxed);
}

} // namespace harmonizer::dsp
