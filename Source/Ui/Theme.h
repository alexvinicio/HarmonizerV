#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include <array>

namespace harmonizer::ui
{

namespace Theme
{
inline const juce::Colour bgCanvas      { 0xFF111318 };
inline const juce::Colour bgSurface     { 0xFF181C24 };
inline const juce::Colour bgElevated    { 0xFF202531 };
inline const juce::Colour borderSubtle  { 0xFF262C3A };
inline const juce::Colour borderFocus   { 0xFF3A4358 };

inline const juce::Colour textPrimary   { 0xFFF1F5F9 };
inline const juce::Colour textSecondary { 0xFF94A3B8 };
inline const juce::Colour textMuted     { 0xFF64748B };

inline const juce::Colour liveAccent    { 0xFF10B981 };
inline const juce::Colour studioAccent  { 0xFF8B5CF6 };
inline const juce::Colour warnAccent    { 0xFFF59E0B };
inline const juce::Colour muteAccent    { 0xFFEF4444 };

inline const std::array<juce::Colour, 8> voiceColours = {
    juce::Colour{ 0xFF38BDF8 }, // V1 Cyan
    juce::Colour{ 0xFFA78BFA }, // V2 Lavender
    juce::Colour{ 0xFFFBBF24 }, // V3 Amber
    juce::Colour{ 0xFFFB7185 }, // V4 Coral
    juce::Colour{ 0xFF34D399 }, // V5 Mint
    juce::Colour{ 0xFF60A5FA }, // V6 Sky
    juce::Colour{ 0xFFF472B6 }, // V7 Pink
    juce::Colour{ 0xFFFB923C }  // V8 Tangerine
};

inline juce::Colour getVoiceColour(int index) noexcept
{
    if (index < 0 || index >= static_cast<int>(voiceColours.size()))
        return voiceColours[0];
    return voiceColours[static_cast<size_t>(index)];
}

inline juce::Font makeFont(float height, bool bold = false)
{
    auto opts = juce::FontOptions(height);
    if (bold)
        opts = opts.withStyle("Bold");
    return juce::Font(opts);
}

inline constexpr float kCardRadius = 10.0f;
inline constexpr float kPillRadius = 6.0f;
} // namespace Theme

} // namespace harmonizer::ui
