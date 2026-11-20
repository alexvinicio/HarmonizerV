#pragma once

#include "Dsp/SnapshotBank.h"

#include <juce_audio_processors/juce_audio_processors.h>
#include <array>
#include <atomic>
#include <mutex>

namespace harmonizer
{

class SnapshotManager final : private juce::AudioProcessorValueTreeState::Listener,
                              private juce::AsyncUpdater
{
public:
    explicit SnapshotManager(juce::AudioProcessorValueTreeState& state);
    ~SnapshotManager() override;

    void selectSnapshot(int index);
    void storeSnapshot(int index);
    void copySnapshot(int srcIndex, int dstIndex);
    void clearSnapshot(int index);

    int getActiveSnapshotIndex() const noexcept { return activeIndexAtomic.load(std::memory_order_relaxed); }
    bool isSnapshotPopulated(int index) const noexcept;

    void writeToStateTree(juce::ValueTree& rootState) const;
    void beginExternalStateLoad();
    void loadFromStateTree(const juce::ValueTree& snapshotsNode);
    void endExternalStateLoad();

private:
    void parameterChanged(const juce::String& parameterID, float newValue) override;
    void handleAsyncUpdate() override;

    dsp::SceneValues readCurrentSceneValues() const noexcept;
    void applySceneValuesToApvts(const dsp::SceneValues& values);
    void setSnapshotParamNotifyingHost(int index);
    void applySwitchInternal(int targetIndex, bool notifySnapshotParam);
    void syncUiAtomicsLocked() noexcept;

    juce::AudioProcessorValueTreeState& apvts;
    std::array<juce::String, dsp::kSceneParamCount> sceneParamIds;

    mutable std::mutex mutex;
    dsp::SnapshotBank bank;

    std::atomic<bool> suppressCallbacks{ false };
    std::atomic<bool> activeSlotDirty{ false };
    std::atomic<bool> hasPendingSwitch{ false };
    std::atomic<int> pendingSnapshotIndex{ 0 };
    std::atomic<int> activeIndexAtomic{ 0 };
    std::array<std::atomic<bool>, dsp::kNumSnapshots> populatedAtomics{};

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(SnapshotManager)
};

} // namespace harmonizer
