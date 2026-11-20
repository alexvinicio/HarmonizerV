#include "Dsp/MidiHarmonyController.h"

#include <algorithm>
#include <cmath>

namespace harmonizer::dsp
{

void MidiHarmonyController::prepare(double sampleRate, int maxActiveSlots) noexcept
{
    fs = std::max(8000.0, sampleRate);
    activeSlots = std::clamp(maxActiveSlots, 1, kMaxVoices);
    reset();
}

void MidiHarmonyController::reset() noexcept
{
    for (auto& s : slots)
        s = MidiVoiceSlot{};
    sustainDown = false;
    pitchBendSemitones = 0.0f;
    nextAge = 1;
}

void MidiHarmonyController::setActiveSlotCount(int maxActiveSlots) noexcept
{
    const int newSlots = std::clamp(maxActiveSlots, 1, kMaxVoices);
    if (newSlots < activeSlots)
    {
        for (int i = newSlots; i < activeSlots; ++i)
        {
            slots[static_cast<size_t>(i)].keyHeld = false;
            slots[static_cast<size_t>(i)].sustained = false;
        }
    }
    activeSlots = newSlots;
}

void MidiHarmonyController::noteOn(int midiNote, float velocity) noexcept
{
    if (midiNote < 0 || midiNote > 127)
        return;

    const float vel = std::clamp(velocity, 0.05f, 1.0f);

    // 1) Reuse existing slot already assigned to this MIDI note
    for (int i = 0; i < activeSlots; ++i)
    {
        auto& s = slots[static_cast<size_t>(i)];
        if (s.midiNote == midiNote)
        {
            s.keyHeld = true;
            s.sustained = false;
            s.velocity = vel;
            s.ageCounter = nextAge++;
            return;
        }
    }

    // 2) Claim completely idle slot
    for (int i = 0; i < activeSlots; ++i)
    {
        auto& s = slots[static_cast<size_t>(i)];
        if (s.midiNote < 0 || (!s.isActive() && s.gateEnvelope <= 0.002f))
        {
            s.midiNote = midiNote;
            s.keyHeld = true;
            s.sustained = false;
            s.velocity = vel;
            s.ageCounter = nextAge++;
            return;
        }
    }

    // 3) Steal oldest released slot first, otherwise steal oldest active slot
    int bestSlot = 0;
    bool bestIsInactive = false;
    uint64_t oldestAge = UINT64_MAX;

    for (int i = 0; i < activeSlots; ++i)
    {
        const auto& s = slots[static_cast<size_t>(i)];
        const bool inactive = !s.isActive();
        if ((inactive && !bestIsInactive) || (inactive == bestIsInactive && s.ageCounter < oldestAge))
        {
            bestIsInactive = inactive;
            oldestAge = s.ageCounter;
            bestSlot = i;
        }
    }

    auto& chosen = slots[static_cast<size_t>(bestSlot)];
    chosen.midiNote = midiNote;
    chosen.keyHeld = true;
    chosen.sustained = false;
    chosen.velocity = vel;
    chosen.ageCounter = nextAge++;
}

void MidiHarmonyController::noteOff(int midiNote) noexcept
{
    for (int i = 0; i < activeSlots; ++i)
    {
        auto& s = slots[static_cast<size_t>(i)];
        if (s.midiNote == midiNote)
        {
            s.keyHeld = false;
            s.sustained = sustainDown;
        }
    }
}

void MidiHarmonyController::setSustainPedal(bool down) noexcept
{
    sustainDown = down;
    if (!down)
    {
        for (int i = 0; i < activeSlots; ++i)
        {
            auto& s = slots[static_cast<size_t>(i)];
            if (!s.keyHeld)
                s.sustained = false;
        }
    }
}

void MidiHarmonyController::setPitchBendSemitones(float semitones) noexcept
{
    pitchBendSemitones = std::clamp(semitones, -12.0f, 12.0f);
}

void MidiHarmonyController::allNotesOff() noexcept
{
    sustainDown = false;
    for (int i = 0; i < activeSlots; ++i)
    {
        slots[static_cast<size_t>(i)].keyHeld = false;
        slots[static_cast<size_t>(i)].sustained = false;
    }
}

void MidiHarmonyController::advanceBlock(int numSamples) noexcept
{
    if (numSamples <= 0)
        return;

    const float attackAlpha = static_cast<float>(1.0 - std::exp(-static_cast<double>(numSamples) / (0.010 * fs)));
    const float releaseAlpha = static_cast<float>(1.0 - std::exp(-static_cast<double>(numSamples) / (0.065 * fs)));

    for (int i = 0; i < kMaxVoices; ++i)
    {
        auto& s = slots[static_cast<size_t>(i)];
        const bool inPool = (i < activeSlots);
        const float target = (inPool && s.isActive()) ? 1.0f : 0.0f;
        const float alpha = (target > s.gateEnvelope) ? attackAlpha : releaseAlpha;
        s.gateEnvelope += alpha * (target - s.gateEnvelope);

        if ((!inPool || !s.isActive()) && s.gateEnvelope < 0.0005f)
            s.gateEnvelope = 0.0f;
    }
}

const MidiVoiceSlot& MidiHarmonyController::getSlot(int index) const noexcept
{
    const int clamped = std::clamp(index, 0, kMaxVoices - 1);
    return slots[static_cast<size_t>(clamped)];
}

bool MidiHarmonyController::hasAnyActiveNotes() const noexcept
{
    for (int i = 0; i < activeSlots; ++i)
    {
        if (slots[static_cast<size_t>(i)].isActive())
            return true;
    }
    return false;
}

} // namespace harmonizer::dsp
