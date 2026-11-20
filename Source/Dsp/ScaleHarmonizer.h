#pragma once

#include <string>

namespace harmonizer::dsp
{

enum class ScaleType : int
{
    Major = 0,
    NaturalMinor,
    HarmonicMinor,
    MelodicMinor,
    Dorian,
    Mixolydian,
    MajorPentatonic,
    MinorPentatonic,
    Chromatic,
    NumScales
};

class ScaleHarmonizer
{
public:
    void reset() noexcept;

    float computeShiftSemitones(float detectedMidi,
                                bool isVoiced,
                                int key,
                                ScaleType scale,
                                int intervalSteps,
                                float detuneCents) noexcept;

    int getLockedBaseMidi() const noexcept { return lockedBaseMidi; }

    static float quantizeToScaleMidi(float detectedMidi, int key, ScaleType scale) noexcept;
    static std::string formatIntervalLabel(int intervalSteps, ScaleType scale);
    static const char* getKeyName(int keyIndex) noexcept;
    static const char* getScaleName(int scaleIndex) noexcept;

private:
    static int floorDiv(int a, int b) noexcept;

    int lockedBaseMidi = 60;
    bool hasLock = false;
};

} // namespace harmonizer::dsp
