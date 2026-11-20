#include "Plugin/SnapshotManager.h"
#include "Plugin/ParameterIds.h"

#include <algorithm>
#include <cmath>

namespace harmonizer
{

namespace
{
struct ScopedAtomicBoolFlag
{
    explicit ScopedAtomicBoolFlag(std::atomic<bool>& flagRef) noexcept
        : flag(flagRef), previous(flagRef.exchange(true, std::memory_order_acq_rel))
    {
    }

    ~ScopedAtomicBoolFlag() noexcept
    {
        flag.store(previous, std::memory_order_release);
    }

    std::atomic<bool>& flag;
    bool previous;
};
} // namespace

SnapshotManager::SnapshotManager(juce::AudioProcessorValueTreeState& state)
    : apvts(state)
{
    int idx = 0;
    sceneParamIds[static_cast<size_t>(idx++)] = params::voiceBank;
    sceneParamIds[static_cast<size_t>(idx++)] = params::harmonyMode;
    sceneParamIds[static_cast<size_t>(idx++)] = params::mix;
    sceneParamIds[static_cast<size_t>(idx++)] = params::humanize;
    sceneParamIds[static_cast<size_t>(idx++)] = params::inputTune;
    sceneParamIds[static_cast<size_t>(idx++)] = params::glideMs;
    sceneParamIds[static_cast<size_t>(idx++)] = params::naturalness;
    sceneParamIds[static_cast<size_t>(idx++)] = params::deEssAmount;
    sceneParamIds[static_cast<size_t>(idx++)] = params::spaceAmount;

    for (int v = 0; v < dsp::kExposedVoices; ++v)
    {
        sceneParamIds[static_cast<size_t>(idx++)] = params::voiceParamId(v, params::vEnabled);
        sceneParamIds[static_cast<size_t>(idx++)] = params::voiceParamId(v, params::vMute);
        sceneParamIds[static_cast<size_t>(idx++)] = params::voiceParamId(v, params::vSolo);
        sceneParamIds[static_cast<size_t>(idx++)] = params::voiceParamId(v, params::vFixed);
        sceneParamIds[static_cast<size_t>(idx++)] = params::voiceParamId(v, params::vFixedNote);
        sceneParamIds[static_cast<size_t>(idx++)] = params::voiceParamId(v, params::vInterval);
        sceneParamIds[static_cast<size_t>(idx++)] = params::voiceParamId(v, params::vDetune);
        sceneParamIds[static_cast<size_t>(idx++)] = params::voiceParamId(v, params::vFormant);
        sceneParamIds[static_cast<size_t>(idx++)] = params::voiceParamId(v, params::vDelayMs);
        sceneParamIds[static_cast<size_t>(idx++)] = params::voiceParamId(v, params::vHpfHz);
        sceneParamIds[static_cast<size_t>(idx++)] = params::voiceParamId(v, params::vLpfHz);
        sceneParamIds[static_cast<size_t>(idx++)] = params::voiceParamId(v, params::vPan);
        sceneParamIds[static_cast<size_t>(idx++)] = params::voiceParamId(v, params::vGainDb);
        sceneParamIds[static_cast<size_t>(idx++)] = params::voiceParamId(v, params::vVibrato);
    }

    const auto cur = readCurrentSceneValues();
    {
        std::lock_guard<std::mutex> lock(mutex);
        bank.initializeFrom(cur, 0);
        syncUiAtomicsLocked();
    }

    apvts.addParameterListener(params::snapshot, this);
    for (const auto& id : sceneParamIds)
        apvts.addParameterListener(id, this);
}

SnapshotManager::~SnapshotManager()
{
    suppressCallbacks.store(true, std::memory_order_release);
    cancelPendingUpdate();

    apvts.removeParameterListener(params::snapshot, this);
    for (const auto& id : sceneParamIds)
        apvts.removeParameterListener(id, this);
}

void SnapshotManager::syncUiAtomicsLocked() noexcept
{
    activeIndexAtomic.store(bank.getActiveIndex(), std::memory_order_relaxed);
    for (int i = 0; i < dsp::kNumSnapshots; ++i)
        populatedAtomics[static_cast<size_t>(i)].store(bank.isPopulated(i), std::memory_order_relaxed);
}

bool SnapshotManager::isSnapshotPopulated(int index) const noexcept
{
    if (index < 0 || index >= dsp::kNumSnapshots)
        return false;
    return populatedAtomics[static_cast<size_t>(index)].load(std::memory_order_relaxed);
}

dsp::SceneValues SnapshotManager::readCurrentSceneValues() const noexcept
{
    dsp::SceneValues values{};
    for (size_t i = 0; i < static_cast<size_t>(dsp::kSceneParamCount); ++i)
    {
        if (auto* raw = apvts.getRawParameterValue(sceneParamIds[i]))
            values[i] = raw->load(std::memory_order_relaxed);
    }
    return values;
}

void SnapshotManager::applySceneValuesToApvts(const dsp::SceneValues& values)
{
    ScopedAtomicBoolFlag guard(suppressCallbacks);
    for (size_t i = 0; i < static_cast<size_t>(dsp::kSceneParamCount); ++i)
    {
        if (auto* param = apvts.getParameter(sceneParamIds[i]))
        {
            const float norm = param->convertTo0to1(values[i]);
            if (std::abs(param->getValue() - norm) > 1.0e-5f)
            {
                param->beginChangeGesture();
                param->setValueNotifyingHost(norm);
                param->endChangeGesture();
            }
        }
    }
}

void SnapshotManager::setSnapshotParamNotifyingHost(int index)
{
    ScopedAtomicBoolFlag guard(suppressCallbacks);
    if (auto* param = apvts.getParameter(params::snapshot))
    {
        const float clamped = static_cast<float>(std::clamp(index, 0, dsp::kNumSnapshots - 1));
        const float norm = param->convertTo0to1(clamped);
        param->beginChangeGesture();
        param->setValueNotifyingHost(norm);
        param->endChangeGesture();
    }
}

void SnapshotManager::parameterChanged(const juce::String& parameterID, float newValue)
{
    if (suppressCallbacks.load(std::memory_order_acquire))
        return;

    if (parameterID == params::snapshot)
    {
        const int target = std::clamp(static_cast<int>(std::lround(newValue)), 0, dsp::kNumSnapshots - 1);
        pendingSnapshotIndex.store(target, std::memory_order_release);
        hasPendingSwitch.store(true, std::memory_order_release);
        triggerAsyncUpdate();
    }
    else
    {
        activeSlotDirty.store(true, std::memory_order_release);
    }
}

void SnapshotManager::handleAsyncUpdate()
{
    if (suppressCallbacks.load(std::memory_order_acquire))
        return;

    if (hasPendingSwitch.exchange(false, std::memory_order_acq_rel))
        applySwitchInternal(pendingSnapshotIndex.load(std::memory_order_acquire), false);
}

void SnapshotManager::applySwitchInternal(int targetIndex, bool notifySnapshotParam)
{
    const auto currentValues = readCurrentSceneValues();
    dsp::SceneValues toApply{};
    bool shouldApply = false;
    int newActive = 0;

    {
        std::lock_guard<std::mutex> lock(mutex);
        if (activeSlotDirty.exchange(false, std::memory_order_acq_rel))
            bank.markDirty();

        shouldApply = bank.selectSlot(targetIndex, currentValues, toApply);
        newActive = bank.getActiveIndex();
        syncUiAtomicsLocked();
    }

    if (shouldApply || notifySnapshotParam)
    {
        if (auto* um = apvts.undoManager)
            um->beginNewTransaction("Snapshot");
    }

    if (shouldApply)
        applySceneValuesToApvts(toApply);

    if (notifySnapshotParam)
        setSnapshotParamNotifyingHost(newActive);

    activeSlotDirty.store(false, std::memory_order_release);
}

void SnapshotManager::selectSnapshot(int index)
{
    hasPendingSwitch.store(false, std::memory_order_release);
    applySwitchInternal(index, true);
}

void SnapshotManager::storeSnapshot(int index)
{
    const auto currentValues = readCurrentSceneValues();
    int storedIdx = 0;
    {
        std::lock_guard<std::mutex> lock(mutex);
        bank.storeSlot(index, currentValues);
        storedIdx = bank.getActiveIndex();
        syncUiAtomicsLocked();
    }
    activeSlotDirty.store(false, std::memory_order_release);
    if (auto* um = apvts.undoManager)
        um->beginNewTransaction("Snapshot");
    setSnapshotParamNotifyingHost(storedIdx);
}

void SnapshotManager::copySnapshot(int srcIndex, int dstIndex)
{
    dsp::SceneValues toApply{};
    bool applyNow = false;
    {
        std::lock_guard<std::mutex> lock(mutex);
        applyNow = bank.copySlot(srcIndex, dstIndex, toApply);
        syncUiAtomicsLocked();
    }
    if (applyNow)
    {
        if (auto* um = apvts.undoManager)
            um->beginNewTransaction("Snapshot");
        applySceneValuesToApvts(toApply);
        activeSlotDirty.store(false, std::memory_order_release);
    }
}

void SnapshotManager::clearSnapshot(int index)
{
    std::lock_guard<std::mutex> lock(mutex);
    if (bank.clearSlot(index))
        syncUiAtomicsLocked();
}

void SnapshotManager::writeToStateTree(juce::ValueTree& rootState) const
{
    while (true)
    {
        auto existing = rootState.getChildWithName("SNAPSHOTS");
        if (!existing.isValid())
            break;
        rootState.removeChild(existing, nullptr);
    }

    const auto currentValues = readCurrentSceneValues();
    dsp::SnapshotBank localBank;
    {
        std::lock_guard<std::mutex> lock(mutex);
        localBank = bank;
    }

    if (activeSlotDirty.load(std::memory_order_acquire))
        localBank.setSlotRaw(localBank.getActiveIndex(), true, currentValues);

    juce::ValueTree snaps("SNAPSHOTS");
    snaps.setProperty("active", localBank.getActiveIndex(), nullptr);

    for (int i = 0; i < dsp::kNumSnapshots; ++i)
    {
        const bool populated = localBank.isPopulated(i);
        juce::ValueTree slotTree("SLOT");
        slotTree.setProperty("index", i, nullptr);
        slotTree.setProperty("populated", populated, nullptr);

        if (populated)
        {
            const auto& slot = localBank.getSlot(i);
            for (size_t p = 0; p < static_cast<size_t>(dsp::kSceneParamCount); ++p)
                slotTree.setProperty(juce::Identifier(sceneParamIds[p]), static_cast<double>(slot.values[p]), nullptr);
        }

        snaps.appendChild(slotTree, nullptr);
    }

    rootState.appendChild(snaps, nullptr);
}

void SnapshotManager::beginExternalStateLoad()
{
    suppressCallbacks.store(true, std::memory_order_release);
    hasPendingSwitch.store(false, std::memory_order_release);
    cancelPendingUpdate();
}

void SnapshotManager::loadFromStateTree(const juce::ValueTree& snapshotsNode)
{
    const auto currentValues = readCurrentSceneValues();
    int activeIdx = 0;
    if (auto* rawSnap = apvts.getRawParameterValue(params::snapshot))
        activeIdx = std::clamp(static_cast<int>(std::lround(rawSnap->load(std::memory_order_relaxed))), 0, dsp::kNumSnapshots - 1);

    std::lock_guard<std::mutex> lock(mutex);
    bank.initializeFrom(currentValues, activeIdx);

    if (snapshotsNode.isValid() && snapshotsNode.hasType("SNAPSHOTS"))
    {
        for (int c = 0; c < snapshotsNode.getNumChildren(); ++c)
        {
            const auto child = snapshotsNode.getChild(c);
            if (!child.hasType("SLOT"))
                continue;

            const int slotIdx = static_cast<int>(child.getProperty("index", -1));
            if (slotIdx < 0 || slotIdx >= dsp::kNumSnapshots)
                continue;

            const bool populated = static_cast<bool>(child.getProperty("populated", false));
            if (populated)
            {
                dsp::SceneValues vals = currentValues;
                for (size_t p = 0; p < static_cast<size_t>(dsp::kSceneParamCount); ++p)
                {
                    const juce::Identifier propId(sceneParamIds[p]);
                    if (child.hasProperty(propId))
                        vals[p] = static_cast<float>(static_cast<double>(child.getProperty(propId)));
                }
                bank.setSlotRaw(slotIdx, true, vals);
            }
            else
            {
                bank.setSlotRaw(slotIdx, false, currentValues);
            }
        }

        // Ensure active slot mirrors live restored APVTS state
        bank.setSlotRaw(activeIdx, true, currentValues);
        bank.setActiveIndexRaw(activeIdx);
        bank.clearDirty();
    }

    syncUiAtomicsLocked();
}

void SnapshotManager::endExternalStateLoad()
{
    hasPendingSwitch.store(false, std::memory_order_release);
    activeSlotDirty.store(false, std::memory_order_release);
    suppressCallbacks.store(false, std::memory_order_release);
}

} // namespace harmonizer
