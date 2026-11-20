#pragma once

#include "Dsp/DspConstants.h"

#include <array>
#include <cstdint>

namespace harmonizer::dsp
{

struct MidiVoiceSlot
{
    int midiNote = -1;
    bool keyHeld = false;
    bool sustained = false;
    float velocity = 1.0f;
    float gateEnvelope = 0.0f;
    uint64_t ageCounter = 0;

    bool isActive() const noexcept { return midiNote >= 0 && (keyHeld || sustained); }
};

class MidiHarmonyController
{
public:
    void prepare(double sampleRate, int maxActiveSlots = kExposedVoicesV1) noexcept;
    void reset() noexcept;

    void noteOn(int midiNote, float velocity) noexcept;
    void noteOff(int midiNote) noexcept;
    void setSustainPedal(bool down) noexcept;
    void setPitchBendSemitones(float semitones) noexcept;
    void allNotesOff() noexcept;

    void advanceBlock(int numSamples) noexcept;

    const MidiVoiceSlot& getSlot(int index) const noexcept;
    int getActiveSlotCount() const noexcept { return activeSlots; }
    float getPitchBendSemitones() const noexcept { return pitchBendSemitones; }
    bool hasAnyActiveNotes() const noexcept;

private:
    std::array<MidiVoiceSlot, kMaxVoices> slots{};
    double fs = 48000.0;
    int activeSlots = kExposedVoicesV1;
    bool sustainDown = false;
    float pitchBendSemitones = 0.0f;
    uint64_t nextAge = 1;
};

} // namespace harmonizer::dsp
