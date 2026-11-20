#pragma once

#include "Dsp/DspConstants.h"
#include "Plugin/PluginProcessor.h"
#include "Ui/ModernLookAndFeel.h"

#include <juce_gui_basics/juce_gui_basics.h>
#include <juce_audio_processors/juce_audio_processors.h>
#include <memory>

namespace harmonizer::ui
{

class EngineModePill final : public juce::Component,
                             public juce::SettableTooltipClient
{
public:
    explicit EngineModePill(juce::AudioProcessorValueTreeState& apvts);

    void setLatencyText(const juce::String& text);
    void paint(juce::Graphics& g) override;
    void mouseMove(const juce::MouseEvent& e) override;
    void mouseExit(const juce::MouseEvent& e) override;
    void mouseDown(const juce::MouseEvent& e) override;

private:
    void handleModeChanged(float newValue);
    void handleLiveBufferChanged(float newValue);
    void openLiveBufferMenu();

    std::unique_ptr<juce::ParameterAttachment> modeAttachment;
    std::unique_ptr<juce::ParameterAttachment> liveBufferAttachment;
    int currentMode = 0;       // 0 = Live, 1 = Studio
    int currentLiveBuffer = 1; // 0 = 128, 1 = 256, 2 = 384, 3 = 512
    bool latencyHovered = false;
    juce::String latencyLabel{ "5.3 ms" };
};

class HarmonyModePill final : public juce::Component
{
public:
    explicit HarmonyModePill(juce::AudioProcessorValueTreeState& apvts);

    void paint(juce::Graphics& g) override;
    void mouseDown(const juce::MouseEvent& e) override;

private:
    void handleParameterChanged(float newValue);

    std::unique_ptr<juce::ParameterAttachment> attachment;
    int currentMode = 0; // 0 = Auto, 1 = MIDI
};

class PowerBypassButton final : public juce::Component,
                                public juce::SettableTooltipClient
{
public:
    PowerBypassButton(juce::AudioProcessorValueTreeState& apvts, LiveMidiController& liveMidi);

    void setActiveColour(juce::Colour c);
    void paint(juce::Graphics& g) override;
    void mouseEnter(const juce::MouseEvent&) override;
    void mouseExit(const juce::MouseEvent&) override;
    void mouseDown(const juce::MouseEvent& e) override;

private:
    void handleBypassChanged(float newValue);
    void openContextMenu();

    LiveMidiController& liveMidiController;
    juce::UndoManager* undoManager = nullptr;
    std::unique_ptr<juce::ParameterAttachment> attachment;
    juce::Colour activeColour{ Theme::liveAccent };
    bool bypassed = false;
    bool hovered = false;
};

class HeaderBar final : public juce::Component
{
public:
    explicit HeaderBar(HarmonizerVProcessor& proc);

    void updateTelemetry(const dsp::TelemetrySnapshot& snap);
    void rebuildPresetMenu();

    void resized() override;
    void paint(juce::Graphics& g) override;

private:
    void openSavePresetDialog();
    static juce::String midiToNoteName(float midiNote);

    HarmonizerVProcessor& processor;

    PowerBypassButton powerBypassBtn;

    juce::TextButton prevPresetBtn{ "<" };
    juce::ComboBox presetCombo;
    juce::TextButton nextPresetBtn{ ">" };
    juce::TextButton savePresetBtn{ "Save" };

    juce::ComboBox keyCombo;
    juce::ComboBox scaleCombo;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> keyAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> scaleAttachment;

    EngineModePill enginePill;
    HarmonyModePill harmonyModePill;

    LabeledKnob mixKnob{ "MIX", Theme::liveAccent };
    LabeledKnob outKnob{ "OUT", Theme::textPrimary };

    juce::Rectangle<int> notePillBounds;
    juce::Rectangle<int> meterBounds;

    dsp::TelemetrySnapshot latestSnap{};
    float meterLevelL = 0.0f;
    float meterLevelR = 0.0f;
};

} // namespace harmonizer::ui
