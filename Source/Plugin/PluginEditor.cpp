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
    setResizeLimits(840, 518, 1400, 900);
    setSize(920, 606);
    setWantsKeyboardFocus(true);

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
    harmonyFeelBar.updateTelemetry(snap);
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
    harmonyFeelBar.setBounds(r.removeFromBottom(44));
    r.removeFromBottom(8);
    harmonyStage.setBounds(r);
}

bool HarmonizerVEditor::keyPressed(const juce::KeyPress& key)
{
    if (key == juce::KeyPress::escapeKey && proc.getLiveMidiController().getLearningTarget() >= 0)
    {
        proc.getLiveMidiController().cancelLearn();
        headerBar.repaint();
        harmonyStage.repaint();
        return true;
    }

    const auto mods = key.getModifiers();
    if (mods.isCommandDown())
    {
        const auto ch = key.getTextCharacter();
        if ((ch == 'z' || ch == 'Z') && mods.isShiftDown())
        {
            proc.getUndoManager().redo();
            return true;
        }
        if (ch == 'z' || ch == 'Z')
        {
            proc.getUndoManager().undo();
            return true;
        }
        if (ch == 'y' || ch == 'Y')
        {
            proc.getUndoManager().redo();
            return true;
        }
    }

    return AudioProcessorEditor::keyPressed(key);
}

} // namespace harmonizer
