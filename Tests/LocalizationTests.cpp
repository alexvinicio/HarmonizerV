#include <catch2/catch_test_macros.hpp>

#include "Dsp/LocalizationTable.h"

#include <cstring>
#include <string>

using namespace harmonizer::dsp;

namespace
{
bool isValidUtf8NoMojibake(const char* s)
{
    if (s == nullptr || s[0] == '\0')
        return false;

    const auto* bytes = reinterpret_cast<const unsigned char*>(s);
    const size_t len = std::strlen(s);

    for (size_t i = 0; i < len;)
    {
        const unsigned char b = bytes[i];
        // Reject UTF-8 double-encoded mojibake prefix C3 83 (e.g. "Ã...")
        if (b == 0xC3u && (i + 1) < len && bytes[i + 1] == 0x83u)
            return false;

        if (b < 0x80u)
        {
            ++i;
        }
        else if ((b & 0xE0u) == 0xC0u)
        {
            if (i + 1 >= len || (bytes[i + 1] & 0xC0u) != 0x80u || b < 0xC2u)
                return false;
            i += 2;
        }
        else if ((b & 0xF0u) == 0xE0u)
        {
            if (i + 2 >= len || (bytes[i + 1] & 0xC0u) != 0x80u || (bytes[i + 2] & 0xC0u) != 0x80u)
                return false;
            i += 3;
        }
        else if ((b & 0xF8u) == 0xF0u)
        {
            if (i + 3 >= len || (bytes[i + 1] & 0xC0u) != 0x80u
                || (bytes[i + 2] & 0xC0u) != 0x80u || (bytes[i + 3] & 0xC0u) != 0x80u)
                return false;
            i += 4;
        }
        else
        {
            return false;
        }
    }

    return true;
}
} // namespace

TEST_CASE("LocalizationTable provides valid UTF-8 EN and ES strings and consistent placeholders", "[Localization]")
{
    for (int k = 0; k < kNumTextKeys; ++k)
    {
        const auto key = static_cast<TextKey>(k);
        const char* en = lookupUtf8(Language::English, key);
        const char* es = lookupUtf8(Language::Spanish, key);

        INFO("TextKey index=" << k << " en='" << (en ? en : "null") << "' es='" << (es ? es : "null") << "'");
        REQUIRE(isValidUtf8NoMojibake(en));
        REQUIRE(isValidUtf8NoMojibake(es));

        const bool enHasArg = (std::string(en).find("{0}") != std::string::npos);
        const bool esHasArg = (std::string(es).find("{0}") != std::string::npos);
        REQUIRE(enHasArg == esHasArg);
    }

    for (int s = 0; s < static_cast<int>(kScaleNames.size()); ++s)
    {
        REQUIRE(isValidUtf8NoMojibake(lookupScaleNameUtf8(Language::English, s)));
        REQUIRE(isValidUtf8NoMojibake(lookupScaleNameUtf8(Language::Spanish, s)));
    }

    for (int v = 0; v < static_cast<int>(kVoicingNames.size()); ++v)
    {
        REQUIRE(isValidUtf8NoMojibake(lookupVoicingNameUtf8(Language::English, v)));
        REQUIRE(isValidUtf8NoMojibake(lookupVoicingNameUtf8(Language::Spanish, v)));
    }

    REQUIRE(std::string(languageCode(Language::English)) == "en");
    REQUIRE(std::string(languageCode(Language::Spanish)) == "es");

    REQUIRE(languageFromCode("es") == Language::Spanish);
    REQUIRE(languageFromCode("es-CO") == Language::Spanish);
    REQUIRE(languageFromCode("ES_MX") == Language::Spanish);
    REQUIRE(languageFromCode("en") == Language::English);
    REQUIRE(languageFromCode("en-US") == Language::English);
    REQUIRE(languageFromCode("fr") == Language::English);
    REQUIRE(languageFromCode("") == Language::English);
    REQUIRE(languageFromCode(nullptr) == Language::English);
}
