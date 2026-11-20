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
        modeAttachment = std::make_unique<juce::ParameterAttachment>(
            *param,
            [this](float newValue) { handleModeChanged(newValue); },
            nullptr);
        modeAttachment->sendInitialUpdate();
    }

    if (auto* bufParam = apvts.getParameter(params::liveBuffer))
    {
        liveBufferAttachment = std::make_unique<juce::ParameterAttachment>(
            *bufParam,
            [this](float newValue) { handleLiveBufferChanged(newValue); },
            nullptr);
        liveBufferAttachment->sendInitialUpdate();
    }

    setMouseCursor(juce::MouseCursor::PointingHandCursor);
    setTooltip("LIVE / STUDIO engine · Click latency badge in LIVE mode to change DSP buffer (2.7–10.7 ms)");
}

void EngineModePill::handleModeChanged(float newValue)
{
    currentMode = (newValue >= 0.5f) ? 1 : 0;
    repaint();
}

void EngineModePill::handleLiveBufferChanged(float newValue)
{
    currentLiveBuffer = std::clamp(static_cast<int>(std::lround(newValue)), 0, 3);
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

void EngineModePill::openLiveBufferMenu()
{
    juce::PopupMenu menu;
    menu.addItem(1, "Ultra · 2.7 ms (128 smp)", true, currentLiveBuffer == 0);
    menu.addItem(2, "Fast · 5.3 ms (256 smp)",  true, currentLiveBuffer == 1);
    menu.addItem(3, "Low · 8.0 ms (384 smp)",   true, currentLiveBuffer == 2);
    menu.addItem(4, "Safe · 10.7 ms (512 smp)", true, currentLiveBuffer == 3);

    menu.showMenuAsync(
        juce::PopupMenu::Options().withTargetComponent(this),
        [safeThis = juce::Component::SafePointer<EngineModePill>(this)](int selectedId)
        {
            if (safeThis != nullptr && selectedId >= 1 && selectedId <= 4 && safeThis->liveBufferAttachment != nullptr)
                safeThis->liveBufferAttachment->setValueAsCompleteGesture(static_cast<float>(selectedId - 1));
        });
}

void EngineModePill::paint(juce::Graphics& g)
{
    auto bounds = getLocalBounds().toFloat();
    auto pillArea = bounds.removeFromTop(24.0f);

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

    bounds.removeFromTop(2.0f);

    if (currentMode == 0)
    {
        const auto chip = bounds.reduced(6.0f, 0.0f);
        g.setColour(latencyHovered ? Theme::bgElevated.brighter(0.14f) : Theme::bgElevated);
        g.fillRoundedRectangle(chip, 4.5f);
        g.setColour(Theme::liveAccent.withAlpha(latencyHovered ? 0.80f : 0.35f));
        g.drawRoundedRectangle(chip.reduced(0.5f), 4.5f, 1.0f);

        g.setFont(Theme::makeFont(9.8f, true));
        g.setColour(Theme::liveAccent);
        g.drawFittedText(latencyLabel, chip.withTrimmedRight(12.0f).toNearestInt(), juce::Justification::centred, 1);

        // Sharp downward triangle chevron
        const float cx = chip.getRight() - 8.5f;
        const float cy = chip.getCentreY();
        juce::Path chevron;
        chevron.addTriangle(cx - 2.6f, cy - 1.4f, cx + 2.6f, cy - 1.4f, cx, cy + 1.9f);
        g.fillPath(chevron);
    }
    else
    {
        g.setFont(Theme::makeFont(10.0f));
        g.setColour(Theme::studioAccent);
        g.drawFittedText(latencyLabel, bounds.toNearestInt(), juce::Justification::centred, 1);
    }
}

void EngineModePill::mouseMove(const juce::MouseEvent& e)
{
    const bool overChip = (currentMode == 0 && e.position.y >= 24.0f);
    if (latencyHovered != overChip)
    {
        latencyHovered = overChip;
        repaint();
    }
}

void EngineModePill::mouseExit(const juce::MouseEvent&)
{
    if (latencyHovered)
    {
        latencyHovered = false;
        repaint();
    }
}

void EngineModePill::mouseDown(const juce::MouseEvent& e)
{
    if (currentMode == 0 && (e.position.y >= 24.0f || e.mods.isRightButtonDown()))
    {
        openLiveBufferMenu();
        return;
    }

    if (modeAttachment == nullptr)
        return;

    const float target = (e.position.x < static_cast<float>(getWidth()) * 0.5f) ? 0.0f : 1.0f;
    modeAttachment->setValueAsCompleteGesture(target);
}

// ============================================================================
// HarmonyModePill
// ============================================================================

HarmonyModePill::HarmonyModePill(juce::AudioProcessorValueTreeState& apvts)
{
    if (auto* param = apvts.getParameter(params::harmonyMode))
    {
        attachment = std::make_unique<juce::ParameterAttachment>(
            *param,
            [this](float newValue) { handleParameterChanged(newValue); },
            nullptr);
        attachment->sendInitialUpdate();
    }
    setMouseCursor(juce::MouseCursor::PointingHandCursor);
}

void HarmonyModePill::handleParameterChanged(float newValue)
{
    currentMode = (newValue >= 0.5f) ? 1 : 0;
    repaint();
}

void HarmonyModePill::paint(juce::Graphics& g)
{
    const auto pillArea = getLocalBounds().toFloat();

    g.setColour(Theme::bgElevated);
    g.fillRoundedRectangle(pillArea, 6.0f);
    g.setColour(Theme::borderSubtle);
    g.drawRoundedRectangle(pillArea.reduced(0.5f), 6.0f, 1.0f);

    const float halfW = pillArea.getWidth() * 0.5f;
    const auto autoRect = pillArea.withWidth(halfW).reduced(2.0f);
    const auto midiRect = pillArea.withTrimmedLeft(halfW).reduced(2.0f);

    if (currentMode == 0)
    {
        g.setColour(Theme::borderFocus.brighter(0.25f));
        g.fillRoundedRectangle(autoRect, 4.5f);
    }
    else
    {
        g.setColour(Theme::getVoiceColour(0)); // Cyan MIDI accent
        g.fillRoundedRectangle(midiRect, 4.5f);
    }

    g.setFont(Theme::makeFont(10.5f, true));
    g.setColour(currentMode == 0 ? Theme::textPrimary : Theme::textSecondary);
    g.drawFittedText("AUTO", autoRect.toNearestInt(), juce::Justification::centred, 1);

    g.setColour(currentMode == 1 ? Theme::bgCanvas : Theme::textSecondary);
    g.drawFittedText("MIDI", midiRect.toNearestInt(), juce::Justification::centred, 1);
}

void HarmonyModePill::mouseDown(const juce::MouseEvent& e)
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
      enginePill(proc.getApvts()),
      harmonyModePill(proc.getApvts())
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

    addAndMakeVisible(harmonyModePill);
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
    enginePill.setLatencyText(snap.activeMode == dsp::EngineMode::Live
        ? juce::String(latMs, 1) + " ms"
        : juce::String(latMs, 1) + " ms PDC");

    mixKnob.setAccentColour(snap.activeMode == dsp::EngineMode::Live ? Theme::liveAccent : Theme::studioAccent);

    // Smooth decay output peak meters
    meterLevelL = std::max(snap.outputPeakL, meterLevelL * 0.80f);
    meterLevelR = std::max(snap.outputPeakR, meterLevelR * 0.80f);
    repaint();
}

