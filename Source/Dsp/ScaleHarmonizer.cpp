#include "Dsp/ScaleHarmonizer.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdlib>

namespace harmonizer::dsp
{

namespace
{
struct ScaleDef
{
    const char* name;
    int length;
    std::array<int, 12> degrees;
};

constexpr std::array<ScaleDef, static_cast<size_t>(ScaleType::NumScales)> kScales = {{
    { "Major",            7, { 0, 2, 4, 5, 7, 9, 11, 0, 0, 0, 0, 0 } },
    { "Natural Minor",    7, { 0, 2, 3, 5, 7, 8, 10, 0, 0, 0, 0, 0 } },
    { "Harmonic Minor",   7, { 0, 2, 3, 5, 7, 8, 11, 0, 0, 0, 0, 0 } },
    { "Melodic Minor",    7, { 0, 2, 3, 5, 7, 9, 11, 0, 0, 0, 0, 0 } },
    { "Dorian",           7, { 0, 2, 3, 5, 7, 9, 10, 0, 0, 0, 0, 0 } },
    { "Mixolydian",       7, { 0, 2, 4, 5, 7, 9, 10, 0, 0, 0, 0, 0 } },
    { "Major Pentatonic", 5, { 0, 2, 4, 7, 9, 0,  0, 0, 0, 0, 0, 0 } },
    { "Minor Pentatonic", 5, { 0, 3, 5, 7, 10, 0, 0, 0, 0, 0, 0, 0 } },
    { "Chromatic",       12, { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11 } }
}};

constexpr std::array<const char*, 12> kKeyNames = {
    "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"
};
} // namespace

void ScaleHarmonizer::reset() noexcept
{
    lockedBaseMidi = 60;
    hasLock = false;
}

int ScaleHarmonizer::floorDiv(int a, int b) noexcept
{
    int q = a / b;
    int r = a % b;
    if (r != 0 && ((r < 0) != (b < 0)))
        --q;
    return q;
}

float ScaleHarmonizer::computeShiftSemitones(float detectedMidi,
                                             bool isVoiced,
                                             int key,
                                             ScaleType scale,
                                             int intervalSteps,
                                             float detuneCents) noexcept
{
    const float detuneSt = detuneCents * 0.01f;

    // Update hysteresis-locked base note only during confident voiced input
    if (isVoiced)
    {
        if (!hasLock)
        {
            lockedBaseMidi = static_cast<int>(std::lround(detectedMidi));
            hasLock = true;
        }
        else if (std::abs(detectedMidi - static_cast<float>(lockedBaseMidi)) > 0.70f)
        {
            // +/- 70 cents from locked center (~20 cents past midpoint) keeps +/-35..50ct vibrato steady
            lockedBaseMidi = static_cast<int>(std::lround(detectedMidi));
        }
    }

    const int clampedScale = std::clamp(static_cast<int>(scale), 0, static_cast<int>(ScaleType::NumScales) - 1);
    if (clampedScale == static_cast<int>(ScaleType::Chromatic))
        return static_cast<float>(intervalSteps) + detuneSt;

    const auto& def = kScales[static_cast<size_t>(clampedScale)];
    const int wrappedKey = ((key % 12) + 12) % 12;
    const int baseMidi = lockedBaseMidi;

    const int rel = baseMidi - wrappedKey;
    const int approxOct = floorDiv(rel, 12);

    // Search closest scale pitch across adjacent octaves to handle wrap-around accurately
    int bestAbsMidi = baseMidi;
    int bestOct = approxOct;
    int bestDeg = 0;
    int bestDist = 999;

    for (int octOffset = -1; octOffset <= 1; ++octOffset)
    {
        const int candOct = approxOct + octOffset;
        for (int deg = 0; deg < def.length; ++deg)
        {
            const int candMidi = wrappedKey + candOct * 12 + def.degrees[static_cast<size_t>(deg)];
            const int dist = std::abs(candMidi - baseMidi);
            if (dist < bestDist)
            {
                bestDist = dist;
                bestAbsMidi = candMidi;
                bestOct = candOct;
                bestDeg = deg;
            }
        }
    }

    const int totalDeg = bestDeg + intervalSteps;
    const int octDelta = floorDiv(totalDeg, def.length);
    const int targetDeg = totalDeg - octDelta * def.length;
    const int targetMidi = wrappedKey + (bestOct + octDelta) * 12 + def.degrees[static_cast<size_t>(targetDeg)];

    return static_cast<float>(targetMidi - bestAbsMidi) + detuneSt;
}

float ScaleHarmonizer::quantizeToScaleMidi(float detectedMidi, int key, ScaleType scale) noexcept
{
    const int clampedScale = std::clamp(static_cast<int>(scale), 0, static_cast<int>(ScaleType::NumScales) - 1);
    if (clampedScale == static_cast<int>(ScaleType::Chromatic))
        return std::round(detectedMidi);

    const auto& def = kScales[static_cast<size_t>(clampedScale)];
    const int wrappedKey = ((key % 12) + 12) % 12;
    const int approxOct = floorDiv(static_cast<int>(std::lround(detectedMidi)) - wrappedKey, 12);

    float bestMidi = std::round(detectedMidi);
    float bestDist = 999.0f;

    for (int octOffset = -1; octOffset <= 1; ++octOffset)
    {
        const int candOct = approxOct + octOffset;
        for (int deg = 0; deg < def.length; ++deg)
        {
            const float candMidi = static_cast<float>(wrappedKey + candOct * 12 + def.degrees[static_cast<size_t>(deg)]);
            const float dist = std::abs(candMidi - detectedMidi);
            if (dist < bestDist)
            {
                bestDist = dist;
                bestMidi = candMidi;
            }
        }
    }

    return bestMidi;
}

std::string ScaleHarmonizer::formatIntervalLabel(int intervalSteps, ScaleType scale)
{
    if (intervalSteps == 0)
        return "Unison";

    const int clampedScale = std::clamp(static_cast<int>(scale), 0, static_cast<int>(ScaleType::NumScales) - 1);
    const char* sign = (intervalSteps > 0) ? "+" : "-";
    const int absSteps = std::abs(intervalSteps);

    if (clampedScale == static_cast<int>(ScaleType::Chromatic))
        return std::string(sign) + std::to_string(absSteps) + " st";

    const int len = kScales[static_cast<size_t>(clampedScale)].length;
    if (absSteps == len)
        return std::string(sign) + "8ve";
    if (absSteps == 2 * len)
        return std::string(sign) + "2 Oct";

    if (len == 7)
    {
        const int degreeNumber = absSteps + 1;
        switch (degreeNumber)
        {
            case 2:  return std::string(sign) + "2nd";
            case 3:  return std::string(sign) + "3rd";
            default: return std::string(sign) + std::to_string(degreeNumber) + "th";
        }
    }

    return std::string(sign) + std::to_string(absSteps) + " deg";
}

const char* ScaleHarmonizer::getKeyName(int keyIndex) noexcept
{
    const int idx = ((keyIndex % 12) + 12) % 12;
    return kKeyNames[static_cast<size_t>(idx)];
}

const char* ScaleHarmonizer::getScaleName(int scaleIndex) noexcept
{
    const int idx = std::clamp(scaleIndex, 0, static_cast<int>(ScaleType::NumScales) - 1);
    return kScales[static_cast<size_t>(idx)].name;
}

} // namespace harmonizer::dsp
