#pragma once

#include "Dsp/DspConstants.h"
#include "Plugin/PluginProcessor.h"

#include <juce_gui_basics/juce_gui_basics.h>
#include <array>
#include <functional>

namespace harmonizer::ui
{

class HarmonyStage final : public juce::Component,
                           public juce::SettableTooltipClient
{
public:
    explicit HarmonyStage(HarmonizerVProcessor& proc);

    void setSelectedVoice(int voiceIndex);
    int getSelectedVoice() const noexcept { return selectedVoice; }
    void updateTelemetry(const dsp::TelemetrySnapshot& snap);

    std::function<void(int)> onVoiceSelected;

    void paint(juce::Graphics& g) override;
    void resized() override;

    void mouseDown(const juce::MouseEvent& e) override;
    void mouseDrag(const juce::MouseEvent& e) override;
    void mouseUp(const juce::MouseEvent& e) override;
    void mouseDoubleClick(const juce::MouseEvent& e) override;
    void mouseWheelMove(const juce::MouseEvent& e, const juce::MouseWheelDetails& wheel) override;

private:
    juce::Rectangle<float> getStageArea() const noexcept;
    juce::Point<float> voiceToPoint(int voiceIndex) const noexcept;
    float intervalToY(int interval) const noexcept;
    float panToX(float panPct) const noexcept;
    int yToInterval(float y) const noexcept;
    float xToPan(float x) const noexcept;
    float voiceRadius(int voiceIndex) const noexcept;
    int hitTestOrb(juce::Point<float> pos) const noexcept;

    void beginDragVoice(int voiceIndex);
    void endDragVoice();
    float getParamPlain(const juce::String& id, float fallback = 0.0f) const noexcept;
    void setParamPlain(const juce::String& id, float plainVal, bool notifyGesture);
    void showSnapshotContextMenu(int slotIndex);
    void showVoicingMenu();
    void updateHeaderLayout() noexcept;
    int getVisibleVoiceCount() const noexcept;
    static juce::String formatMidiNoteName(int note);

    HarmonizerVProcessor& processor;
    dsp::TelemetrySnapshot latestSnap{};
    int selectedVoice = 0;
    int lastVisibleVoices = 4;

    int draggingVoice = -1;
    bool draggingFixed = false;
    bool gestureActive = false;

    std::array<juce::Rectangle<float>, dsp::kMaxVoices> pillRects{};
    juce::Rectangle<float> bankToggleRect{};
    juce::Rectangle<float> voicingBtnRect{};
    std::array<juce::Rectangle<float>, dsp::kNumSnapshots> snapPillRects{};
    juce::Rectangle<float> undoBtnRect{};
    juce::Rectangle<float> redoBtnRect{};
    juce::Rectangle<float> snapLabelRect{};
    juce::Rectangle<float> hintRect{};
};

} // namespace harmonizer::ui
