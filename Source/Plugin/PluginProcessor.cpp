#include "Plugin/PluginProcessor.h"
#include "Plugin/ParameterIds.h"
#include "Plugin/PluginEditor.h"

#include <algorithm>
#include <array>

namespace harmonizer
{

namespace
{
struct DefaultVoiceInit
{
    bool enabled;
    int interval;
    float delayMs;
    float hpfHz;
    float lpfHz;
    float panPct;
    float gainDb;
};

constexpr std::array<DefaultVoiceInit, dsp::kExposedVoicesV1> kDefaultVoices = {{
    { true,  +2,  4.0f, 110.0f, 14000.0f, -35.0f, -5.0f },
    { true,  +4,  9.0f, 130.0f, 13500.0f, +35.0f, -6.0f },
    { false, -2, 12.0f, 100.0f, 14000.0f, -65.0f, -8.0f },
    { false, +7, 16.0f, 160.0f, 15000.0f, +65.0f, -8.0f }
}};
} // namespace

HarmonizerVProcessor::HarmonizerVProcessor()
    : AudioProcessor(BusesProperties()
                         .withInput("Input", juce::AudioChannelSet::stereo(), true)
                         .withOutput("Output", juce::AudioChannelSet::stereo(), true)),
      apvts(*this, nullptr, "HarmonizerVState", createParameterLayout()),
      presetManager(apvts)
{
    keyParam        = apvts.getRawParameterValue(params::key);
    scaleParam      = apvts.getRawParameterValue(params::scale);
    engineModeParam = apvts.getRawParameterValue(params::engineMode);
    mixParam        = apvts.getRawParameterValue(params::mix);
    outputGainParam = apvts.getRawParameterValue(params::outputGain);
    humanizeParam   = apvts.getRawParameterValue(params::humanize);
    glideParam      = apvts.getRawParameterValue(params::glideMs);
    naturalParam    = apvts.getRawParameterValue(params::naturalness);
    deEssParam      = apvts.getRawParameterValue(params::deEssAmount);
    spaceParam      = apvts.getRawParameterValue(params::spaceAmount);

    for (int v = 0; v < dsp::kExposedVoicesV1; ++v)
    {
        auto& va = voiceAtomics[static_cast<size_t>(v)];
        va.enabled  = apvts.getRawParameterValue(params::voiceParamId(v, params::vEnabled));
        va.mute     = apvts.getRawParameterValue(params::voiceParamId(v, params::vMute));
        va.solo     = apvts.getRawParameterValue(params::voiceParamId(v, params::vSolo));
        va.interval = apvts.getRawParameterValue(params::voiceParamId(v, params::vInterval));
        va.detune   = apvts.getRawParameterValue(params::voiceParamId(v, params::vDetune));
        va.formant  = apvts.getRawParameterValue(params::voiceParamId(v, params::vFormant));
        va.delayMs  = apvts.getRawParameterValue(params::voiceParamId(v, params::vDelayMs));
        va.hpfHz    = apvts.getRawParameterValue(params::voiceParamId(v, params::vHpfHz));
        va.lpfHz    = apvts.getRawParameterValue(params::voiceParamId(v, params::vLpfHz));
        va.pan      = apvts.getRawParameterValue(params::voiceParamId(v, params::vPan));
        va.gainDb   = apvts.getRawParameterValue(params::voiceParamId(v, params::vGainDb));
    }
}

HarmonizerVProcessor::~HarmonizerVProcessor()
{
    cancelPendingUpdate();
}

juce::AudioProcessorValueTreeState::ParameterLayout HarmonizerVProcessor::createParameterLayout()
{
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> p;

    juce::StringArray keyChoices{ "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" };
    p.push_back(std::make_unique<juce::AudioParameterChoice>(
        juce::ParameterID{ params::key, 1 }, "Key", keyChoices, 0));

    juce::StringArray scaleChoices{
        "Major", "Natural Minor", "Harmonic Minor", "Melodic Minor",
        "Dorian", "Mixolydian", "Major Pentatonic", "Minor Pentatonic", "Chromatic"
    };
    p.push_back(std::make_unique<juce::AudioParameterChoice>(
        juce::ParameterID{ params::scale, 1 }, "Scale", scaleChoices, 0));

    juce::StringArray engineChoices{ "Live", "Studio" };
    p.push_back(std::make_unique<juce::AudioParameterChoice>(
        juce::ParameterID{ params::engineMode, 1 }, "Engine Mode", engineChoices, 0));

    p.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{ params::mix, 1 }, "Mix",
        juce::NormalisableRange<float>(0.0f, 100.0f, 0.1f), 40.0f,
        juce::AudioParameterFloatAttributes().withLabel("%")));

    p.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{ params::outputGain, 1 }, "Output Gain",
        juce::NormalisableRange<float>(-24.0f, 12.0f, 0.1f), 0.0f,
        juce::AudioParameterFloatAttributes().withLabel("dB")));

    p.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{ params::humanize, 1 }, "Humanize",
        juce::NormalisableRange<float>(0.0f, 100.0f, 0.1f), 20.0f,
        juce::AudioParameterFloatAttributes().withLabel("%")));

    p.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{ params::glideMs, 1 }, "Glide",
        juce::NormalisableRange<float>(0.0f, 150.0f, 0.5f, 0.5f), 28.0f,
        juce::AudioParameterFloatAttributes().withLabel("ms")));

    p.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{ params::naturalness, 1 }, "Natural",
        juce::NormalisableRange<float>(0.0f, 100.0f, 0.5f), 35.0f,
        juce::AudioParameterFloatAttributes().withLabel("%")));

    p.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{ params::deEssAmount, 1 }, "De-Ess",
        juce::NormalisableRange<float>(0.0f, 100.0f, 0.5f), 35.0f,
        juce::AudioParameterFloatAttributes().withLabel("%")));

    p.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{ params::spaceAmount, 1 }, "Space",
        juce::NormalisableRange<float>(0.0f, 100.0f, 0.5f), 18.0f,
        juce::AudioParameterFloatAttributes().withLabel("%")));

    for (int v = 0; v < dsp::kExposedVoicesV1; ++v)
    {
        const auto& def = kDefaultVoices[static_cast<size_t>(v)];
        const juce::String prefix = "V" + juce::String(v + 1) + " ";

        p.push_back(std::make_unique<juce::AudioParameterBool>(
            juce::ParameterID{ params::voiceParamId(v, params::vEnabled), 1 },
            prefix + "Enabled", def.enabled));

        p.push_back(std::make_unique<juce::AudioParameterBool>(
            juce::ParameterID{ params::voiceParamId(v, params::vMute), 1 },
            prefix + "Mute", false));

        p.push_back(std::make_unique<juce::AudioParameterBool>(
            juce::ParameterID{ params::voiceParamId(v, params::vSolo), 1 },
            prefix + "Solo", false));

        p.push_back(std::make_unique<juce::AudioParameterInt>(
            juce::ParameterID{ params::voiceParamId(v, params::vInterval), 1 },
            prefix + "Interval", -14, 14, def.interval));

        p.push_back(std::make_unique<juce::AudioParameterFloat>(
            juce::ParameterID{ params::voiceParamId(v, params::vDetune), 1 },
            prefix + "Detune",
            juce::NormalisableRange<float>(-50.0f, 50.0f, 0.1f), 0.0f,
            juce::AudioParameterFloatAttributes().withLabel("ct")));

        p.push_back(std::make_unique<juce::AudioParameterFloat>(
            juce::ParameterID{ params::voiceParamId(v, params::vFormant), 1 },
            prefix + "Formant",
            juce::NormalisableRange<float>(-12.0f, 12.0f, 0.1f), 0.0f,
            juce::AudioParameterFloatAttributes().withLabel("st")));

        p.push_back(std::make_unique<juce::AudioParameterFloat>(
            juce::ParameterID{ params::voiceParamId(v, params::vDelayMs), 1 },
            prefix + "Delay",
            juce::NormalisableRange<float>(0.0f, 40.0f, 0.1f), def.delayMs,
            juce::AudioParameterFloatAttributes().withLabel("ms")));

        p.push_back(std::make_unique<juce::AudioParameterFloat>(
            juce::ParameterID{ params::voiceParamId(v, params::vHpfHz), 1 },
            prefix + "HPF",
            juce::NormalisableRange<float>(60.0f, 800.0f, 1.0f, 0.45f), def.hpfHz,
            juce::AudioParameterFloatAttributes().withLabel("Hz")));

        p.push_back(std::make_unique<juce::AudioParameterFloat>(
            juce::ParameterID{ params::voiceParamId(v, params::vLpfHz), 1 },
            prefix + "LPF",
            juce::NormalisableRange<float>(2000.0f, 18000.0f, 1.0f, 0.45f), def.lpfHz,
            juce::AudioParameterFloatAttributes().withLabel("Hz")));

        p.push_back(std::make_unique<juce::AudioParameterFloat>(
            juce::ParameterID{ params::voiceParamId(v, params::vPan), 1 },
            prefix + "Pan",
            juce::NormalisableRange<float>(-100.0f, 100.0f, 0.1f), def.panPct,
            juce::AudioParameterFloatAttributes().withLabel("%")));

        p.push_back(std::make_unique<juce::AudioParameterFloat>(
            juce::ParameterID{ params::voiceParamId(v, params::vGainDb), 1 },
            prefix + "Level",
            juce::NormalisableRange<float>(-60.0f, 6.0f, 0.1f, 2.0f), def.gainDb,
            juce::AudioParameterFloatAttributes().withLabel("dB")));
    }

    return { p.begin(), p.end() };
}

