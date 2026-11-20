#include "Plugin/PresetManager.h"
#include "Dsp/VoicingPresets.h"
#include "Plugin/ParameterIds.h"
#include "Plugin/SnapshotManager.h"
#include "Plugin/LiveMidiController.h"

namespace harmonizer
{

namespace
{
struct VoicePresetSpec
{
    bool enabled = false;
    bool fixedPitch = false;
    int fixedNote = 60;
    int interval = 0;
    float detune = 0.0f;
    float formant = 0.0f;
    float delayMs = 0.0f;
    float hpfHz = 110.0f;
    float lpfHz = 14000.0f;
    float panPct = 0.0f;
    float gainDb = -6.0f;
    float vibratoPct = 0.0f;
};

struct FactoryPresetSpec
{
    const char* name;
    float mixPct;
    float humanizePct;
    float glideMs;
    float naturalPct;
    float deEssPct;
    float spacePct;
    bool forceLive;
    bool use8Voices;
    std::array<VoicePresetSpec, dsp::kMaxVoices> voices;
};

constexpr std::array<FactoryPresetSpec, 9> kFactoryPresets = {{
    {
        "Natural 3rd & 5th", 40.0f, 20.0f, 28.0f, 35.0f, 35.0f, 18.0f, false, false,
        {{
            { true,  false, 60, +2,  0.0f,  0.0f,  4.0f, 110.0f, 14000.0f, -35.0f, -5.0f,  6.0f },
            { true,  false, 60, +4,  0.0f,  0.0f,  9.0f, 130.0f, 13500.0f, +35.0f, -6.0f,  6.0f },
            { false, false, 60, -2,  0.0f,  0.0f, 12.0f, 100.0f, 14000.0f, -65.0f, -8.0f,  0.0f },
            { false, false, 60, +7,  0.0f,  0.0f, 16.0f, 160.0f, 15000.0f, +65.0f, -8.0f,  0.0f },
            {}, {}, {}, {}
        }}
    },
    {
        "Pop Choir (4V)", 50.0f, 28.0f, 30.0f, 40.0f, 40.0f, 24.0f, false, false,
        {{
            { true,  false, 60, -2, -4.0f, -1.0f,  5.0f, 110.0f, 13000.0f, -50.0f, -6.0f, 12.0f },
            { true,  false, 60, +2, +4.0f, +0.5f,  8.0f, 130.0f, 14000.0f, +50.0f, -5.0f, 14.0f },
            { true,  false, 60, +4, -3.0f,  0.0f, 14.0f, 150.0f, 12500.0f, -80.0f, -7.5f, 16.0f },
            { true,  false, 60, +7, +3.0f, +2.0f, 19.0f, 220.0f, 15500.0f, +80.0f, -9.0f, 18.0f },
            {}, {}, {}, {}
        }}
    },
    {
        "Wide Octave Thickener", 42.0f, 22.0f, 24.0f, 35.0f, 35.0f, 16.0f, false, false,
        {{
            { true,  false, 60, -7,  0.0f, -2.0f,  2.0f,  80.0f,  9500.0f,   0.0f, -5.5f,  0.0f },
            { true,  false, 60, +7, -6.0f, +1.5f, 10.0f, 240.0f, 16000.0f, -65.0f, -8.5f,  8.0f },
            { true,  false, 60, +7, +6.0f, +1.5f, 18.0f, 240.0f, 16000.0f, +65.0f, -8.5f,  8.0f },
            { false, false, 60,  0,  0.0f,  0.0f,  0.0f, 110.0f, 14000.0f,   0.0f, -12.0f, 0.0f },
            {}, {}, {}, {}
        }}
    },
    {
        "Gospel Stack", 55.0f, 30.0f, 32.0f, 45.0f, 42.0f, 28.0f, false, false,
        {{
            { true,  false, 60, -4, -3.0f, -1.5f,  4.0f,  95.0f, 12000.0f, -40.0f, -5.0f, 16.0f },
            { true,  false, 60, +2, +3.0f,  0.0f,  9.0f, 130.0f, 13500.0f, +40.0f, -4.5f, 18.0f },
            { true,  false, 60, +4, -4.0f, +1.0f, 15.0f, 160.0f, 13000.0f, -75.0f, -6.0f, 20.0f },
            { true,  false, 60, +7, +4.0f, +1.5f, 22.0f, 220.0f, 15000.0f, +75.0f, -7.5f, 22.0f },
            {}, {}, {}, {}
        }}
    },
    {
        "Dark Baritone Shadow", 38.0f, 15.0f, 25.0f, 25.0f, 25.0f, 14.0f, false, false,
        {{
            { true,  false, 60, -7, -2.0f, -4.0f,  3.0f,  75.0f,  6000.0f, -25.0f, -4.5f,  0.0f },
            { true,  false, 60, -4, +2.0f, -3.0f, 11.0f,  85.0f,  6500.0f, +25.0f, -6.5f,  6.0f },
            { false, false, 60, -2,  0.0f, -2.0f, 14.0f, 100.0f,  7000.0f, -55.0f, -10.0f, 0.0f },
            { false, false, 60,  0,  0.0f,  0.0f,  0.0f, 100.0f, 12000.0f, +55.0f, -12.0f, 0.0f },
            {}, {}, {}, {}
        }}
    },
    {
        "Angelic Air", 46.0f, 26.0f, 34.0f, 55.0f, 45.0f, 32.0f, false, false,
        {{
            { true,  false, 60, +4, -4.0f, +3.0f, 10.0f, 300.0f, 17500.0f, -55.0f, -7.0f, 18.0f },
            { true,  false, 60, +7, +4.0f, +5.0f, 16.0f, 400.0f, 18000.0f, +55.0f, -7.5f, 20.0f },
            { true,  false, 60, +2,  0.0f, +2.0f, 22.0f, 260.0f, 16500.0f,   0.0f, -9.5f, 14.0f },
            { false, false, 60, +9,  0.0f, +4.0f, 26.0f, 450.0f, 18000.0f, +75.0f, -12.0f, 0.0f },
            {}, {}, {}, {}
        }}
    },
    {
        "Live Doubler", 35.0f, 35.0f, 15.0f, 40.0f, 30.0f, 12.0f, true, false,
        {{
            { true,  false, 60,  0, -12.0f, -0.5f, 11.0f, 120.0f, 13000.0f, -70.0f, -4.5f,  0.0f },
            { true,  false, 60,  0, +12.0f, +0.5f, 17.0f, 120.0f, 13000.0f, +70.0f, -4.5f,  0.0f },
            { false, false, 60, +2,   0.0f,  0.0f, 20.0f, 140.0f, 14000.0f, -45.0f, -9.0f,  0.0f },
            { false, false, 60, +4,   0.0f,  0.0f, 24.0f, 160.0f, 14000.0f, +45.0f, -9.0f,  0.0f },
            {}, {}, {}, {}
        }}
    },
    {
        "Epic 8-Voice Choir", 52.0f, 32.0f, 32.0f, 45.0f, 42.0f, 30.0f, false, true,
        {{
            { true, false, 60, -7, -2.0f, -2.0f,  3.0f,  85.0f, 10500.0f, -25.0f, -6.0f, 14.0f },
            { true, false, 60, -4, +3.0f, -1.0f,  6.0f, 100.0f, 12000.0f, +25.0f, -6.5f, 16.0f },
            { true, false, 60, -2, -5.0f, -0.5f,  9.0f, 115.0f, 13000.0f, -60.0f, -6.0f, 18.0f },
            { true, false, 60, +2, +5.0f, +0.5f, 12.0f, 135.0f, 14000.0f, +60.0f, -5.5f, 18.0f },
            { true, false, 60, +4, -4.0f, +1.0f, 15.0f, 160.0f, 13500.0f, -85.0f, -7.0f, 20.0f },
            { true, false, 60, +6, +4.0f, +1.5f, 18.0f, 190.0f, 14500.0f, +85.0f, -8.0f, 22.0f },
            { true, false, 60, +7, -6.0f, +2.0f, 21.0f, 230.0f, 15500.0f, -45.0f, -8.5f, 22.0f },
            { true, false, 60, +9, +6.0f, +2.5f, 24.0f, 270.0f, 16500.0f, +45.0f, -9.5f, 24.0f }
        }}
    },
    {
        "Worship 8V Wall", 48.0f, 30.0f, 28.0f, 40.0f, 38.0f, 28.0f, false, true,
        {{
            { true, false, 60, -7, -6.0f, -2.0f,  4.0f,  85.0f, 10500.0f, -55.0f, -6.5f, 15.0f },
            { true, false, 60, -7, +6.0f, -1.5f,  8.0f,  85.0f, 10500.0f, +55.0f, -6.5f, 16.0f },
            { true, false, 60, +2, -8.0f,  0.0f, 11.0f, 130.0f, 14000.0f, -80.0f, -5.5f, 18.0f },
            { true, false, 60, +2, +8.0f, +0.5f, 15.0f, 130.0f, 14000.0f, +80.0f, -5.5f, 19.0f },
            { true, false, 60, +4, -7.0f, +0.5f, 17.0f, 160.0f, 13500.0f, -40.0f, -6.5f, 18.0f },
            { true, false, 60, +4, +7.0f, +1.0f, 21.0f, 160.0f, 13500.0f, +40.0f, -6.5f, 20.0f },
            { true, false, 60, +7, -8.0f, +2.0f, 24.0f, 230.0f, 16000.0f, -95.0f, -8.5f, 18.0f },
            { true, false, 60, +7, +8.0f, +2.0f, 28.0f, 230.0f, 16000.0f, +95.0f, -8.5f, 18.0f }
        }}
    }
}};
} // namespace

PresetManager::PresetManager(juce::AudioProcessorValueTreeState& state,
                             SnapshotManager& snapshots,
                             LiveMidiController& liveMidi)
    : apvts(state),
      snapshotManager(snapshots),
      liveMidiController(liveMidi)
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
    if (auto* um = apvts.undoManager)
        um->beginNewTransaction("Load Preset");

