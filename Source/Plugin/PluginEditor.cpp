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
    addChildComponent(aboutOverlay);

    headerBar.onAboutClicked = [this]()
    {
        aboutOverlay.showOverlay();
    };

    harmonyStage.onVoiceSelected = [this](int voiceIdx)
    {
        voiceInspector.setSelectedVoice(voiceIdx);
    };

    ui::LocalizationManager::getInstance().addListener(this);

    setResizable(true, true);
    setResizeLimits(860, 518, 1400, 900);
    setSize(940, 606);
    setWantsKeyboardFocus(true);

    startTimerHz(30);
}

HarmonizerVEditor::~HarmonizerVEditor()
{
    ui::LocalizationManager::getInstance().removeListener(this);
    stopTimer();
    setLookAndFeel(nullptr);
}

void HarmonizerVEditor::languageChanged(dsp::Language)
{
    headerBar.refreshLocalization();
    harmonyStage.refreshLocalization();
    harmonyFeelBar.refreshLocalization();
    voiceInspector.refreshLocalization();
    aboutOverlay.refreshLocalization();
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
    aboutOverlay.setBounds(getLocalBounds());
}

bool HarmonizerVEditor::keyPressed(const juce::KeyPress& key)
{
    if (key == juce::KeyPress::escapeKey && aboutOverlay.isVisible())
    {
        aboutOverlay.hideOverlay();
        return true;
    }

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
