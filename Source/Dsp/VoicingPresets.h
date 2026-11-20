#pragma once

#include "Dsp/DspConstants.h"

#include <algorithm>
#include <array>

namespace harmonizer::dsp
{

struct VoicingVoiceSpec
{
    bool enabled = false;
    bool fixedPitch = false;
    int fixedOffsetFromRoot = 0;
    int interval = 0;
    float detune = 0.0f;
    float formant = 0.0f;
    float delayMs = 0.0f;
    float hpfHz = 110.0f;
    float lpfHz = 14000.0f;
    float panPct = 0.0f;
    float gainDb = -6.0f;
    float vibratoPct = 0.0f;
};

struct VoicingSpec
{
    const char* name;
    bool use8Voices;
    std::array<VoicingVoiceSpec, kMaxVoices> voices;
};

inline int computePedalMidiNote(int keyIndex, int semitoneOffset = 0) noexcept
{
    int note = 60 + std::clamp(keyIndex, 0, 11) + semitoneOffset;
    while (note > 72)
        note -= 12;
    while (note < 48)
        note += 12;
    return note;
}

inline constexpr std::array<VoicingSpec, 8> kVoicingPresets = {{
    {
        "Classic Triad (3V)", false,
        {{
            { true,  false, 0, +2,  0.0f,  0.0f,  4.0f, 110.0f, 14000.0f, -40.0f,  -5.0f,  6.0f },
            { true,  false, 0, +4,  0.0f,  0.0f,  9.0f, 130.0f, 13500.0f, +40.0f,  -5.5f,  6.0f },
            { true,  false, 0,  0, +8.0f,  0.0f, 15.0f, 120.0f, 13000.0f,   0.0f,  -9.5f,  0.0f },
            { false, false, 0, +7,  0.0f,  0.0f, 18.0f, 160.0f, 15000.0f, +65.0f,  -9.0f,  0.0f },
            { false, false, 0, -2, -5.0f, -0.5f, 10.0f, 120.0f, 13000.0f, -55.0f,  -8.0f,  8.0f },
            { false, false, 0, +5, +5.0f, +0.5f, 14.0f, 150.0f, 13500.0f, +55.0f,  -8.5f, 10.0f },
            { false, false, 0, -7,  0.0f, -2.0f,  6.0f,  85.0f, 10000.0f, -20.0f,  -9.0f,  0.0f },
            { false, false, 0, +7, +4.0f, +1.5f, 22.0f, 200.0f, 15500.0f, +20.0f, -10.0f, 12.0f }
        }}
    },
    {
        "Open Pop 4-Part (4V)", false,
        {{
            { true,  false, 0, -2, -4.0f, -1.0f,  5.0f, 110.0f, 13000.0f, -50.0f,  -6.0f, 12.0f },
            { true,  false, 0, +2, +4.0f, +0.5f,  8.0f, 130.0f, 14000.0f, +50.0f,  -5.0f, 14.0f },
            { true,  false, 0, +4, -3.0f,  0.0f, 14.0f, 150.0f, 12500.0f, -80.0f,  -7.5f, 16.0f },
            { true,  false, 0, +7, +3.0f, +2.0f, 19.0f, 220.0f, 15500.0f, +80.0f,  -9.0f, 18.0f },
            { false, false, 0, -4, -5.0f, -1.5f,  7.0f, 100.0f, 12000.0f, -65.0f,  -8.0f, 10.0f },
            { false, false, 0, +5, +5.0f, +1.0f, 12.0f, 160.0f, 13500.0f, +65.0f,  -8.5f, 14.0f },
            { false, false, 0, -7,  0.0f, -2.0f,  4.0f,  85.0f,  9500.0f,   0.0f,  -9.0f,  0.0f },
            { false, false, 0, +9,  0.0f, +2.0f, 24.0f, 260.0f, 16000.0f, +75.0f, -11.0f, 15.0f }
        }}
    },
    {
        "Gospel 7th Choir (4V)", false,
        {{
            { true,  false, 0, -4, -3.0f, -1.5f,  4.0f,  95.0f, 12000.0f, -40.0f,  -5.0f, 16.0f },
            { true,  false, 0, +2, +3.0f,  0.0f,  9.0f, 130.0f, 13500.0f, +40.0f,  -4.5f, 18.0f },
            { true,  false, 0, +4, -4.0f, +1.0f, 15.0f, 160.0f, 13000.0f, -75.0f,  -6.0f, 20.0f },
            { true,  false, 0, +6, +4.0f, +1.5f, 21.0f, 200.0f, 14500.0f, +75.0f,  -7.5f, 22.0f },
            { false, false, 0, -7, -2.0f, -2.0f,  3.0f,  80.0f, 10000.0f, -20.0f,  -7.0f, 10.0f },
            { false, false, 0, +7, +5.0f, +2.0f, 17.0f, 220.0f, 15000.0f, +60.0f,  -8.5f, 22.0f },
            { false, false, 0, -2, -6.0f, -0.5f, 11.0f, 110.0f, 12500.0f, -90.0f,  -8.0f, 18.0f },
            { false, false, 0, +9, +6.0f, +2.5f, 25.0f, 260.0f, 15500.0f, +90.0f,  -9.5f, 24.0f }
        }}
    },
    {
        "Power 5ths & Octaves (4V)", false,
        {{
            { true,  false, 0, -7,  0.0f, -2.0f,  2.0f,  80.0f, 10000.0f, -25.0f,  -5.5f,  0.0f },
            { true,  false, 0, -4, -3.0f, -1.0f,  8.0f,  95.0f, 11500.0f, -65.0f,  -6.5f,  8.0f },
            { true,  false, 0, +4, +3.0f, +0.5f, 13.0f, 140.0f, 13500.0f, +65.0f,  -6.5f,  8.0f },
            { true,  false, 0, +7,  0.0f, +1.5f, 18.0f, 220.0f, 16000.0f, +25.0f,  -8.0f,  6.0f },
            { false, false, 0,  0, -9.0f, -0.5f,  6.0f, 120.0f, 13000.0f, -85.0f,  -8.5f,  5.0f },
            { false, false, 0,  0, +9.0f, +0.5f, 16.0f, 120.0f, 13000.0f, +85.0f,  -8.5f,  5.0f },
            { false, false, 0, -3,  0.0f, -1.0f, 11.0f, 100.0f, 11000.0f, -45.0f, -10.0f,  0.0f },
            { false, false, 0, +7, +6.0f, +2.0f, 23.0f, 240.0f, 16500.0f, +45.0f, -10.0f, 10.0f }
        }}
    },
    {
        "Pedal Tonic + Triad (4V)", false,
        {{
            { true,  true,  0,  0,  0.0f,  0.0f,  3.0f, 100.0f, 12500.0f,   0.0f,  -4.5f, 12.0f },
            { true,  false, 0, +2, -4.0f,  0.0f,  8.0f, 130.0f, 13500.0f, -55.0f,  -6.0f, 14.0f },
            { true,  false, 0, +4, +4.0f, +0.5f, 14.0f, 150.0f, 14000.0f, +55.0f,  -6.0f, 14.0f },
            { true,  false, 0, +7,  0.0f, +1.5f, 20.0f, 210.0f, 15500.0f, +20.0f,  -8.5f, 16.0f },
            { false, true, +7, +4,  0.0f, +0.5f, 10.0f, 140.0f, 13000.0f, -35.0f,  -8.0f, 10.0f },
            { false, false, 0, -2, -5.0f, -1.0f,  6.0f, 105.0f, 12000.0f, +75.0f,  -8.0f, 12.0f },
            { false, false, 0, +5, +5.0f, +1.0f, 16.0f, 170.0f, 14500.0f, -80.0f,  -9.0f, 15.0f },
            { false, false, 0, -7,  0.0f, -2.0f, 12.0f,  85.0f,  9500.0f, +80.0f,  -9.5f, 10.0f }
        }}
    },
    {
        "Epic 8-Voice Choir (8V)", true,
        {{
            { true, false, 0, -7, -2.0f, -2.0f,  3.0f,  85.0f, 10500.0f, -25.0f,  -6.0f, 14.0f },
            { true, false, 0, -4, +3.0f, -1.0f,  6.0f, 100.0f, 12000.0f, +25.0f,  -6.5f, 16.0f },
            { true, false, 0, -2, -5.0f, -0.5f,  9.0f, 115.0f, 13000.0f, -60.0f,  -6.0f, 18.0f },
            { true, false, 0, +2, +5.0f, +0.5f, 12.0f, 135.0f, 14000.0f, +60.0f,  -5.5f, 18.0f },
            { true, false, 0, +4, -4.0f, +1.0f, 15.0f, 160.0f, 13500.0f, -85.0f,  -7.0f, 20.0f },
            { true, false, 0, +6, +4.0f, +1.5f, 18.0f, 190.0f, 14500.0f, +85.0f,  -8.0f, 22.0f },
            { true, false, 0, +7, -6.0f, +2.0f, 21.0f, 230.0f, 15500.0f, -45.0f,  -8.5f, 22.0f },
            { true, false, 0, +9, +6.0f, +2.5f, 24.0f, 270.0f, 16500.0f, +45.0f,  -9.5f, 24.0f }
        }}
    },
    {
        "Wide 8V Worship Wall (8V)", true,
        {{
            { true, false, 0, -7, -6.0f, -2.0f,  4.0f,  85.0f, 10500.0f, -55.0f,  -6.5f, 15.0f },
            { true, false, 0, -7, +6.0f, -1.5f,  8.0f,  85.0f, 10500.0f, +55.0f,  -6.5f, 16.0f },
            { true, false, 0, +2, -8.0f,  0.0f, 11.0f, 130.0f, 14000.0f, -80.0f,  -5.5f, 18.0f },
            { true, false, 0, +2, +8.0f, +0.5f, 15.0f, 130.0f, 14000.0f, +80.0f,  -5.5f, 19.0f },
            { true, false, 0, +4, -7.0f, +0.5f, 17.0f, 160.0f, 13500.0f, -40.0f,  -6.5f, 18.0f },
            { true, false, 0, +4, +7.0f, +1.0f, 21.0f, 160.0f, 13500.0f, +40.0f,  -6.5f, 20.0f },
            { true, false, 0, +7, -8.0f, +2.0f, 24.0f, 230.0f, 16000.0f, -95.0f,  -8.5f, 18.0f },
            { true, false, 0, +7, +8.0f, +2.0f, 28.0f, 230.0f, 16000.0f, +95.0f,  -8.5f, 18.0f }
        }}
    },
    {
        "Close Jazz 8V Spread (8V)", true,
        {{
            { true, false, 0, -4, -3.0f, -1.5f,  5.0f, 100.0f, 10500.0f, -70.0f,  -6.5f, 20.0f },
            { true, false, 0, -1, +4.0f, -0.5f,  9.0f, 120.0f, 11000.0f, -30.0f,  -7.5f, 22.0f },
            { true, false, 0, +1, -4.0f,  0.0f, 12.0f, 130.0f, 11500.0f, +30.0f,  -7.5f, 22.0f },
            { true, false, 0, +2, +3.0f, +0.5f, 15.0f, 140.0f, 12000.0f, +70.0f,  -6.0f, 20.0f },
            { true, false, 0, +4, -5.0f, +1.0f, 18.0f, 160.0f, 12000.0f, -50.0f,  -6.5f, 24.0f },
            { true, false, 0, +5, +5.0f, +1.0f, 21.0f, 180.0f, 12500.0f, +50.0f,  -8.0f, 24.0f },
            { true, false, 0, +6, -3.0f, +1.5f, 24.0f, 200.0f, 13000.0f, -88.0f,  -8.5f, 22.0f },
            { true, false, 0, +8, +3.0f, +2.0f, 27.0f, 220.0f, 13500.0f, +88.0f,  -9.0f, 22.0f }
        }}
    }
}};

} // namespace harmonizer::dsp
