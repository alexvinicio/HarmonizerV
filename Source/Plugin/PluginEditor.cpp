#include "Plugin/PluginEditor.h"
#include "Ui/Theme.h"

namespace harmonizer
{

HarmonizerVEditor::HarmonizerVEditor(HarmonizerVProcessor& p)
    : AudioProcessorEditor(&p),
      proc(p),
      headerBar(p),
      harmonyStage(p),
      harmonyFeelBar(p),
      voiceInspector(p)
{
    setLookAndFeel(&lookAndFeel);

    addAndMakeVisible(headerBar);
    addAndMakeVisible(harmonyStage);
    addAndMakeVisible(harmonyFeelBar);
    addAndMakeVisible(voiceInspector);

    harmonyStage.onVoiceSelected = [this](int voiceIdx)
    {
        voiceInspector.setSelectedVoice(voiceIdx);
    };

    setResizable(true, true);
    setResizeLimits(760, 510, 1400, 900);
    setSize(920, 598);

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
    harmonyFeelBar.updateModeAccent(snap.activeMode);
}

void HarmonizerVEditor::paint(juce::Graphics& g)
{
    g.fillAll(ui::Theme::bgCanvas);
}

void HarmonizerVEditor::resized()
{
    auto r = getLocalBounds().reduced(12);
    headerBar.setBounds(r.removeFromTop(62));
    r.removeFromTop(8);
    voiceInspector.setBounds(r.removeFromBottom(116));
    r.removeFromBottom(8);
    harmonyFeelBar.setBounds(r.removeFromBottom(36));
    r.removeFromBottom(8);
    harmonyStage.setBounds(r);
}

} // namespace harmonizer
