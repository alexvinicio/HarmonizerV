#pragma once

#include "Dsp/DspConstants.h"

#include <algorithm>
#include <array>

namespace harmonizer::dsp
{

inline constexpr int kNumSnapshots = 4;
inline constexpr int kSceneGlobalCount = 9;      // voice_bank, harmony_mode, mix, humanize, input_tune, glide_ms, natural, deess, space
inline constexpr int kSceneVoiceParamCount = 14; // enabled, mute, solo, fixed, fixed_note, interval, detune, formant, delay_ms, hpf_hz, lpf_hz, pan, gain_db, vibrato
inline constexpr int kSceneParamCount = kSceneGlobalCount + kExposedVoices * kSceneVoiceParamCount;

using SceneValues = std::array<float, kSceneParamCount>;

struct SnapshotSlot
{
    bool populated = false;
    SceneValues values{};
};

/** Pure C++ fixed-capacity bank for 4 musical harmony snapshots (A/B/C/D). */
class SnapshotBank
{
public:
    void initializeFrom(const SceneValues& initialValues, int initialActive = 0) noexcept
    {
        for (auto& slot : slots)
            slot = SnapshotSlot{};

        activeIndex = std::clamp(initialActive, 0, kNumSnapshots - 1);
        slots[static_cast<size_t>(activeIndex)].populated = true;
        slots[static_cast<size_t>(activeIndex)].values = initialValues;
        dirty = false;
    }

    void markDirty() noexcept { dirty = true; }
    void clearDirty() noexcept { dirty = false; }
    bool isDirty() const noexcept { return dirty; }

    int getActiveIndex() const noexcept { return activeIndex; }

    bool isPopulated(int index) const noexcept
    {
        if (index < 0 || index >= kNumSnapshots)
            return false;
        return slots[static_cast<size_t>(index)].populated;
    }

    const SnapshotSlot& getSlot(int index) const noexcept
    {
        const int clamped = std::clamp(index, 0, kNumSnapshots - 1);
        return slots[static_cast<size_t>(clamped)];
    }

    /** Switches active snapshot. Returns true when outValuesToApply must be pushed to parameters. */
    bool selectSlot(int targetIndex, const SceneValues& currentValues, SceneValues& outValuesToApply) noexcept
    {
        const int target = std::clamp(targetIndex, 0, kNumSnapshots - 1);
        if (target == activeIndex)
            return false;

        auto& curSlot = slots[static_cast<size_t>(activeIndex)];
        if (dirty || !curSlot.populated)
        {
            curSlot.values = currentValues;
            curSlot.populated = true;
        }

        activeIndex = target;
        dirty = false;

        auto& nextSlot = slots[static_cast<size_t>(target)];
        if (nextSlot.populated)
        {
            outValuesToApply = nextSlot.values;
            return true;
        }

        // First activation of an empty slot clones the current scene cleanly as starting point
        nextSlot.values = currentValues;
        nextSlot.populated = true;
        return false;
    }

    void storeSlot(int targetIndex, const SceneValues& currentValues) noexcept
    {
        const int target = std::clamp(targetIndex, 0, kNumSnapshots - 1);
        slots[static_cast<size_t>(target)].values = currentValues;
        slots[static_cast<size_t>(target)].populated = true;
        activeIndex = target;
        dirty = false;
    }

    bool copySlot(int srcIndex, int dstIndex, SceneValues& outValuesToApply) noexcept
    {
        if (srcIndex < 0 || srcIndex >= kNumSnapshots || dstIndex < 0 || dstIndex >= kNumSnapshots)
            return false;
        if (srcIndex == dstIndex || !slots[static_cast<size_t>(srcIndex)].populated)
            return false;

        slots[static_cast<size_t>(dstIndex)].values = slots[static_cast<size_t>(srcIndex)].values;
        slots[static_cast<size_t>(dstIndex)].populated = true;

        if (dstIndex == activeIndex)
        {
            dirty = false;
            outValuesToApply = slots[static_cast<size_t>(dstIndex)].values;
            return true;
        }
        return false;
    }

    bool clearSlot(int targetIndex) noexcept
    {
        if (targetIndex < 0 || targetIndex >= kNumSnapshots || targetIndex == activeIndex)
            return false;
        slots[static_cast<size_t>(targetIndex)].populated = false;
        return true;
    }

    void setSlotRaw(int index, bool populated, const SceneValues& values) noexcept
    {
        if (index < 0 || index >= kNumSnapshots)
            return;
        slots[static_cast<size_t>(index)].populated = populated;
        slots[static_cast<size_t>(index)].values = values;
    }

    void setActiveIndexRaw(int index) noexcept
    {
        activeIndex = std::clamp(index, 0, kNumSnapshots - 1);
    }

private:
    std::array<SnapshotSlot, kNumSnapshots> slots{};
    int activeIndex = 0;
    bool dirty = false;
};

} // namespace harmonizer::dsp