void HeaderBar::resized()
{
    auto r = getLocalBounds().reduced(10, 8);

    // Right side controls
    meterBounds = r.removeFromRight(14).reduced(1, 3);
    r.removeFromRight(6);
    outKnob.setBounds(r.removeFromRight(42));
    r.removeFromRight(2);
    mixKnob.setBounds(r.removeFromRight(42));
    r.removeFromRight(8);
    enginePill.setBounds(r.removeFromRight(112).withSizeKeepingCentre(112, 42));
    r.removeFromRight(8);

    // Left side brand + presets
    r.removeFromLeft(102); // Space for brand wordmark painted cleanly
    prevPresetBtn.setBounds(r.removeFromLeft(20).withSizeKeepingCentre(20, 26));
    r.removeFromLeft(2);

    const int fixedMiddleW = 84 + 6 + 50 + 4 + 38 + 20 + 2 + 4 + 8 + 8;
    const int flexAvail = std::max(180, r.getWidth() - fixedMiddleW);
    const int presetWidth = std::clamp(flexAvail * 36 / 100, 96, 150);
    const int scaleW      = std::clamp(flexAvail * 36 / 100, 90, 122);
    const int noteW       = std::clamp(flexAvail - presetWidth - scaleW, 76, 98);

    presetCombo.setBounds(r.removeFromLeft(presetWidth).withSizeKeepingCentre(presetWidth, 26));
    r.removeFromLeft(2);
    nextPresetBtn.setBounds(r.removeFromLeft(20).withSizeKeepingCentre(20, 26));
    r.removeFromLeft(3);
    savePresetBtn.setBounds(r.removeFromLeft(38).withSizeKeepingCentre(38, 26));
    r.removeFromLeft(8);

    // Center Harmony Mode / Key / Scale / Detected note pill
    harmonyModePill.setBounds(r.removeFromLeft(84).withSizeKeepingCentre(84, 26));
    r.removeFromLeft(6);
    keyCombo.setBounds(r.removeFromLeft(50).withSizeKeepingCentre(50, 26));
    r.removeFromLeft(4);
    scaleCombo.setBounds(r.removeFromLeft(scaleW).withSizeKeepingCentre(scaleW, 26));
    r.removeFromLeft(6);

    notePillBounds = r.withSizeKeepingCentre(std::min(noteW, r.getWidth()), 26);
}

void HeaderBar::paint(juce::Graphics& g)
{
    const auto bounds = getLocalBounds().toFloat();
    g.setColour(Theme::bgSurface);
    g.fillRoundedRectangle(bounds, Theme::kCardRadius);
    g.setColour(Theme::borderSubtle);
    g.drawRoundedRectangle(bounds.reduced(0.5f), Theme::kCardRadius, 1.0f);

    // Brand logo
    const auto brandRect = getLocalBounds().reduced(10, 8).withWidth(98);
    const auto accent = (latestSnap.activeMode == dsp::EngineMode::Live) ? Theme::liveAccent : Theme::studioAccent;
    g.setColour(accent);
    g.fillEllipse(static_cast<float>(brandRect.getX()), static_cast<float>(brandRect.getCentreY() - 4), 8.0f, 8.0f);

    g.setColour(Theme::textPrimary);
    g.setFont(Theme::makeFont(13.5f, true));
    g.drawFittedText("HarmonizerV", brandRect.withTrimmedLeft(12), juce::Justification::centredLeft, 1);

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
