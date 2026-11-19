#pragma once

#include "Dsp/DspConstants.h"

#include <juce_core/juce_core.h>

namespace harmonizer::params
{

inline constexpr const char* key         = "key";
inline constexpr const char* scale       = "scale";
inline constexpr const char* engineMode  = "engine_mode";
inline constexpr const char* mix         = "mix";
inline constexpr const char* outputGain  = "output_gain";
inline constexpr const char* humanize    = "humanize";
inline constexpr const char* glideMs     = "glide_ms";
inline constexpr const char* naturalness = "natural";
inline constexpr const char* deEssAmount = "deess";
inline constexpr const char* spaceAmount = "space";

inline constexpr const char* vEnabled  = "enabled";
inline constexpr const char* vMute     = "mute";
inline constexpr const char* vSolo     = "solo";
inline constexpr const char* vInterval = "interval";
inline constexpr const char* vDetune   = "detune";
inline constexpr const char* vFormant  = "formant";
inline constexpr const char* vDelayMs  = "delay_ms";
inline constexpr const char* vHpfHz    = "hpf_hz";
inline constexpr const char* vLpfHz    = "lpf_hz";
inline constexpr const char* vPan      = "pan";
inline constexpr const char* vGainDb   = "gain_db";

inline juce::String voiceParamId(int voiceIndex, const char* suffix)
{
    return "v" + juce::String(voiceIndex + 1) + "_" + suffix;
}

} // namespace harmonizer::params
