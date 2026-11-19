#include "Ui/HeaderBar.h"
#include "Dsp/ScaleHarmonizer.h"
#include "Plugin/ParameterIds.h"
#include "Ui/Theme.h"

#include <algorithm>
#include <array>
#include <cmath>

namespace harmonizer::ui
{

// ============================================================================
// EngineModePill
// ============================================================================

EngineModePill::EngineModePill(juce::AudioProcessorValueTreeState& apvts)
{
    if (auto* param = apvts.getParameter(params::engineMode))
    {
        attachment = std::make_unique<juce::ParameterAttachment>(
            *param,
            [this](float newValue) { handleParameterChanged(newValue); },
            nullptr);
        attachment->sendInitialUpdate();
    }
    setMouseCursor(juce::MouseCursor::PointingHandCursor);
}

void EngineModePill::handleParameterChanged(float newValue)
{
    currentMode = (newValue >= 0.5f) ? 1 : 0;
    repaint();
}

void EngineModePill::setLatencyText(const juce::String& text)
{
    if (latencyLabel != text)
    {
        latencyLabel = text;
        repaint();
    }
}

void EngineModePill::paint(juce::Graphics& g)
{
    auto bounds = getLocalBounds().toFloat();
    auto pillArea = bounds.removeFromTop(26.0f);

    g.setColour(Theme::bgElevated);
    g.fillRoundedRectangle(pillArea, 7.0f);
    g.setColour(Theme::borderSubtle);
    g.drawRoundedRectangle(pillArea.reduced(0.5f), 7.0f, 1.0f);

    const auto halfW = pillArea.getWidth() * 0.5f;
    const auto liveRect = pillArea.withWidth(halfW).reduced(2.0f);
    const auto studioRect = pillArea.withTrimmedLeft(halfW).reduced(2.0f);

    if (currentMode == 0)
    {
        g.setColour(Theme::liveAccent);
        g.fillRoundedRectangle(liveRect, 5.5f);
    }
    else
    {
        g.setColour(Theme::studioAccent);
        g.fillRoundedRectangle(studioRect, 5.5f);
    }

    g.setFont(Theme::makeFont(11.0f, true));
    g.setColour(currentMode == 0 ? Theme::bgCanvas : Theme::textSecondary);
    g.drawFittedText("LIVE", liveRect.toNearestInt(), juce::Justification::centred, 1);

    g.setColour(currentMode == 1 ? Theme::textPrimary : Theme::textSecondary);
    g.drawFittedText("STUDIO", studioRect.toNearestInt(), juce::Justification::centred, 1);

    g.setFont(Theme::makeFont(10.0f));
    g.setColour(currentMode == 0 ? Theme::liveAccent : Theme::studioAccent);
    g.drawFittedText(latencyLabel, bounds.toNearestInt(), juce::Justification::centred, 1);
}

void EngineModePill::mouseDown(const juce::MouseEvent& e)
{
    if (attachment == nullptr)
        return;

    const float target = (e.position.x < static_cast<float>(getWidth()) * 0.5f) ? 0.0f : 1.0f;
    attachment->setValueAsCompleteGesture(target);
}

// ============================================================================
// HeaderBar
// ============================================================================

HeaderBar::HeaderBar(HarmonizerVProcessor& proc)
    : processor(proc),
      enginePill(proc.getApvts())
{
    addAndMakeVisible(prevPresetBtn);
    addAndMakeVisible(presetCombo);
    addAndMakeVisible(nextPresetBtn);
    addAndMakeVisible(savePresetBtn);

    prevPresetBtn.setTooltip("Previous Preset");
    nextPresetBtn.setTooltip("Next Preset");
    savePresetBtn.setTooltip("Save Current State as User Preset");

    rebuildPresetMenu();
    presetCombo.onChange = [this]()
    {
        const int selectedId = presetCombo.getSelectedId();
        if (selectedId > 0)
            processor.getPresetManager().loadPreset(selectedId - 1);
    };

    prevPresetBtn.onClick = [this]()
    {
        const int count = presetCombo.getNumItems();
        if (count <= 0)
            return;
        const int cur = std::max(0, presetCombo.getSelectedItemIndex());
        const int next = (cur - 1 + count) % count;
        presetCombo.setSelectedItemIndex(next, juce::sendNotification);
    };

    nextPresetBtn.onClick = [this]()
    {
        const int count = presetCombo.getNumItems();
        if (count <= 0)
            return;
        const int cur = std::max(0, presetCombo.getSelectedItemIndex());
        const int next = (cur + 1) % count;
        presetCombo.setSelectedItemIndex(next, juce::sendNotification);
    };

    savePresetBtn.onClick = [this]() { openSavePresetDialog(); };

    for (int k = 0; k < 12; ++k)
        keyCombo.addItem(dsp::ScaleHarmonizer::getKeyName(k), k + 1);
    addAndMakeVisible(keyCombo);
    keyAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(
        processor.getApvts(), params::key, keyCombo);

    for (int s = 0; s < static_cast<int>(dsp::ScaleType::NumScales); ++s)
        scaleCombo.addItem(dsp::ScaleHarmonizer::getScaleName(s), s + 1);
    addAndMakeVisible(scaleCombo);
    scaleAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(
        processor.getApvts(), params::scale, scaleCombo);

    addAndMakeVisible(enginePill);

    mixKnob.setValueFormatter([](double v) { return juce::String(std::lround(v)) + "%"; });
    outKnob.setValueFormatter([](double v) { return juce::String(v, 1) + "dB"; });
    mixKnob.attach(processor.getApvts(), params::mix);
    outKnob.attach(processor.getApvts(), params::outputGain);
    addAndMakeVisible(mixKnob);
    addAndMakeVisible(outKnob);
}

void HeaderBar::rebuildPresetMenu()
{
    presetCombo.clear(juce::dontSendNotification);
    const auto& list = processor.getPresetManager().getPresets();
    const int numFactory = processor.getPresetManager().getNumFactoryPresets();

    for (int i = 0; i < static_cast<int>(list.size()); ++i)
    {
        if (i == numFactory && numFactory < static_cast<int>(list.size()))
            presetCombo.addSeparator();
        presetCombo.addItem(list[static_cast<size_t>(i)].name, i + 1);
    }

    const int cur = processor.getPresetManager().getCurrentPresetIndex();
    presetCombo.setSelectedId(cur + 1, juce::dontSendNotification);
}

void HeaderBar::openSavePresetDialog()
{
    auto* aw = new juce::AlertWindow("Save User Preset",
                                     "Enter a name for your vocal harmony preset:",
                                     juce::MessageBoxIconType::NoIcon);
    aw->addTextEditor("presetName", "My Harmony");
    aw->addButton("Save", 1, juce::KeyPress(juce::KeyPress::returnKey));
    aw->addButton("Cancel", 0, juce::KeyPress(juce::KeyPress::escapeKey));

    aw->enterModalState(true, juce::ModalCallbackFunction::create(
        [safeThis = juce::Component::SafePointer<HeaderBar>(this), aw](int result)
        {
            if (result == 1 && safeThis != nullptr)
            {
                const auto name = aw->getTextEditorContents("presetName");
                if (safeThis->processor.getPresetManager().saveUserPreset(name))
                    safeThis->rebuildPresetMenu();
            }
        }),
        true);
}

juce::String HeaderBar::midiToNoteName(float midiNote)
{
    static constexpr std::array<const char*, 12> kNotes = {
        "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"
    };
    const int rounded = std::clamp(static_cast<int>(std::lround(midiNote)), 0, 127);
    const int octave = (rounded / 12) - 1;
    const int pc = rounded % 12;
    return juce::String(kNotes[static_cast<size_t>(pc)]) + juce::String(octave);
}

void HeaderBar::updateTelemetry(const dsp::TelemetrySnapshot& snap)
{
    latestSnap = snap;

    const double sr = std::max(8000.0, processor.getCurrentSampleRateSafe());
    const double latMs = static_cast<double>(snap.activeLatencySamples) * 1000.0 / sr;
    enginePill.setLatencyText(juce::String(latMs, 1) + " ms PDC");

    mixKnob.setAccentColour(snap.activeMode == dsp::EngineMode::Live ? Theme::liveAccent : Theme::studioAccent);

    // Smooth decay output peak meters
    meterLevelL = std::max(snap.outputPeakL, meterLevelL * 0.80f);
    meterLevelR = std::max(snap.outputPeakR, meterLevelR * 0.80f);
    repaint();
}

void HeaderBar::resized()
{
    auto r = getLocalBounds().reduced(12, 8);

    // Right side controls
    meterBounds = r.removeFromRight(14).reduced(1, 3);
    r.removeFromRight(8);
    outKnob.setBounds(r.removeFromRight(46));
    r.removeFromRight(4);
    mixKnob.setBounds(r.removeFromRight(46));
    r.removeFromRight(10);
    enginePill.setBounds(r.removeFromRight(118).withSizeKeepingCentre(118, 42));
    r.removeFromRight(10);

    // Left side brand + presets
    r.removeFromLeft(112); // Space for brand wordmark painted cleanly
    prevPresetBtn.setBounds(r.removeFromLeft(22).withSizeKeepingCentre(22, 26));
    r.removeFromLeft(3);

    const int presetWidth = std::clamp(r.getWidth() / 3, 105, 165);
    presetCombo.setBounds(r.removeFromLeft(presetWidth).withSizeKeepingCentre(presetWidth, 26));
    r.removeFromLeft(3);
    nextPresetBtn.setBounds(r.removeFromLeft(22).withSizeKeepingCentre(22, 26));
    r.removeFromLeft(4);
    savePresetBtn.setBounds(r.removeFromLeft(42).withSizeKeepingCentre(42, 26));
    r.removeFromLeft(12);

    // Center Key / Scale / Detected note pill
    const int keyW = 54;
    const int scaleW = std::min(126, std::max(90, r.getWidth() - 160));
    keyCombo.setBounds(r.removeFromLeft(keyW).withSizeKeepingCentre(keyW, 26));
    r.removeFromLeft(5);
    scaleCombo.setBounds(r.removeFromLeft(scaleW).withSizeKeepingCentre(scaleW, 26));
    r.removeFromLeft(8);

    notePillBounds = r.withSizeKeepingCentre(std::min(104, r.getWidth()), 26);
}

void HeaderBar::paint(juce::Graphics& g)
{
    const auto bounds = getLocalBounds().toFloat();
    g.setColour(Theme::bgSurface);
    g.fillRoundedRectangle(bounds, Theme::kCardRadius);
    g.setColour(Theme::borderSubtle);
    g.drawRoundedRectangle(bounds.reduced(0.5f), Theme::kCardRadius, 1.0f);

    // Brand logo
    const auto brandRect = getLocalBounds().reduced(12, 8).withWidth(108);
    const auto accent = (latestSnap.activeMode == dsp::EngineMode::Live) ? Theme::liveAccent : Theme::studioAccent;
    g.setColour(accent);
    g.fillEllipse(static_cast<float>(brandRect.getX()), static_cast<float>(brandRect.getCentreY() - 4), 8.0f, 8.0f);

    g.setColour(Theme::textPrimary);
    g.setFont(Theme::makeFont(14.5f, true));
    g.drawFittedText("HarmonizerV", brandRect.withTrimmedLeft(13), juce::Justification::centredLeft, 1);

    // Live detected pitch pill
    if (!notePillBounds.isEmpty())
    {
        const auto np = notePillBounds.toFloat();
        g.setColour(Theme::bgElevated);
        g.fillRoundedRectangle(np, Theme::kPillRadius);
        g.setColour(latestSnap.isVoiced ? Theme::liveAccent.withAlpha(0.6f) : Theme::borderSubtle);
        g.drawRoundedRectangle(np.reduced(0.5f), Theme::kPillRadius, 1.0f);

        juce::String noteText = "--  --- Hz";
        if (latestSnap.isVoiced && latestSnap.detectedHz > 20.0f)
        {
            noteText = midiToNoteName(latestSnap.detectedMidi)
                     + " · " + juce::String(std::lround(latestSnap.detectedHz)) + "Hz";
        }
        g.setColour(latestSnap.isVoiced ? Theme::textPrimary : Theme::textMuted);
        g.setFont(Theme::makeFont(11.5f, latestSnap.isVoiced));
        g.drawFittedText(noteText, notePillBounds.reduced(4, 0), juce::Justification::centred, 1);
    }

    // Compact stereo output peak meter
    if (!meterBounds.isEmpty())
    {
        const auto mf = meterBounds.toFloat();
        g.setColour(Theme::bgCanvas);
        g.fillRoundedRectangle(mf, 3.0f);

        auto drawBar = [&](float level, float xPos, float w)
        {
            const float db = std::clamp(dsp::gainToDb(level), -48.0f, 3.0f);
            const float norm = (db + 48.0f) / 51.0f;
            const float h = mf.getHeight() * norm;
            const auto barRect = juce::Rectangle<float>(xPos, mf.getBottom() - h, w, h);
            g.setColour(db > -1.0f ? Theme::muteAccent : (db > -6.0f ? Theme::warnAccent : Theme::liveAccent));
            g.fillRoundedRectangle(barRect, 1.5f);
        };

        const float barW = (mf.getWidth() - 3.0f) * 0.5f;
        drawBar(meterLevelL, mf.getX() + 1.0f, barW);
        drawBar(meterLevelR, mf.getX() + 2.0f + barW, barW);
    }
}

} // namespace harmonizer::ui
