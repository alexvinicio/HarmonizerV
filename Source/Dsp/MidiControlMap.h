#pragma once

#include <algorithm>
#include <array>
#include <cstdint>
#include <string>

namespace harmonizer::dsp
{

enum class LiveMidiTarget : int
{
    SnapshotA = 0,
    SnapshotB = 1,
    SnapshotC = 2,
    SnapshotD = 3,
    Bypass    = 4,
    NumTargets = 5
};

inline constexpr int kNumMidiControlTargets = static_cast<int>(LiveMidiTarget::NumTargets);

enum class MidiBindType : int
{
    None          = 0,
    ProgramChange = 1,
    ControlChange = 2,
    Note          = 3
};

struct MidiBinding
{
    MidiBindType type = MidiBindType::None;
    int channel = 0; // 0 = Omni, 1..16 = specific MIDI channel
    int number = -1; // 0..127
};

inline bool isValidBinding(const MidiBinding& b) noexcept
{
    return b.type != MidiBindType::None
        && b.number >= 0 && b.number <= 127
        && b.channel >= 0 && b.channel <= 16;
}

inline uint32_t packMidiBinding(const MidiBinding& b) noexcept
{
    if (!isValidBinding(b))
        return 0u;
    const uint32_t t = static_cast<uint32_t>(static_cast<int>(b.type)) & 0xFFu;
    const uint32_t c = static_cast<uint32_t>(b.channel) & 0xFFu;
    const uint32_t n = static_cast<uint32_t>(b.number) & 0xFFu;
    return (t << 16) | (c << 8) | n;
}

inline MidiBinding unpackMidiBinding(uint32_t packed) noexcept
{
    if (packed == 0u)
        return MidiBinding{};
    const int rawType = static_cast<int>((packed >> 16) & 0xFFu);
    if (rawType < static_cast<int>(MidiBindType::ProgramChange)
        || rawType > static_cast<int>(MidiBindType::Note))
    {
        return MidiBinding{};
    }
    MidiBinding b;
    b.type = static_cast<MidiBindType>(rawType);
    b.channel = std::clamp(static_cast<int>((packed >> 8) & 0xFFu), 0, 16);
    b.number = std::clamp(static_cast<int>(packed & 0xFFu), 0, 127);
    return b;
}

inline bool bindingMatchesEvent(const MidiBinding& b,
                                MidiBindType evType,
                                int evChannel,
                                int evNumber,
                                int evValue) noexcept
{
    if (!isValidBinding(b) || b.type != evType)
        return false;
    if (b.channel != 0 && b.channel != evChannel)
        return false;
    if (b.number != evNumber)
        return false;

    if (evType == MidiBindType::ControlChange)
        return evValue >= 64;
    if (evType == MidiBindType::Note)
        return evValue > 0;
    return true; // ProgramChange
}

inline MidiBinding makeDefaultBinding(LiveMidiTarget target) noexcept
{
    const int idx = static_cast<int>(target);
    if (idx >= static_cast<int>(LiveMidiTarget::SnapshotA)
        && idx <= static_cast<int>(LiveMidiTarget::SnapshotD))
    {
        return { MidiBindType::ProgramChange, 0, idx };
    }
    return MidiBinding{};
}

inline std::string formatMidiBindingAscii(const MidiBinding& b)
{
    if (!isValidBinding(b))
        return "Unassigned";

    if (b.type == MidiBindType::ProgramChange)
        return "PC " + std::to_string(b.number + 1);

    if (b.type == MidiBindType::ControlChange)
        return "CC " + std::to_string(b.number);

    static constexpr std::array<const char*, 12> kNotes = {
        "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"
    };
    const int pc = b.number % 12;
    const int octave = (b.number / 12) - 1;
    return std::string("Note ") + kNotes[static_cast<size_t>(pc)] + std::to_string(octave);
}

} // namespace harmonizer::dsp
