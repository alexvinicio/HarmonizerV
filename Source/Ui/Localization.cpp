#include "Ui/Localization.h"

namespace harmonizer::ui
{

LocalizationManager& LocalizationManager::getInstance()
{
    static LocalizationManager instance;
    return instance;
}

LocalizationManager::LocalizationManager()
{
    loadFromDisk();
}

juce::File LocalizationManager::getSettingsFile()
{
    auto dir = juce::File::getSpecialLocation(juce::File::userApplicationDataDirectory)
                   .getChildFile("HarmonizerV");
    if (!dir.exists())
        dir.createDirectory();
    return dir.getChildFile("settings.xml");
}

void LocalizationManager::loadFromDisk()
{
    const auto file = getSettingsFile();
    if (file.existsAsFile())
    {
        if (auto xml = juce::XmlDocument::parse(file))
        {
            const auto code = xml->getStringAttribute("language", "");
            if (code.isNotEmpty())
            {
                const auto parsed = dsp::languageFromCode(code.toRawUTF8());
                currentLang.store(static_cast<int>(parsed), std::memory_order_release);
                return;
            }
        }
    }

    // Auto-detect OS user language on first launch
    const auto osLang = juce::SystemStats::getUserLanguage();
    const auto detected = dsp::languageFromCode(osLang.toRawUTF8());
    currentLang.store(static_cast<int>(detected), std::memory_order_release);
}

void LocalizationManager::saveToDisk(dsp::Language lang) const
{
    const auto file = getSettingsFile();
    juce::XmlElement xml("HarmonizerVSettings");
    xml.setAttribute("language", dsp::languageCode(lang));
    xml.writeTo(file);
}

dsp::Language LocalizationManager::getLanguage() const noexcept
{
    return static_cast<dsp::Language>(currentLang.load(std::memory_order_acquire));
}

void LocalizationManager::setLanguage(dsp::Language newLang)
{
    const int target = static_cast<int>(newLang);
    const int prev = currentLang.exchange(target, std::memory_order_acq_rel);
    if (prev == target)
        return;

    saveToDisk(newLang);
    listeners.call([newLang](Listener& l) { l.languageChanged(newLang); });
}

void LocalizationManager::toggleLanguage()
{
    const auto cur = getLanguage();
    setLanguage(cur == dsp::Language::Spanish ? dsp::Language::English : dsp::Language::Spanish);
}

void LocalizationManager::addListener(Listener* listener)
{
    if (listener != nullptr)
        listeners.add(listener);
}

void LocalizationManager::removeListener(Listener* listener)
{
    listeners.remove(listener);
}

} // namespace harmonizer::ui