    const auto& spec = kFactoryPresets[static_cast<size_t>(std::clamp(index, 0, numFactoryPresets - 1))];

    setParamValue(params::mix, spec.mixPct);
    setParamValue(params::humanize, spec.humanizePct);
    setParamValue(params::glideMs, spec.glideMs);
    setParamValue(params::naturalness, spec.naturalPct);
    setParamValue(params::deEssAmount, spec.deEssPct);
    setParamValue(params::spaceAmount, spec.spacePct);
    setParamValue(params::voiceBank, spec.use8Voices ? 1.0f : 0.0f);
    if (spec.forceLive)
        setParamValue(params::engineMode, 0.0f);

    for (int v = 0; v < dsp::kExposedVoices; ++v)
    {
        const auto& vp = spec.voices[static_cast<size_t>(v)];
        setParamValue(params::voiceParamId(v, params::vEnabled),   vp.enabled ? 1.0f : 0.0f);
        setParamValue(params::voiceParamId(v, params::vMute),      0.0f);
        setParamValue(params::voiceParamId(v, params::vSolo),      0.0f);
        setParamValue(params::voiceParamId(v, params::vFixed),     vp.fixedPitch ? 1.0f : 0.0f);
        setParamValue(params::voiceParamId(v, params::vFixedNote), static_cast<float>(vp.fixedNote));
        setParamValue(params::voiceParamId(v, params::vInterval),  static_cast<float>(vp.interval));
        setParamValue(params::voiceParamId(v, params::vDetune),    vp.detune);
        setParamValue(params::voiceParamId(v, params::vFormant),   vp.formant);
        setParamValue(params::voiceParamId(v, params::vDelayMs),   vp.delayMs);
        setParamValue(params::voiceParamId(v, params::vHpfHz),     vp.hpfHz);
        setParamValue(params::voiceParamId(v, params::vLpfHz),     vp.lpfHz);
        setParamValue(params::voiceParamId(v, params::vPan),       vp.panPct);
        setParamValue(params::voiceParamId(v, params::vGainDb),    vp.gainDb);
        setParamValue(params::voiceParamId(v, params::vVibrato),   vp.vibratoPct);
    }
}