bool HarmonizerVProcessor::isBusesLayoutSupported(const BusesLayout& layouts) const
{
    const auto inSet = layouts.getMainInputChannelSet();
    const auto outSet = layouts.getMainOutputChannelSet();

    if (inSet.isDisabled() || outSet.isDisabled())
        return false;

    const bool validMonoToMono = (inSet == juce::AudioChannelSet::mono() && outSet == juce::AudioChannelSet::mono());
    const bool validMonoToStereo = (inSet == juce::AudioChannelSet::mono() && outSet == juce::AudioChannelSet::stereo());
    const bool validStereoToStereo = (inSet == juce::AudioChannelSet::stereo() && outSet == juce::AudioChannelSet::stereo());

    return validMonoToMono || validMonoToStereo || validStereoToStereo;
}

void HarmonizerVProcessor::prepareToPlay(double sampleRate, int samplesPerBlock)
{
    currentSampleRate.store(sampleRate, std::memory_order_relaxed);
    engine.prepare(sampleRate, std::max(64, samplesPerBlock));
    int dummy = 0;
    engine.consumeLatencyChanged(dummy);
    setLatencySamples(engine.getActiveLatencySamples());
}

void HarmonizerVProcessor::releaseResources()
{
    engine.reset();
}

void HarmonizerVProcessor::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;

    const int numSamples = buffer.getNumSamples();
    if (numSamples <= 0)
        return;

    const int numChannels = buffer.getNumChannels();
    const int numIn = std::min(getTotalNumInputChannels(), numChannels);
    const int numOut = std::min(getTotalNumOutputChannels(), numChannels);
    if (numChannels <= 0 || numIn <= 0 || numOut <= 0)
    {
        buffer.clear();
        return;
    }

    dsp::GlobalParams gp;
    gp.key          = static_cast<int>(std::lround(keyParam->load(std::memory_order_relaxed)));
    gp.scaleIndex   = static_cast<int>(std::lround(scaleParam->load(std::memory_order_relaxed)));
    gp.engineMode   = (engineModeParam->load(std::memory_order_relaxed) >= 0.5f)
                        ? dsp::EngineMode::Studio
                        : dsp::EngineMode::Live;
    gp.mix          = mixParam->load(std::memory_order_relaxed) * 0.01f;
    gp.outputGainDb = outputGainParam->load(std::memory_order_relaxed);
    gp.humanize     = humanizeParam->load(std::memory_order_relaxed) * 0.01f;
    gp.glideMs      = glideParam->load(std::memory_order_relaxed);
    gp.naturalness  = naturalParam->load(std::memory_order_relaxed) * 0.01f;
    gp.deEssAmount  = deEssParam->load(std::memory_order_relaxed) * 0.01f;
    gp.spaceAmount  = spaceParam->load(std::memory_order_relaxed) * 0.01f;

    std::array<dsp::VoiceParams, dsp::kMaxVoices> vp{};
    for (int v = 0; v < dsp::kExposedVoicesV1; ++v)
    {
        const auto& va = voiceAtomics[static_cast<size_t>(v)];
        auto& dst = vp[static_cast<size_t>(v)];
        dst.enabled          = va.enabled->load(std::memory_order_relaxed) >= 0.5f;
        dst.mute             = va.mute->load(std::memory_order_relaxed) >= 0.5f;
        dst.solo             = va.solo->load(std::memory_order_relaxed) >= 0.5f;
        dst.intervalSteps    = static_cast<int>(std::lround(va.interval->load(std::memory_order_relaxed)));
        dst.detuneCents      = va.detune->load(std::memory_order_relaxed);
        dst.formantSemitones = va.formant->load(std::memory_order_relaxed);
        dst.delayMs          = va.delayMs->load(std::memory_order_relaxed);
        dst.hpfHz            = va.hpfHz->load(std::memory_order_relaxed);
        dst.lpfHz            = va.lpfHz->load(std::memory_order_relaxed);
        dst.pan              = va.pan->load(std::memory_order_relaxed) * 0.01f;
        dst.gainDb           = va.gainDb->load(std::memory_order_relaxed);
    }

    const float* inPtrs[2] = {
        buffer.getReadPointer(0),
        (numIn >= 2) ? buffer.getReadPointer(1) : buffer.getReadPointer(0)
    };
    float* outPtrs[2] = {
        buffer.getWritePointer(0),
        (numOut >= 2) ? buffer.getWritePointer(1) : buffer.getWritePointer(0)
    };

    engine.processBlock(inPtrs, numIn, outPtrs, numOut, numSamples, gp, vp);

    int newLatency = 0;
    if (engine.consumeLatencyChanged(newLatency))
        triggerAsyncUpdate();
}

