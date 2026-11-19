#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>

#include "Dsp/ScaleHarmonizer.h"

#include <cmath>

using harmonizer::dsp::ScaleHarmonizer;
using harmonizer::dsp::ScaleType;

TEST_CASE("ScaleHarmonizer computes exact diatonic intervals across scales", "[ScaleHarmonizer]")
{
    ScaleHarmonizer sh;

    SECTION("C Major intervals from C4 (MIDI 60)")
    {
        REQUIRE(sh.computeShiftSemitones(60.0f, true, 0, ScaleType::Major, 0, 0.0f) == Catch::Approx(0.0f));
        REQUIRE(sh.computeShiftSemitones(60.0f, true, 0, ScaleType::Major, +2, 0.0f) == Catch::Approx(4.0f));  // E4 (+3rd)
        REQUIRE(sh.computeShiftSemitones(60.0f, true, 0, ScaleType::Major, +4, 0.0f) == Catch::Approx(7.0f));  // G4 (+5th)
        REQUIRE(sh.computeShiftSemitones(60.0f, true, 0, ScaleType::Major, +7, 0.0f) == Catch::Approx(12.0f)); // C5 (+8ve)
        REQUIRE(sh.computeShiftSemitones(60.0f, true, 0, ScaleType::Major, -2, 0.0f) == Catch::Approx(-3.0f)); // A3 (-3rd)
        REQUIRE(sh.computeShiftSemitones(60.0f, true, 0, ScaleType::Major, -7, 0.0f) == Catch::Approx(-12.0f));// C3 (-8ve)
    }

    SECTION("C Major intervals from D4 (MIDI 62) gives minor 3rd (+3 st)")
    {
        sh.reset();
        REQUIRE(sh.computeShiftSemitones(62.0f, true, 0, ScaleType::Major, +2, 0.0f) == Catch::Approx(3.0f)); // F4
    }

    SECTION("A Natural Minor from A3 (MIDI 57)")
    {
        sh.reset();
        REQUIRE(sh.computeShiftSemitones(57.0f, true, 9, ScaleType::NaturalMinor, +2, 0.0f) == Catch::Approx(3.0f)); // C4
        REQUIRE(sh.computeShiftSemitones(57.0f, true, 9, ScaleType::NaturalMinor, +4, 0.0f) == Catch::Approx(7.0f)); // E4
    }

    SECTION("D Dorian and G Mixolydian")
    {
        sh.reset();
        // D Dorian: D E F G A B C -> +5 steps (6th) from D4 (62) is B4 (+9 st)
        REQUIRE(sh.computeShiftSemitones(62.0f, true, 2, ScaleType::Dorian, +5, 0.0f) == Catch::Approx(9.0f));

        sh.reset();
        // G Mixolydian: G A B C D E F -> +6 steps (7th) from G4 (67) is F5 (+10 st)
        REQUIRE(sh.computeShiftSemitones(67.0f, true, 7, ScaleType::Mixolydian, +6, 0.0f) == Catch::Approx(10.0f));
    }

    SECTION("Pentatonic and Chromatic + Detune")
    {
        sh.reset();
        // C Major Pentatonic: C D E G A -> +3 steps from C4 is G4 (+7 st)
        REQUIRE(sh.computeShiftSemitones(60.0f, true, 0, ScaleType::MajorPentatonic, +3, 0.0f) == Catch::Approx(7.0f));
        REQUIRE(sh.computeShiftSemitones(60.0f, true, 0, ScaleType::MajorPentatonic, +5, 0.0f) == Catch::Approx(12.0f));

        sh.reset();
        REQUIRE(sh.computeShiftSemitones(60.0f, true, 0, ScaleType::Chromatic, +4, 15.0f) == Catch::Approx(4.15f));
    }
}

TEST_CASE("ScaleHarmonizer hysteresis keeps intervals stable under +/- 35 cent vibrato", "[ScaleHarmonizer]")
{
    ScaleHarmonizer sh;
    // Lock onto C4 (60.0)
    REQUIRE(sh.computeShiftSemitones(60.0f, true, 0, ScaleType::Major, +2, 0.0f) == Catch::Approx(4.0f));

    for (int i = 0; i < 200; ++i)
    {
        const float vibrato = 0.35f * std::sin(static_cast<float>(i) * 0.25f);
        const float shift = sh.computeShiftSemitones(60.0f + vibrato, true, 0, ScaleType::Major, +2, 0.0f);
        REQUIRE(shift == Catch::Approx(4.0f));
    }

    // Moving clearly to D4 (62.0) updates the lock to D4 -> +3rd = F4 (+3 st)
    const float nextShift = sh.computeShiftSemitones(62.0f, true, 0, ScaleType::Major, +2, 0.0f);
    REQUIRE(nextShift == Catch::Approx(3.0f));
}
