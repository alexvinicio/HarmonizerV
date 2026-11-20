#include "Ui/HeaderBar.h"
#include "Dsp/ScaleHarmonizer.h"
#include "Plugin/AppInfo.h"
#include "Plugin/ParameterIds.h"
#include "Ui/Localization.h"
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
            apvts.undoManager);
        modeAttachment->sendInitialUpdate();
    }

    if (auto* bufParam = apvts.getParameter(params::liveBuffer))
    {
        liveBufferAttachment = std::make_unique<juce::ParameterAttachment>(
            *bufParam,
            [this](float newValue) { handleLiveBufferChanged(newValue); },
            apvts.undoManager);
        liveBufferAttachment->sendInitialUpdate();
    }

    setMouseCursor(juce::MouseCursor::PointingHandCursor);
    refreshLocalization();
}

void EngineModePill::refreshLocalization()
{
    setTooltip(tr(dsp::TextKey::EngineTooltip));
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
    menu.addItem(1, tr(dsp::TextKey::BufUltra), true, currentLiveBuffer == 0);
    menu.addItem(2, tr(dsp::TextKey::BufFast),  true, currentLiveBuffer == 1);
    menu.addItem(3, tr(dsp::TextKey::BufLow),   true, currentLiveBuffer == 2);
    menu.addItem(4, tr(dsp::TextKey::BufSafe),  true, currentLiveBuffer == 3);

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
            apvts.undoManager);
        attachment->sendInitialUpdate();
    }
    setMouseCursor(juce::MouseCursor::PointingHandCursor);
    refreshLocalization();
}

