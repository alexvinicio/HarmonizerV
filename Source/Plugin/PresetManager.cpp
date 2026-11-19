#include "Plugin/PresetManager.h"
#include "Plugin/ParameterIds.h"

namespace harmonizer
{

namespace
{
struct VoicePresetSpec
{
    bool enabled = false;
    int interval = 0;
    float detune = 0.0f;
    float formant = 0.0f;
    float delayMs = 0.0f;
    float hpfHz = 110.0f;
    float lpfHz = 14000.0f;
    float panPct = 0.0f;
    float gainDb = -6.0f;
};

struct FactoryPresetSpec
{
    const char* name;
    float mixPct;
    float humanizePct;
    bool forceLive;
    std::array<VoicePresetSpec, dsp::kExposedVoicesV1> voices;
};

constexpr std::array<FactoryPresetSpec, 7> kFactoryPresets = {{
    {
        "Natural 3rd & 5th", 40.0f, 20.0f, false,
        {{
            { true,  +2,  0.0f,  0.0f,  4.0f, 110.0f, 14000.0f, -35.0f, -5.0f },
            { true,  +4,  0.0f,  0.0f,  9.0f, 130.0f, 13500.0f, +35.0f, -6.0f },
            { false, -2,  0.0f,  0.0f, 12.0f, 100.0f, 14000.0f, -65.0f, -8.0f },
            { false, +7,  0.0f,  0.0f, 16.0f, 160.0f, 15000.0f, +65.0f, -8.0f }
        }}
    },
    {
        "Pop Choir (4V)", 50.0f, 28.0f, false,
        {{
            { true,  -2, -4.0f, -1.0f,  5.0f, 110.0f, 13000.0f, -50.0f, -6.0f },
            { true,  +2, +4.0f, +0.5f,  8.0f, 130.0f, 14000.0f, +50.0f, -5.0f },
            { true,  +4, -3.0f,  0.0f, 14.0f, 150.0f, 12500.0f, -80.0f, -7.5f },
            { true,  +7, +3.0f, +2.0f, 19.0f, 220.0f, 15500.0f, +80.0f, -9.0f }
        }}
    },
    {
        "Wide Octave Thickener", 42.0f, 22.0f, false,
        {{
            { true,  -7,  0.0f, -2.0f,  2.0f,  80.0f,  9500.0f,   0.0f, -5.5f },
            { true,  +7, -6.0f, +1.5f, 10.0f, 240.0f, 16000.0f, -65.0f, -8.5f },
            { true,  +7, +6.0f, +1.5f, 18.0f, 240.0f, 16000.0f, +65.0f, -8.5f },
            { false,  0,  0.0f,  0.0f,  0.0f, 110.0f, 14000.0f,   0.0f, -12.0f }
        }}
    },
    {
        "Gospel Stack", 55.0f, 30.0f, false,
        {{
            { true,  -4, -3.0f, -1.5f,  4.0f,  95.0f, 12000.0f, -40.0f, -5.0f },
            { true,  +2, +3.0f,  0.0f,  9.0f, 130.0f, 13500.0f, +40.0f, -4.5f },
            { true,  +4, -4.0f, +1.0f, 15.0f, 160.0f, 13000.0f, -75.0f, -6.0f },
            { true,  +7, +4.0f, +1.5f, 22.0f, 220.0f, 15000.0f, +75.0f, -7.5f }
        }}
    },
    {
        "Dark Baritone Shadow", 38.0f, 15.0f, false,
        {{
            { true,  -7, -2.0f, -4.0f,  3.0f,  75.0f,  6000.0f, -25.0f, -4.5f },
            { true,  -4, +2.0f, -3.0f, 11.0f,  85.0f,  6500.0f, +25.0f, -6.5f },
            { false, -2,  0.0f, -2.0f, 14.0f, 100.0f,  7000.0f, -55.0f, -10.0f },
            { false,  0,  0.0f,  0.0f,  0.0f, 100.0f, 12000.0f, +55.0f, -12.0f }
        }}
    },
    {
        "Angelic Air", 46.0f, 26.0f, false,
        {{
            { true,  +4, -4.0f, +3.0f, 10.0f, 300.0f, 17500.0f, -55.0f, -7.0f },
            { true,  +7, +4.0f, +5.0f, 16.0f, 400.0f, 18000.0f, +55.0f, -7.5f },
            { true,  +2,  0.0f, +2.0f, 22.0f, 260.0f, 16500.0f,   0.0f, -9.5f },
            { false, +9,  0.0f, +4.0f, 26.0f, 450.0f, 18000.0f, +75.0f, -12.0f }
        }}
    },
    {
        "Live Doubler", 35.0f, 35.0f, true,
        {{
            { true,   0, -12.0f, -0.5f, 11.0f, 120.0f, 13000.0f, -70.0f, -4.5f },
            { true,   0, +12.0f, +0.5f, 17.0f, 120.0f, 13000.0f, +70.0f, -4.5f },
            { false, +2,   0.0f,  0.0f, 20.0f, 140.0f, 14000.0f, -45.0f, -9.0f },
            { false, +4,   0.0f,  0.0f, 24.0f, 160.0f, 14000.0f, +45.0f, -9.0f }
        }}
    }
}};
} // namespace

PresetManager::PresetManager(juce::AudioProcessorValueTreeState& state)
    : apvts(state)
{
    numFactoryPresets = static_cast<int>(kFactoryPresets.size());
    refreshPresets();
}

juce::File PresetManager::getUserPresetDirectory()
{
    auto dir = juce::File::getSpecialLocation(juce::File::userApplicationDataDirectory)
                   .getChildFile("HarmonizerV")
                   .getChildFile("Presets");
    if (!dir.exists())
        dir.createDirectory();
    return dir;
}

void PresetManager::refreshPresets()
{
    presets.clear();
    for (const auto& fp : kFactoryPresets)
        presets.push_back({ fp.name, true, {} });

    const auto userDir = getUserPresetDirectory();
    auto files = userDir.findChildFiles(juce::File::findFiles, false, "*.xml");
    files.sort();
    for (const auto& f : files)
        presets.push_back({ f.getFileNameWithoutExtension(), false, f });

    currentIndex = std::clamp(currentIndex, 0, std::max(0, static_cast<int>(presets.size()) - 1));
}

void PresetManager::setParamValue(const juce::String& id, float plainValue)
{
    if (auto* param = apvts.getParameter(id))
    {
        const float norm = param->convertTo0to1(plainValue);
        param->beginChangeGesture();
        param->setValueNotifyingHost(norm);
        param->endChangeGesture();
    }
}

void PresetManager::applyFactoryPreset(int index)
{
    const auto& spec = kFactoryPresets[static_cast<size_t>(std::clamp(index, 0, numFactoryPresets - 1))];

    setParamValue(params::mix, spec.mixPct);
    setParamValue(params::humanize, spec.humanizePct);
    if (spec.forceLive)
        setParamValue(params::engineMode, 0.0f);

    for (int v = 0; v < dsp::kExposedVoicesV1; ++v)
    {
        const auto& vp = spec.voices[static_cast<size_t>(v)];
        setParamValue(params::voiceParamId(v, params::vEnabled),  vp.enabled ? 1.0f : 0.0f);
        setParamValue(params::voiceParamId(v, params::vMute),     0.0f);
        setParamValue(params::voiceParamId(v, params::vSolo),     0.0f);
        setParamValue(params::voiceParamId(v, params::vInterval), static_cast<float>(vp.interval));
        setParamValue(params::voiceParamId(v, params::vDetune),   vp.detune);
        setParamValue(params::voiceParamId(v, params::vFormant),  vp.formant);
        setParamValue(params::voiceParamId(v, params::vDelayMs),  vp.delayMs);
        setParamValue(params::voiceParamId(v, params::vHpfHz),    vp.hpfHz);
        setParamValue(params::voiceParamId(v, params::vLpfHz),    vp.lpfHz);
        setParamValue(params::voiceParamId(v, params::vPan),      vp.panPct);
        setParamValue(params::voiceParamId(v, params::vGainDb),   vp.gainDb);
    }
}

void PresetManager::loadPreset(int index)
{
    if (index < 0 || index >= static_cast<int>(presets.size()))
        return;

    currentIndex = index;
    const auto& entry = presets[static_cast<size_t>(index)];

    if (entry.isFactory)
    {
        applyFactoryPreset(index);
        return;
    }

    if (entry.file.existsAsFile())
    {
        if (auto xml = juce::XmlDocument::parse(entry.file))
        {
            auto tree = juce::ValueTree::fromXml(*xml);
            if (tree.isValid() && tree.hasType(apvts.state.getType()))
                apvts.replaceState(tree);
        }
    }
}

bool PresetManager::saveUserPreset(const juce::String& name)
{
    const juce::String sanitized = juce::File::createLegalFileName(name.trim());
    if (sanitized.isEmpty())
        return false;

    const auto file = getUserPresetDirectory().getChildFile(sanitized + ".xml");
    auto stateCopy = apvts.copyState();
    std::unique_ptr<juce::XmlElement> xml(stateCopy.createXml());
    if (xml == nullptr || !xml->writeTo(file))
        return false;

    refreshPresets();
    for (int i = numFactoryPresets; i < static_cast<int>(presets.size()); ++i)
    {
        if (presets[static_cast<size_t>(i)].name == sanitized)
        {
            currentIndex = i;
            break;
        }
    }
    return true;
}

} // namespace harmonizer
