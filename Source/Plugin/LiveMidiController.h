#pragma once

#include "Dsp/MidiControlMap.h"
#include "Plugin/SnapshotManager.h"

#include <juce_audio_processors/juce_audio_processors.h>
#include <array>
#include <atomic>

namespace harmonizer
{

class LiveMidiController final : private juce::AsyncUpdater
{
public:
    LiveMidiController(juce::AudioProcessorValueTreeState& state, SnapshotManager& snapshots);
    ~LiveMidiController() override;

    /** Called strictly on audio thread (lock-free, allocation-free).
        Returns true if consumed (learned or matched a control target). */
    bool processMidiMessageOnAudioThread(const juce::MidiMessage& msg) noexcept;

    void startLearn(dsp::LiveMidiTarget target) noexcept;
    void cancelLearn() noexcept;
    int getLearningTarget() const noexcept { return learningTarget.load(std::memory_order_acquire); }

    dsp::MidiBinding getBinding(dsp::LiveMidiTarget target) const noexcept;
    void setBinding(dsp::LiveMidiTarget target, const dsp::MidiBinding& b) noexcept;
    void clearBinding(dsp::LiveMidiTarget target) noexcept;
    void resetDefaultBinding(dsp::LiveMidiTarget target) noexcept;
    juce::String getBindingLabel(dsp::LiveMidiTarget target) const;

    void writeToStateTree(juce::ValueTree& rootState) const;
    void loadFromStateTree(const juce::ValueTree& midiMapNode);

private:
    void handleAsyncUpdate() override;

    juce::AudioProcessorValueTreeState& apvts;
    SnapshotManager& snapshotManager;

    std::array<std::atomic<uint32_t>, dsp::kNumMidiControlTargets> bindings{};
    std::atomic<int> learningTarget{ -1 };
    std::atomic<int> pendingSnapshotIdx{ -1 };
    std::atomic<int> pendingBypassToggles{ 0 };
    std::array<bool, 128> prevCcHigh{};

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(LiveMidiController)
};

} // namespace harmonizer
