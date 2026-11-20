#include "Plugin/LiveMidiController.h"
#include "Plugin/ParameterIds.h"

#include <algorithm>

namespace harmonizer
{

LiveMidiController::LiveMidiController(juce::AudioProcessorValueTreeState& state, SnapshotManager& snapshots)
    : apvts(state),
      snapshotManager(snapshots)
{
    for (int t = 0; t < dsp::kNumMidiControlTargets; ++t)
    {
        const auto def = dsp::makeDefaultBinding(static_cast<dsp::LiveMidiTarget>(t));
        bindings[static_cast<size_t>(t)].store(dsp::packMidiBinding(def), std::memory_order_relaxed);
    }
    prevCcHigh.fill(false);
}

LiveMidiController::~LiveMidiController()
{
    learningTarget.store(-1, std::memory_order_release);
    cancelPendingUpdate();
}

bool LiveMidiController::processMidiMessageOnAudioThread(const juce::MidiMessage& msg) noexcept
{
    dsp::MidiBindType evType = dsp::MidiBindType::None;
    const int evChannel = msg.getChannel();
    int evNumber = -1;
    int evValue = 0;

    if (msg.isProgramChange())
    {
        evType = dsp::MidiBindType::ProgramChange;
        evNumber = std::clamp(msg.getProgramChangeNumber(), 0, 127);
        evValue = 127;
    }
    else if (msg.isController())
    {
        evNumber = std::clamp(msg.getControllerNumber(), 0, 127);
        evValue = msg.getControllerValue();
        const bool isHigh = (evValue >= 64);
        const bool risingEdge = isHigh && !prevCcHigh[static_cast<size_t>(evNumber)];
        prevCcHigh[static_cast<size_t>(evNumber)] = isHigh;
        if (!risingEdge)
            return false;
        evType = dsp::MidiBindType::ControlChange;
    }
    else if (msg.isNoteOn())
    {
        evValue = static_cast<int>(msg.getVelocity());
        if (evValue <= 0)
            return false;
        evType = dsp::MidiBindType::Note;
        evNumber = std::clamp(msg.getNoteNumber(), 0, 127);
    }
    else
    {
        return false;
    }

    const int learnIdx = learningTarget.load(std::memory_order_acquire);
    if (learnIdx >= 0 && learnIdx < dsp::kNumMidiControlTargets)
    {
        const dsp::MidiBinding b{ evType, 0, evNumber };
        bindings[static_cast<size_t>(learnIdx)].store(dsp::packMidiBinding(b), std::memory_order_release);
        learningTarget.store(-1, std::memory_order_release);
        return true;
    }

    for (int t = 0; t < dsp::kNumMidiControlTargets; ++t)
    {
        const auto b = dsp::unpackMidiBinding(bindings[static_cast<size_t>(t)].load(std::memory_order_acquire));
        if (dsp::bindingMatchesEvent(b, evType, evChannel, evNumber, evValue))
        {
            if (t < dsp::kNumSnapshots)
                pendingSnapshotIdx.store(t, std::memory_order_release);
            else if (t == static_cast<int>(dsp::LiveMidiTarget::Bypass))
                pendingBypassToggles.fetch_add(1, std::memory_order_acq_rel);

            triggerAsyncUpdate();
            return true;
        }
    }

    return false;
}

void LiveMidiController::handleAsyncUpdate()
{
    const int snap = pendingSnapshotIdx.exchange(-1, std::memory_order_acq_rel);
    if (snap >= 0)
        snapshotManager.selectSnapshot(snap);

    const int toggles = pendingBypassToggles.exchange(0, std::memory_order_acq_rel);
    if ((toggles & 1) != 0)
    {
        if (auto* param = apvts.getParameter(params::bypass))
        {
            if (auto* um = apvts.undoManager)
                um->beginNewTransaction("Toggle Bypass");

            const bool isBypassed = param->getValue() >= 0.5f;
            param->beginChangeGesture();
            param->setValueNotifyingHost(isBypassed ? 0.0f : 1.0f);
            param->endChangeGesture();
        }
    }
}

void LiveMidiController::startLearn(dsp::LiveMidiTarget target) noexcept
{
    const int idx = static_cast<int>(target);
    if (idx >= 0 && idx < dsp::kNumMidiControlTargets)
        learningTarget.store(idx, std::memory_order_release);
}

void LiveMidiController::cancelLearn() noexcept
{
    learningTarget.store(-1, std::memory_order_release);
}

dsp::MidiBinding LiveMidiController::getBinding(dsp::LiveMidiTarget target) const noexcept
{
    const int idx = static_cast<int>(target);
    if (idx < 0 || idx >= dsp::kNumMidiControlTargets)
        return dsp::MidiBinding{};
    return dsp::unpackMidiBinding(bindings[static_cast<size_t>(idx)].load(std::memory_order_acquire));
}

void LiveMidiController::setBinding(dsp::LiveMidiTarget target, const dsp::MidiBinding& b) noexcept
{
    const int idx = static_cast<int>(target);
    if (idx >= 0 && idx < dsp::kNumMidiControlTargets)
        bindings[static_cast<size_t>(idx)].store(dsp::packMidiBinding(b), std::memory_order_release);
}

void LiveMidiController::clearBinding(dsp::LiveMidiTarget target) noexcept
{
    setBinding(target, dsp::MidiBinding{});
}

void LiveMidiController::resetDefaultBinding(dsp::LiveMidiTarget target) noexcept
{
    setBinding(target, dsp::makeDefaultBinding(target));
}

juce::String LiveMidiController::getBindingLabel(dsp::LiveMidiTarget target) const
{
    return juce::String(dsp::formatMidiBindingAscii(getBinding(target)));
}

void LiveMidiController::writeToStateTree(juce::ValueTree& rootState) const
{
    while (true)
    {
        auto existing = rootState.getChildWithName("MIDIMAP");
        if (!existing.isValid())
            break;
        rootState.removeChild(existing, nullptr);
    }

    juce::ValueTree mapTree("MIDIMAP");
    for (int t = 0; t < dsp::kNumMidiControlTargets; ++t)
    {
        const auto b = getBinding(static_cast<dsp::LiveMidiTarget>(t));
        juce::ValueTree bindTree("BIND");
        bindTree.setProperty("target", t, nullptr);
        bindTree.setProperty("type", static_cast<int>(b.type), nullptr);
        bindTree.setProperty("channel", b.channel, nullptr);
        bindTree.setProperty("number", b.number, nullptr);
        mapTree.appendChild(bindTree, nullptr);
    }

    rootState.appendChild(mapTree, nullptr);
}

void LiveMidiController::loadFromStateTree(const juce::ValueTree& midiMapNode)
{
    cancelLearn();
    pendingSnapshotIdx.store(-1, std::memory_order_release);
    pendingBypassToggles.store(0, std::memory_order_release);

    for (int t = 0; t < dsp::kNumMidiControlTargets; ++t)
        resetDefaultBinding(static_cast<dsp::LiveMidiTarget>(t));

    if (!midiMapNode.isValid() || !midiMapNode.hasType("MIDIMAP"))
        return;

    for (int c = 0; c < midiMapNode.getNumChildren(); ++c)
    {
        const auto child = midiMapNode.getChild(c);
        if (!child.hasType("BIND"))
            continue;

        const int t = static_cast<int>(child.getProperty("target", -1));
        if (t < 0 || t >= dsp::kNumMidiControlTargets)
            continue;

        dsp::MidiBinding b;
        b.type = static_cast<dsp::MidiBindType>(static_cast<int>(child.getProperty("type", 0)));
        b.channel = std::clamp(static_cast<int>(child.getProperty("channel", 0)), 0, 16);
        b.number = static_cast<int>(child.getProperty("number", -1));

        setBinding(static_cast<dsp::LiveMidiTarget>(t), b);
    }
}

} // namespace harmonizer