void PresetManager::applyVoicingPreset(int voicingIndex)
{
    const int clamped = std::clamp(voicingIndex, 0, static_cast<int>(dsp::kVoicingPresets.size()) - 1);
    const auto& spec = dsp::kVoicingPresets[static_cast<size_t>(clamped)];

    if (auto* um = apvts.undoManager)
        um->beginNewTransaction("Apply Voicing");

    int keyIdx = 0;
    if (auto* rawKey = apvts.getRawParameterValue(params::key))
        keyIdx = std::clamp(static_cast<int>(std::lround(rawKey->load(std::memory_order_relaxed))), 0, 11);

    setParamValue(params::voiceBank, spec.use8Voices ? 1.0f : 0.0f);

    for (int v = 0; v < dsp::kExposedVoices; ++v)
    {
        const auto& vp = spec.voices[static_cast<size_t>(v)];
        const int fixedMidi = dsp::computePedalMidiNote(keyIdx, vp.fixedOffsetFromRoot);
        setParamValue(params::voiceParamId(v, params::vEnabled),   vp.enabled ? 1.0f : 0.0f);
        setParamValue(params::voiceParamId(v, params::vMute),      0.0f);
        setParamValue(params::voiceParamId(v, params::vSolo),      0.0f);
        setParamValue(params::voiceParamId(v, params::vFixed),     vp.fixedPitch ? 1.0f : 0.0f);
        setParamValue(params::voiceParamId(v, params::vFixedNote), static_cast<float>(fixedMidi));
        setParamValue(params::voiceParamId(v, params::vInterval),  static_cast<float>(vp.interval));
        setParamValue(params::voiceParamId(v, params::vDetune),    vp.detune);
        setParamValue(params::voiceParamId(v, params::vFormant),   vp.formant);
        setParamValue(params::voiceParamId(v, params::vDelayMs),   vp.delayMs);
        setParamValue(params::voiceParamId(v, params::vHpfHz),     vp.hpfHz);
        setParamValue(params::voiceParamId(v, params::vLpfHz),     vp.lpfHz);
        setParamValue(params::voiceParamId(v, params::vPan),       vp.panPct);
        setParamValue(params::voiceParamId(v, params::vGainDb),    vp.gainDb);
        setParamValue(params::voiceParamId(v, params::vVibrato),   vp.vibratoPct);
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
            {
                auto snapshotsNode = tree.getChildWithName("SNAPSHOTS");
                if (snapshotsNode.isValid())
                    tree.removeChild(snapshotsNode, nullptr);

                auto midiMapNode = tree.getChildWithName("MIDIMAP");
                if (midiMapNode.isValid())
                    tree.removeChild(midiMapNode, nullptr);

                snapshotManager.beginExternalStateLoad();
                apvts.replaceState(tree);
                snapshotManager.loadFromStateTree(snapshotsNode);
                liveMidiController.loadFromStateTree(midiMapNode);
                snapshotManager.endExternalStateLoad();
            }
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
    snapshotManager.writeToStateTree(stateCopy);
    liveMidiController.writeToStateTree(stateCopy);
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
