#include "Plugin/PluginEditor.h"
#include "Ui/Theme.h"

namespace harmonizer
{

HarmonizerVEditor::HarmonizerVEditor(HarmonizerVProcessor& p)
    : AudioProcessorEditor(&p),
      proc(p),
      headerBar(p),
      harmonyStage(p),
      voiceInspector(p)
{
    setLookAndFeel(&lookAndFeel);

    addAndMakeVisible(headerBar);
    addAndMakeVisible(harmonyStage);
    addAndMakeVisible(voiceInspector);

    harmonyStage.onVoiceSelected = [this](int voiceIdx)
    {
        voiceInspector.setSelectedVoice(voiceIdx);
    };

    setResizable(true, true);
    setResizeLimits(760, 480, 1400, 860);
    setSize(920, 560);

    startTimerHz(30);
}

HarmonizerVEditor::~HarmonizerVEditor()
{
    stopTimer();
    setLookAndFeel(nullptr);
}

void HarmonizerVEditor::timerCallback()
{
    const auto snap = proc.getTelemetry();
    headerBar.updateTelemetry(snap);
    harmonyStage.updateTelemetry(snap);
}

void HarmonizerVEditor::paint(juce::Graphics& g)
{
    g.fillAll(ui::Theme::bgCanvas);
}

void HarmonizerVEditor::resized()
{
    auto r = getLocalBounds().reduced(12);
    headerBar.setBounds(r.removeFromTop(62));
    r.removeFromTop(10);
    voiceInspector.setBounds(r.removeFromBottom(116));
    r.removeFromBottom(10);
    harmonyStage.setBounds(r);
}

} // namespace harmonizer
