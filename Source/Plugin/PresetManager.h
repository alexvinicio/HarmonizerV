#pragma once

#include "Dsp/DspConstants.h"

#include <juce_audio_processors/juce_audio_processors.h>
#include <array>
#include <vector>

namespace harmonizer
{

class SnapshotManager;
class LiveMidiController;

struct PresetEntry
{
    juce::String name;
    bool isFactory = true;
    juce::File file;
};

class PresetManager
{
public:
    PresetManager(juce::AudioProcessorValueTreeState& state,
                  SnapshotManager& snapshots,
                  LiveMidiController& liveMidi);

    void refreshPresets();
    const std::vector<PresetEntry>& getPresets() const noexcept { return presets; }
    int getNumFactoryPresets() const noexcept { return numFactoryPresets; }

    void loadPreset(int index);
    void applyVoicingPreset(int voicingIndex);
    bool saveUserPreset(const juce::String& name);

    int getCurrentPresetIndex() const noexcept { return currentIndex; }
    void setCurrentPresetIndex(int index) noexcept { currentIndex = index; }

    static juce::File getUserPresetDirectory();

private:
    void applyFactoryPreset(int index);
    void setParamValue(const juce::String& id, float plainValue);

    juce::AudioProcessorValueTreeState& apvts;
    SnapshotManager& snapshotManager;
    LiveMidiController& liveMidiController;
    std::vector<PresetEntry> presets;
    int numFactoryPresets = 7;
    int currentIndex = 0;
};

} // namespace harmonizer
