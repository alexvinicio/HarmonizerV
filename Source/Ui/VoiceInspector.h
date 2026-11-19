#pragma once

#include "Plugin/PluginProcessor.h"
#include "Ui/ModernLookAndFeel.h"

#include <juce_gui_basics/juce_gui_basics.h>
#include <juce_audio_processors/juce_audio_processors.h>
#include <memory>

namespace harmonizer::ui
{

class VoiceInspector final : public juce::Component,
                             private juce::AudioProcessorValueTreeState::Listener
{
public:
    explicit VoiceInspector(HarmonizerVProcessor& proc);
    ~VoiceInspector() override;

    void setSelectedVoice(int voiceIndex);
    int getSelectedVoice() const noexcept { return selectedVoice; }

    void resized() override;
    void paint(juce::Graphics& g) override;

private:
    void parameterChanged(const juce::String& parameterID, float newValue) override;
    void bindSelectedVoice();

    HarmonizerVProcessor& processor;
    int selectedVoice = 0;

    juce::TextButton onBtn{ "ON" };
    juce::TextButton muteBtn{ "M" };
    juce::TextButton soloBtn{ "S" };

    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> onAtt;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> muteAtt;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> soloAtt;

    LabeledKnob intervalKnob { "INTERVAL" };
    LabeledKnob detuneKnob   { "DETUNE" };
    LabeledKnob formantKnob  { "FORMANT" };
    LabeledKnob delayKnob    { "DELAY" };
    LabeledKnob hpfKnob      { "HPF" };
    LabeledKnob lpfKnob      { "LPF" };
    LabeledKnob panKnob      { "PAN" };
    LabeledKnob levelKnob    { "LEVEL" };
    LabeledKnob humanizeKnob { "HUMANIZE", Theme::liveAccent };

    juce::Rectangle<int> dividerBounds;
};

} // namespace harmonizer::ui
