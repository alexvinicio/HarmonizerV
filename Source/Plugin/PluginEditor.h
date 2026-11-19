#pragma once

#include "Plugin/PluginProcessor.h"
#include "Ui/HarmonyStage.h"
#include "Ui/HeaderBar.h"
#include "Ui/ModernLookAndFeel.h"
#include "Ui/VoiceInspector.h"

#include <juce_audio_processors/juce_audio_processors.h>

namespace harmonizer
{

class HarmonizerVEditor final : public juce::AudioProcessorEditor,
                                private juce::Timer
{
public:
    explicit HarmonizerVEditor(HarmonizerVProcessor& processor);
    ~HarmonizerVEditor() override;

    void paint(juce::Graphics& g) override;
    void resized() override;

private:
    void timerCallback() override;

    HarmonizerVProcessor& proc;
    ui::ModernLookAndFeel lookAndFeel;

    ui::HeaderBar headerBar;
    ui::HarmonyStage harmonyStage;
    ui::VoiceInspector voiceInspector;
    juce::TooltipWindow tooltipWindow{ this, 600 };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(HarmonizerVEditor)
};

} // namespace harmonizer
