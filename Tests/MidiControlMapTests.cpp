#include <catch2/catch_test_macros.hpp>

#include "Dsp/MidiControlMap.h"

using namespace harmonizer::dsp;

TEST_CASE("MidiControlMap provides default Program Change bindings for Snapshots A-D", "[MidiControlMap]")
{
    for (int i = 0; i < 4; ++i)
    {
        const auto target = static_cast<LiveMidiTarget>(i);
        const auto b = makeDefaultBinding(target);
        REQUIRE(isValidBinding(b));
        REQUIRE(b.type == MidiBindType::ProgramChange);
        REQUIRE(b.channel == 0);
        REQUIRE(b.number == i);
        REQUIRE(formatMidiBindingAscii(b) == "PC " + std::to_string(i + 1));
    }

    const auto bypassDef = makeDefaultBinding(LiveMidiTarget::Bypass);
    REQUIRE_FALSE(isValidBinding(bypassDef));
    REQUIRE(formatMidiBindingAscii(bypassDef) == "Unassigned");
}

TEST_CASE("MidiControlMap packs, unpacks, and matches PC/CC/Note events cleanly", "[MidiControlMap]")
{
    const MidiBinding cases[] = {
        { MidiBindType::ProgramChange, 0, 5 },
        { MidiBindType::ControlChange, 2, 85 },
        { MidiBindType::Note, 16, 24 },
    };

    for (const auto& orig : cases)
    {
        const uint32_t packed = packMidiBinding(orig);
        const MidiBinding round = unpackMidiBinding(packed);
        REQUIRE(round.type == orig.type);
        REQUIRE(round.channel == orig.channel);
        REQUIRE(round.number == orig.number);
    }

    REQUIRE(packMidiBinding(MidiBinding{}) == 0u);
    REQUIRE_FALSE(isValidBinding(unpackMidiBinding(0u)));

    // Program Change (Omni)
    const MidiBinding pcB{ MidiBindType::ProgramChange, 0, 1 };
    REQUIRE(bindingMatchesEvent(pcB, MidiBindType::ProgramChange, 7, 1, 127));
    REQUIRE_FALSE(bindingMatchesEvent(pcB, MidiBindType::ProgramChange, 7, 2, 127));
    REQUIRE_FALSE(bindingMatchesEvent(pcB, MidiBindType::ControlChange, 7, 1, 127));

    // Control Change requires value >= 64 and matching channel
    const MidiBinding ccB{ MidiBindType::ControlChange, 2, 85 };
    REQUIRE(bindingMatchesEvent(ccB, MidiBindType::ControlChange, 2, 85, 64));
    REQUIRE(bindingMatchesEvent(ccB, MidiBindType::ControlChange, 2, 85, 127));
    REQUIRE_FALSE(bindingMatchesEvent(ccB, MidiBindType::ControlChange, 2, 85, 63));
    REQUIRE_FALSE(bindingMatchesEvent(ccB, MidiBindType::ControlChange, 3, 85, 127));

    // Note requires velocity > 0
    const MidiBinding noteB{ MidiBindType::Note, 0, 24 };
    REQUIRE(bindingMatchesEvent(noteB, MidiBindType::Note, 1, 24, 90));
    REQUIRE_FALSE(bindingMatchesEvent(noteB, MidiBindType::Note, 1, 24, 0));
    REQUIRE_FALSE(bindingMatchesEvent(noteB, MidiBindType::Note, 1, 25, 90));

    // ASCII formatting check (pure ASCII, no non-ASCII middots/dashes)
    REQUIRE(formatMidiBindingAscii(ccB) == "CC 85");
    REQUIRE(formatMidiBindingAscii(noteB) == "Note C1");
    for (const std::string& label : { formatMidiBindingAscii(pcB), formatMidiBindingAscii(ccB), formatMidiBindingAscii(noteB) })
    {
        for (unsigned char c : label)
            REQUIRE(c < 128);
    }
}
