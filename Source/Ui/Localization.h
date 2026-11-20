#pragma once

#include "Dsp/LocalizationTable.h"

#include <juce_core/juce_core.h>
#include <juce_events/juce_events.h>
#include <atomic>

namespace harmonizer::ui
{

class LocalizationManager final
{
public:
    class Listener
    {
    public:
        virtual ~Listener() = default;
        virtual void languageChanged(dsp::Language newLanguage) = 0;
    };

    static LocalizationManager& getInstance();

    dsp::Language getLanguage() const noexcept;
    void setLanguage(dsp::Language newLang);
    void toggleLanguage();

    void addListener(Listener* listener);
    void removeListener(Listener* listener);

    static juce::File getSettingsFile();

private:
    LocalizationManager();
    void loadFromDisk();
    void saveToDisk(dsp::Language lang) const;

    std::atomic<int> currentLang{ static_cast<int>(dsp::Language::English) };
    juce::ListenerList<Listener> listeners;

    JUCE_DECLARE_NON_COPYABLE(LocalizationManager)
};

inline juce::String tr(dsp::TextKey key)
{
    return juce::String::fromUTF8(dsp::lookupUtf8(LocalizationManager::getInstance().getLanguage(), key));
}

inline juce::String trFormat(dsp::TextKey key, const juce::String& arg0)
{
    return tr(key).replace("{0}", arg0);
}

inline juce::String trScale(int scaleIndex)
{
    return juce::String::fromUTF8(dsp::lookupScaleNameUtf8(LocalizationManager::getInstance().getLanguage(), scaleIndex));
}

inline juce::String trVoicing(int voicingIndex)
{
    return juce::String::fromUTF8(dsp::lookupVoicingNameUtf8(LocalizationManager::getInstance().getLanguage(), voicingIndex));
}

} // namespace harmonizer::ui