void HarmonizerVProcessor::handleAsyncUpdate()
{
    setLatencySamples(engine.getActiveLatencySamples());
}

int HarmonizerVProcessor::getNumPrograms()
{
    return std::max(1, presetManager.getNumFactoryPresets());
}

int HarmonizerVProcessor::getCurrentProgram()
{
    return presetManager.getCurrentPresetIndex();
}

void HarmonizerVProcessor::setCurrentProgram(int index)
{
    presetManager.loadPreset(index);
}

const juce::String HarmonizerVProcessor::getProgramName(int index)
{
    const auto& list = presetManager.getPresets();
    if (index >= 0 && index < static_cast<int>(list.size()))
        return list[static_cast<size_t>(index)].name;
    return "Default";
}

void HarmonizerVProcessor::getStateInformation(juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();
    std::unique_ptr<juce::XmlElement> xml(state.createXml());
    if (xml != nullptr)
        copyXmlToBinary(*xml, destData);
}

void HarmonizerVProcessor::setStateInformation(const void* data, int sizeInBytes)
{
    std::unique_ptr<juce::XmlElement> xmlState(getXmlFromBinary(data, sizeInBytes));
    if (xmlState != nullptr && xmlState->hasTagName(apvts.state.getType()))
        apvts.replaceState(juce::ValueTree::fromXml(*xmlState));
}

juce::AudioProcessorEditor* HarmonizerVProcessor::createEditor()
{
    return new HarmonizerVEditor(*this);
}

} // namespace harmonizer

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new harmonizer::HarmonizerVProcessor();
}
