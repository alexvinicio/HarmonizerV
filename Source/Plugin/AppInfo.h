#pragma once

namespace harmonizer::appinfo
{

inline constexpr const char* kAppName = "HarmonizerV";
inline constexpr const char* kCompanyName = "HarmonizerV Audio";

#ifdef JucePlugin_VersionString
inline constexpr const char* kVersionString = JucePlugin_VersionString;
#else
inline constexpr const char* kVersionString = "2.6.0";
#endif

inline constexpr const char* kBuyMeACoffeeUrl = "https://buymeacoffee.com/alexvinicio";
inline constexpr const char* kBuyMeACoffeeText = "buymeacoffee.com/alexvinicio";
inline constexpr bool kSupportLinkFinalized = true;

} // namespace harmonizer::appinfo
