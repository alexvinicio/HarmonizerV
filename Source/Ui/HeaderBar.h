#pragma once

#include "Dsp/DspConstants.h"
#include "Plugin/PluginProcessor.h"
#include "Ui/ModernLookAndFeel.h"

#include <juce_gui_basics/juce_gui_basics.h>
#include <juce_audio_processors/juce_audio_processors.h>
#include <memory>

namespace harmonizer::ui
{

class EngineModePill final : public juce::Component
{
public:
    explicit EngineModePill(juce::AudioProcessorValueTreeState& apvts);

    void setLatencyText(const juce::String& text);
    void paint(juce::Graphics& g) override;
    void mouseDown(const juce::MouseEvent& e) override;

private:
    void handleParameterChanged(float newValue);

    std::unique_ptr<juce::ParameterAttachment> attachment;
    int currentMode = 0; // 0 = Live, 1 = Studio
    juce::String latencyLabel{ "10.7 ms" };
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

    juce::TextButton prevPresetBtn{ "<" };
    juce::ComboBox presetCombo;
    juce::TextButton nextPresetBtn{ ">" };
    juce::TextButton savePresetBtn{ "Save" };

    juce::ComboBox keyCombo;
    juce::ComboBox scaleCombo;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> keyAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> scaleAttachment;

    EngineModePill enginePill;

    LabeledKnob mixKnob{ "MIX", Theme::liveAccent };
    LabeledKnob outKnob{ "OUT", Theme::textPrimary };

    juce::Rectangle<int> notePillBounds;
    juce::Rectangle<int> meterBounds;

    dsp::TelemetrySnapshot latestSnap{};
    float meterLevelL = 0.0f;
    float meterLevelR = 0.0f;
};

} // namespace harmonizer::ui