void HarmonyModePill::refreshLocalization()
{
    setTooltip(tr(dsp::TextKey::HarmonyModeTooltip));
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
// PowerBypassButton
// ============================================================================

PowerBypassButton::PowerBypassButton(juce::AudioProcessorValueTreeState& apvts, LiveMidiController& liveMidi)
    : liveMidiController(liveMidi),
      undoManager(apvts.undoManager)
{
    if (auto* param = apvts.getParameter(params::bypass))
    {
        attachment = std::make_unique<juce::ParameterAttachment>(
            *param,
            [this](float newValue) { handleBypassChanged(newValue); },
            apvts.undoManager);
        attachment->sendInitialUpdate();
    }
    setMouseCursor(juce::MouseCursor::PointingHandCursor);
    refreshLocalization();
}

void PowerBypassButton::refreshLocalization()
{
    setTooltip(tr(dsp::TextKey::BypassTooltip));
}

void PowerBypassButton::setActiveColour(juce::Colour c)
{
    if (activeColour != c)
    {
        activeColour = c;
        repaint();
    }
}

void PowerBypassButton::handleBypassChanged(float newValue)
{
    bypassed = (newValue >= 0.5f);
    repaint();
}

void PowerBypassButton::mouseEnter(const juce::MouseEvent&)
{
    hovered = true;
    repaint();
}

void PowerBypassButton::mouseExit(const juce::MouseEvent&)
{
    hovered = false;
    repaint();
}

void PowerBypassButton::openContextMenu()
{
    const bool isLearning = (liveMidiController.getLearningTarget() == static_cast<int>(dsp::LiveMidiTarget::Bypass));
    const auto binding = liveMidiController.getBinding(dsp::LiveMidiTarget::Bypass);
    const bool hasBinding = dsp::isValidBinding(binding);

    juce::PopupMenu menu;
    menu.addItem(100, tr(dsp::TextKey::MidiPrefix) + liveMidiController.getBindingLabel(dsp::LiveMidiTarget::Bypass), false, false);
    menu.addSeparator();
    menu.addItem(1, isLearning ? tr(dsp::TextKey::CancelMidiLearn) : tr(dsp::TextKey::MidiLearnBypass), true, isLearning);
    menu.addItem(2, tr(dsp::TextKey::ClearMidiBinding), hasBinding, false);

    menu.showMenuAsync(
        juce::PopupMenu::Options().withTargetComponent(this),
        [safeThis = juce::Component::SafePointer<PowerBypassButton>(this), isLearning](int selectedId)
        {
            if (safeThis == nullptr || selectedId <= 0)
                return;
            if (selectedId == 1)
            {
                if (isLearning)
                    safeThis->liveMidiController.cancelLearn();
                else
                    safeThis->liveMidiController.startLearn(dsp::LiveMidiTarget::Bypass);
            }
            else if (selectedId == 2)
            {
                safeThis->liveMidiController.clearBinding(dsp::LiveMidiTarget::Bypass);
            }
            safeThis->repaint();
        });
}

void PowerBypassButton::mouseDown(const juce::MouseEvent& e)
{
    if (e.mods.isRightButtonDown())
    {
        openContextMenu();
        return;
    }

    if (liveMidiController.getLearningTarget() == static_cast<int>(dsp::LiveMidiTarget::Bypass))
    {
        liveMidiController.cancelLearn();
        repaint();
        return;
    }

    if (attachment == nullptr)
        return;

    if (undoManager != nullptr)
        undoManager->beginNewTransaction("Toggle Bypass");
    attachment->setValueAsCompleteGesture(bypassed ? 0.0f : 1.0f);
}

void PowerBypassButton::paint(juce::Graphics& g)
{
    const auto bounds = getLocalBounds().toFloat().reduced(1.0f);
    const bool learning = (liveMidiController.getLearningTarget() == static_cast<int>(dsp::LiveMidiTarget::Bypass));

    const auto col = learning ? Theme::warnAccent
                              : (bypassed ? Theme::muteAccent : activeColour);

    g.setColour(hovered ? Theme::bgElevated.brighter(0.10f) : Theme::bgElevated);
    g.fillEllipse(bounds);
    g.setColour(col.withAlpha(learning ? 0.95f : (hovered ? 0.85f : 0.45f)));
    g.drawEllipse(bounds, learning ? 1.5f : 1.0f);

    const auto c = bounds.getCentre();
    const float r = std::min(bounds.getWidth(), bounds.getHeight()) * 0.27f;

    juce::Path icon;
    icon.addCentredArc(c.x, c.y, r, r, 0.0f, 0.60f, juce::MathConstants<float>::twoPi - 0.60f, true);
    icon.startNewSubPath(c.x, c.y - r * 1.15f);
    icon.lineTo(c.x, c.y - r * 0.15f);

    g.setColour(bypassed ? col.withAlpha(0.75f) : col);
    g.strokePath(icon, juce::PathStrokeType(1.75f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
}

// ============================================================================
// HeaderBar
// ============================================================================

HeaderBar::HeaderBar(HarmonizerVProcessor& proc)
    : processor(proc),
      powerBypassBtn(proc.getApvts(), proc.getLiveMidiController()),
      enginePill(proc.getApvts()),
      harmonyModePill(proc.getApvts())
{
    addAndMakeVisible(powerBypassBtn);
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
        scaleCombo.addItem(trScale(s), s + 1);
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

    refreshLocalization();
}

void HeaderBar::refreshLocalization()
{
    enginePill.refreshLocalization();
    harmonyModePill.refreshLocalization();
    powerBypassBtn.refreshLocalization();

    prevPresetBtn.setTooltip(tr(dsp::TextKey::PrevPresetTooltip));
    nextPresetBtn.setTooltip(tr(dsp::TextKey::NextPresetTooltip));
    savePresetBtn.setButtonText(tr(dsp::TextKey::SaveBtn));
    savePresetBtn.setTooltip(tr(dsp::TextKey::SavePresetTooltip));

    mixKnob.setTooltip(tr(dsp::TextKey::KnobMixTooltip));
    outKnob.setTooltip(tr(dsp::TextKey::KnobOutTooltip));

    for (int s = 0; s < static_cast<int>(dsp::ScaleType::NumScales); ++s)
        scaleCombo.changeItemText(s + 1, trScale(s));

    if (brandHover == BrandHover::LangChip)
        setTooltip(tr(dsp::TextKey::LangTooltip));
    else if (brandHover == BrandHover::Title || brandHover == BrandHover::AboutChip)
        setTooltip(tr(dsp::TextKey::AboutTooltip));

    repaint();
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
    auto* aw = new juce::AlertWindow(tr(dsp::TextKey::SaveDialogTitle),
                                     tr(dsp::TextKey::SaveDialogMessage),
                                     juce::MessageBoxIconType::NoIcon);
    aw->addTextEditor("presetName", tr(dsp::TextKey::SaveDialogDefault));
    aw->addButton(tr(dsp::TextKey::SaveBtn), 1, juce::KeyPress(juce::KeyPress::returnKey));
    aw->addButton(tr(dsp::TextKey::CancelBtn), 0, juce::KeyPress(juce::KeyPress::escapeKey));

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

    const auto modeAccent = (snap.activeMode == dsp::EngineMode::Live) ? Theme::liveAccent : Theme::studioAccent;
    powerBypassBtn.setActiveColour(modeAccent);
    mixKnob.setAccentColour(modeAccent);

    // Smooth decay output peak meters + peak hold (~0.8 s at 30 Hz)
    meterLevelL = std::max(snap.outputPeakL, meterLevelL * 0.80f);
    meterLevelR = std::max(snap.outputPeakR, meterLevelR * 0.80f);

    if (snap.outputPeakL >= peakHoldL)
    {
        peakHoldL = snap.outputPeakL;
        peakHoldTimerL = 24;
    }
    else if (peakHoldTimerL > 0)
    {
        --peakHoldTimerL;
    }
    else
    {
        peakHoldL *= 0.92f;
    }

    if (snap.outputPeakR >= peakHoldR)
    {
        peakHoldR = snap.outputPeakR;
        peakHoldTimerR = 24;
    }
    else if (peakHoldTimerR > 0)
    {
        --peakHoldTimerR;
    }
    else
    {
        peakHoldR *= 0.92f;
    }

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

    // Left side bypass button + brand wordmark + EN/ES + About + presets
    powerBypassBtn.setBounds(r.removeFromLeft(22).withSizeKeepingCentre(22, 22));
    r.removeFromLeft(4);
    const auto brandCol = r.removeFromLeft(94).withSizeKeepingCentre(94, 42).toFloat();
    brandTitleRect = brandCol.withHeight(22.0f);
    const auto subRow = brandCol.withTrimmedTop(24.0f).withHeight(16.0f);
    langChipRect = subRow.withWidth(44.0f);
    aboutChipRect = subRow.withTrimmedLeft(48.0f);
    r.removeFromLeft(4);
    prevPresetBtn.setBounds(r.removeFromLeft(20).withSizeKeepingCentre(20, 26));
    r.removeFromLeft(2);

    constexpr int fixedMiddleW = 235;
    const int flexAvail = std::max(170, r.getWidth() - fixedMiddleW);
    const int presetWidth = std::clamp(flexAvail * 34 / 100, 70, 150);
    const int scaleW      = std::clamp(flexAvail * 33 / 100, 68, 122);
    const int noteW       = std::clamp(flexAvail - presetWidth - scaleW, 62, 98);

    presetCombo.setBounds(r.removeFromLeft(presetWidth).withSizeKeepingCentre(presetWidth, 26));
    r.removeFromLeft(2);
    nextPresetBtn.setBounds(r.removeFromLeft(20).withSizeKeepingCentre(20, 26));
    r.removeFromLeft(3);
    savePresetBtn.setBounds(r.removeFromLeft(48).withSizeKeepingCentre(48, 26));
    r.removeFromLeft(8);

    // Center Harmony Mode / Key / Scale / Detected note pill
    harmonyModePill.setBounds(r.removeFromLeft(84).withSizeKeepingCentre(84, 26));
    r.removeFromLeft(6);
    keyCombo.setBounds(r.removeFromLeft(56).withSizeKeepingCentre(56, 26));
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

    const auto modeAccent = (latestSnap.activeMode == dsp::EngineMode::Live) ? Theme::liveAccent : Theme::studioAccent;

    // Brand wordmark (clickable to open About)
    const auto brandTextCol = latestSnap.bypassed
        ? Theme::textMuted
        : (brandHover == BrandHover::Title ? modeAccent : Theme::textPrimary);
    g.setColour(brandTextCol);
    g.setFont(Theme::makeFont(13.0f, true));
    g.drawFittedText(appinfo::kAppName, brandTitleRect.toNearestInt(), juce::Justification::centredLeft, 1);

    // EN | ES segmented language micro-pill
    if (!langChipRect.isEmpty())
    {
        const bool isEs = (LocalizationManager::getInstance().getLanguage() == dsp::Language::Spanish);
        const bool hovered = (brandHover == BrandHover::LangChip);

        g.setColour(Theme::bgCanvas);
        g.fillRoundedRectangle(langChipRect, 4.0f);
        g.setColour(hovered ? modeAccent.withAlpha(0.75f) : Theme::borderSubtle);
        g.drawRoundedRectangle(langChipRect.reduced(0.5f), 4.0f, 1.0f);

        const float hw = langChipRect.getWidth() * 0.5f;
        const auto enRect = langChipRect.withWidth(hw).reduced(1.5f);
        const auto esRect = langChipRect.withTrimmedLeft(hw).reduced(1.5f);

        g.setColour(modeAccent.withAlpha(0.88f));
        g.fillRoundedRectangle(isEs ? esRect : enRect, 3.0f);

        const auto activeTxtCol = (latestSnap.activeMode == dsp::EngineMode::Live) ? Theme::bgCanvas : Theme::textPrimary;
        g.setFont(Theme::makeFont(8.8f, true));
        g.setColour(!isEs ? activeTxtCol : Theme::textSecondary);
        g.drawFittedText("EN", enRect.toNearestInt(), juce::Justification::centred, 1);
        g.setColour(isEs ? activeTxtCol : Theme::textSecondary);
        g.drawFittedText("ES", esRect.toNearestInt(), juce::Justification::centred, 1);
    }

    // About / Acerca micro-chip
    if (!aboutChipRect.isEmpty())
    {
        const bool hovered = (brandHover == BrandHover::AboutChip);
        g.setColour(hovered ? Theme::bgElevated.brighter(0.12f) : Theme::bgElevated);
        g.fillRoundedRectangle(aboutChipRect, 4.0f);
        g.setColour(hovered ? modeAccent.withAlpha(0.75f) : Theme::borderFocus);
        g.drawRoundedRectangle(aboutChipRect.reduced(0.5f), 4.0f, 1.0f);

        g.setColour(hovered ? Theme::textPrimary : Theme::textSecondary);
        g.setFont(Theme::makeFont(9.0f, true));
        g.drawFittedText(tr(dsp::TextKey::AboutBtn), aboutChipRect.toNearestInt(), juce::Justification::centred, 1);
    }

    // Live detected pitch pill with intonation deviation strip
    if (!notePillBounds.isEmpty())
    {
        const auto np = notePillBounds.toFloat();
        g.setColour(Theme::bgElevated);
        g.fillRoundedRectangle(np, Theme::kPillRadius);
        g.setColour(latestSnap.isVoiced ? modeAccent.withAlpha(0.6f) : Theme::borderSubtle);
        g.drawRoundedRectangle(np.reduced(0.5f), Theme::kPillRadius, 1.0f);

        juce::String noteText = "--  --- Hz";
        if (latestSnap.isVoiced && latestSnap.detectedHz > 20.0f)
        {
            noteText = midiToNoteName(latestSnap.detectedMidi)
                     + " | " + juce::String(std::lround(latestSnap.detectedHz)) + " Hz";
        }
        g.setColour(latestSnap.isVoiced ? Theme::textPrimary : Theme::textMuted);
        g.setFont(Theme::makeFont(11.0f, latestSnap.isVoiced));
        const auto textBounds = latestSnap.isVoiced
            ? notePillBounds.reduced(4, 0).translated(0, -2)
            : notePillBounds.reduced(4, 0);
        g.drawFittedText(noteText, textBounds, juce::Justification::centred, 1);

        if (latestSnap.isVoiced && latestSnap.detectedHz > 20.0f)
        {
            const auto track = juce::Rectangle<float>(np.getX() + 8.0f,
                                                      np.getBottom() - 4.0f,
                                                      std::max(12.0f, np.getWidth() - 16.0f),
                                                      2.2f);
            g.setColour(Theme::bgCanvas);
            g.fillRoundedRectangle(track, 1.1f);
            g.setColour(Theme::borderFocus);
            g.fillRect(track.getCentreX() - 0.5f, track.getY(), 1.0f, track.getHeight());

            const float devCents = (latestSnap.detectedMidi - std::round(latestSnap.detectedMidi)) * 100.0f;
            const float norm = std::clamp(devCents / 50.0f, -1.0f, 1.0f);
            const auto indCol = (std::abs(devCents) <= 12.0f) ? Theme::liveAccent : Theme::warnAccent;

            const float halfW = track.getWidth() * 0.5f;
            const float barW = std::max(3.0f, std::abs(norm) * halfW);
            const float barX = (norm >= 0.0f) ? track.getCentreX() : (track.getCentreX() - barW);
            g.setColour(indCol.withAlpha(0.85f));
            g.fillRoundedRectangle(juce::Rectangle<float>(barX, track.getY(), barW, track.getHeight()), 1.0f);

            const float dotX = std::clamp(track.getCentreX() + norm * (halfW - 1.5f), track.getX() + 1.5f, track.getRight() - 1.5f);
            g.setColour(indCol);
            g.fillEllipse(dotX - 1.6f, track.getCentreY() - 1.6f, 3.2f, 3.2f);
        }
    }

    // Compact stereo output peak meter + peak hold indicators
    if (!meterBounds.isEmpty())
    {
        const auto mf = meterBounds.toFloat();
        g.setColour(Theme::bgCanvas);
        g.fillRoundedRectangle(mf, 3.0f);

        auto drawBar = [&](float level, float peakHold, float xPos, float w)
        {
            const float db = std::clamp(dsp::gainToDb(level), -48.0f, 3.0f);
            const float norm = (db + 48.0f) / 51.0f;
            const float h = mf.getHeight() * norm;
            const auto barRect = juce::Rectangle<float>(xPos, mf.getBottom() - h, w, h);
            g.setColour(db > -1.0f ? Theme::muteAccent : (db > -6.0f ? Theme::warnAccent : Theme::liveAccent));
            g.fillRoundedRectangle(barRect, 1.5f);

            if (peakHold > 0.004f)
            {
                const float holdDb = std::clamp(dsp::gainToDb(peakHold), -48.0f, 3.0f);
                const float holdNorm = (holdDb + 48.0f) / 51.0f;
                const float holdY = std::clamp(mf.getBottom() - mf.getHeight() * holdNorm, mf.getY(), mf.getBottom() - 1.5f);
                g.setColour(holdDb > -1.0f ? Theme::muteAccent : (holdDb > -6.0f ? Theme::warnAccent : Theme::textPrimary.withAlpha(0.85f)));
                g.fillRect(xPos, holdY, w, 1.5f);
            }
        };

        const float barW = (mf.getWidth() - 3.0f) * 0.5f;
        drawBar(meterLevelL, peakHoldL, mf.getX() + 1.0f, barW);
        drawBar(meterLevelR, peakHoldR, mf.getX() + 2.0f + barW, barW);
    }
}

void HeaderBar::mouseMove(const juce::MouseEvent& e)
{
    BrandHover next = BrandHover::None;
    if (langChipRect.contains(e.position))
        next = BrandHover::LangChip;
    else if (aboutChipRect.contains(e.position))
        next = BrandHover::AboutChip;
    else if (brandTitleRect.contains(e.position))
        next = BrandHover::Title;

    setMouseCursor(next != BrandHover::None ? juce::MouseCursor::PointingHandCursor
                                            : juce::MouseCursor::NormalCursor);

    if (next != brandHover)
    {
        brandHover = next;
        if (brandHover == BrandHover::LangChip)
            setTooltip(tr(dsp::TextKey::LangTooltip));
        else if (brandHover == BrandHover::AboutChip || brandHover == BrandHover::Title)
            setTooltip(tr(dsp::TextKey::AboutTooltip));
        else
            setTooltip({});
        repaint();
    }
}

void HeaderBar::mouseExit(const juce::MouseEvent&)
{
    setMouseCursor(juce::MouseCursor::NormalCursor);
    if (brandHover != BrandHover::None)
    {
        brandHover = BrandHover::None;
        setTooltip({});
        repaint();
    }
}

void HeaderBar::mouseDown(const juce::MouseEvent& e)
{
    if (langChipRect.contains(e.position))
    {
        const auto targetLang = (e.position.x >= langChipRect.getCentreX())
            ? dsp::Language::Spanish
            : dsp::Language::English;
        LocalizationManager::getInstance().setLanguage(targetLang);
        repaint();
        return;
    }

    if (aboutChipRect.contains(e.position) || brandTitleRect.contains(e.position))
    {
        if (onAboutClicked)
            onAboutClicked();
    }
}

} // namespace harmonizer::ui
