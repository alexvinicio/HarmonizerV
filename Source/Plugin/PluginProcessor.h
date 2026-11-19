#pragma once

#include "Dsp/HarmonizerEngine.h"
#include "Plugin/PresetManager.h"

#include <juce_audio_processors/juce_audio_processors.h>
#include <array>
#include <atomic>

namespace harmonizer
{

class HarmonizerVProcessor final : public juce::AudioProcessor,
                                   private juce::AsyncUpdater
{
public:
    HarmonizerVProcessor();
    ~HarmonizerVProcessor() override;

    void prepareToPlay(double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;

    bool isBusesLayoutSupported(const BusesLayout& layouts) const override;
    void processBlock(juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    using AudioProcessor::processBlock;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return JucePlugin_Name; }

    bool acceptsMidi() const override { return true; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 0.1; }

    int getNumPrograms() override;
    int getCurrentProgram() override;
    void setCurrentProgram(int index) override;
    const juce::String getProgramName(int index) override;
    void changeProgramName(int, const juce::String&) override {}

    void getStateInformation(juce::MemoryBlock& destData) override;
    void setStateInformation(const void* data, int sizeInBytes) override;

    juce::AudioProcessorValueTreeState& getApvts() noexcept { return apvts; }
    PresetManager& getPresetManager() noexcept { return presetManager; }
    dsp::TelemetrySnapshot getTelemetry() const noexcept { return engine.getTelemetry(); }
    int getLiveLatencySamples() const noexcept { return engine.getLiveLatencySamples(); }
    int getStudioLatencySamples() const noexcept { return engine.getStudioLatencySamples(); }
    double getCurrentSampleRateSafe() const noexcept { return currentSampleRate.load(std::memory_order_relaxed); }

    static juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();

private:
    void handleAsyncUpdate() override;

    struct VoiceAtomics
    {
        std::atomic<float>* enabled = nullptr;
        std::atomic<float>* mute = nullptr;
        std::atomic<float>* solo = nullptr;
        std::atomic<float>* interval = nullptr;
        std::atomic<float>* detune = nullptr;
        std::atomic<float>* formant = nullptr;
        std::atomic<float>* delayMs = nullptr;
        std::atomic<float>* hpfHz = nullptr;
        std::atomic<float>* lpfHz = nullptr;
        std::atomic<float>* pan = nullptr;
        std::atomic<float>* gainDb = nullptr;
    };

    juce::AudioProcessorValueTreeState apvts;
    PresetManager presetManager;
    dsp::HarmonizerEngine engine;

    std::atomic<float>* keyParam = nullptr;
    std::atomic<float>* scaleParam = nullptr;
    std::atomic<float>* engineModeParam = nullptr;
    std::atomic<float>* mixParam = nullptr;
    std::atomic<float>* outputGainParam = nullptr;
    std::atomic<float>* humanizeParam = nullptr;

    std::array<VoiceAtomics, dsp::kExposedVoicesV1> voiceAtomics{};
    std::atomic<double> currentSampleRate{ 48000.0 };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(HarmonizerVProcessor)
};

} // namespace harmonizer
