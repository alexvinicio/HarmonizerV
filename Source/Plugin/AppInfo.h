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

// TODO before packaging release: replace placeholder URL/text with final buymeacoffee.com/<handle> and contact info.
inline constexpr const char* kBuyMeACoffeeUrl = "https://buymeacoffee.com/";
inline constexpr const char* kBuyMeACoffeeText = "buymeacoffee.com";
inline constexpr bool kSupportLinkFinalized = false;

} // namespace harmonizer::appinfo
