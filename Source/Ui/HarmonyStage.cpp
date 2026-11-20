#include "Ui/HarmonyStage.h"
#include "Dsp/ScaleHarmonizer.h"
#include "Dsp/VoicingPresets.h"
#include "Plugin/ParameterIds.h"
#include "Ui/Theme.h"

#include <algorithm>
#include <array>
#include <cmath>

namespace harmonizer::ui
{

HarmonyStage::HarmonyStage(HarmonizerVProcessor& proc)
    : processor(proc)
{
    setMouseCursor(juce::MouseCursor::NormalCursor);
    setTooltip("Drag orbs: Pan/Pitch | Wheel: Level | 4V/8V voices | VOICING menu | Undo/Redo | Snapshots A-D");
}

int HarmonyStage::getVisibleVoiceCount() const noexcept
{
    return (getParamPlain(params::voiceBank, 0.0f) >= 0.5f) ? dsp::kMaxVoices : dsp::kExposedVoicesV1;
}

juce::String HarmonyStage::formatMidiNoteName(int note)
{
    if (note < 0 || note > 127)
        return "--";
    static constexpr std::array<const char*, 12> kNotes = {
        "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"
    };
    return juce::String(kNotes[static_cast<size_t>(note % 12)]) + juce::String((note / 12) - 1);
}

void HarmonyStage::setSelectedVoice(int voiceIndex)
{
    const int limit = getVisibleVoiceCount();
    const int clamped = std::clamp(voiceIndex, 0, limit - 1);
    if (selectedVoice != clamped)
    {
        selectedVoice = clamped;
        repaint();
    }
}

void HarmonyStage::updateTelemetry(const dsp::TelemetrySnapshot& snap)
{
    latestSnap = snap;
    const int vis = getVisibleVoiceCount();
    if (vis != lastVisibleVoices)
        updateHeaderLayout();
    if (selectedVoice >= vis)
    {
        selectedVoice = 0;
        if (onVoiceSelected)
            onVoiceSelected(selectedVoice);
    }
    repaint();
}

float HarmonyStage::getParamPlain(const juce::String& id, float fallback) const noexcept
{
    if (auto* raw = processor.getApvts().getRawParameterValue(id))
        return raw->load(std::memory_order_relaxed);
    return fallback;
}

void HarmonyStage::setParamPlain(const juce::String& id, float plainVal, bool notifyGesture)
{
    if (auto* param = processor.getApvts().getParameter(id))
    {
        const float norm = param->convertTo0to1(plainVal);
        if (notifyGesture)
            param->beginChangeGesture();
        param->setValueNotifyingHost(norm);
        if (notifyGesture)
            param->endChangeGesture();
    }
}

juce::Rectangle<float> HarmonyStage::getStageArea() const noexcept
{
    auto r = getLocalBounds().toFloat().reduced(14.0f, 12.0f);
    r.removeFromTop(30.0f);    // Space for quick voice pills
    r.removeFromLeft(50.0f);   // Space for Y interval axis labels
    r.removeFromRight(18.0f);
    r.removeFromBottom(20.0f); // Space for L / Center / R axis labels
    return r;
}

float HarmonyStage::panToX(float panPct) const noexcept
{
    const auto area = getStageArea();
    const float norm = (std::clamp(panPct, -100.0f, 100.0f) + 100.0f) / 200.0f;
    return area.getX() + norm * area.getWidth();
}

float HarmonyStage::intervalToY(int interval) const noexcept
{
    const auto area = getStageArea();
    const float norm = (static_cast<float>(std::clamp(interval, -14, 14)) + 14.0f) / 28.0f;
    return area.getBottom() - norm * area.getHeight();
}

float HarmonyStage::xToPan(float x) const noexcept
{
    const auto area = getStageArea();
    const float norm = std::clamp((x - area.getX()) / std::max(1.0f, area.getWidth()), 0.0f, 1.0f);
    return norm * 200.0f - 100.0f;
}

int HarmonyStage::yToInterval(float y) const noexcept
{
    const auto area = getStageArea();
    const float norm = std::clamp((area.getBottom() - y) / std::max(1.0f, area.getHeight()), 0.0f, 1.0f);
    return std::clamp(static_cast<int>(std::lround(norm * 28.0f - 14.0f)), -14, 14);
}

float HarmonyStage::voiceRadius(int voiceIndex) const noexcept
{
    const float gainDb = getParamPlain(params::voiceParamId(voiceIndex, params::vGainDb), -6.0f);
    const float norm = std::clamp((gainDb + 42.0f) / 48.0f, 0.0f, 1.0f);
    return 14.0f + norm * 11.0f;
}

juce::Point<float> HarmonyStage::voiceToPoint(int voiceIndex) const noexcept
{
    const float pan = getParamPlain(params::voiceParamId(voiceIndex, params::vPan), 0.0f);
    if (latestSnap.activeHarmonyMode == dsp::HarmonyMode::Midi)
    {
        const int semi = latestSnap.voiceMidiActive[static_cast<size_t>(voiceIndex)]
            ? std::clamp(static_cast<int>(std::lround(latestSnap.voiceTargetSemitones[static_cast<size_t>(voiceIndex)])), -14, 14)
            : 0;
        return { panToX(pan), intervalToY(semi) };
    }
    const bool isFixed = getParamPlain(params::voiceParamId(voiceIndex, params::vFixed), 0.0f) >= 0.5f;
    if (isFixed)
    {
        const int fixedNote = static_cast<int>(std::lround(getParamPlain(params::voiceParamId(voiceIndex, params::vFixedNote), 60.0f)));
        const int rel = std::clamp(fixedNote - 60, -14, 14);
        return { panToX(pan), intervalToY(rel) };
    }
    const int interval = static_cast<int>(std::lround(getParamPlain(params::voiceParamId(voiceIndex, params::vInterval), 0.0f)));
    return { panToX(pan), intervalToY(interval) };
}

int HarmonyStage::hitTestOrb(juce::Point<float> pos) const noexcept
{
    const int limit = getVisibleVoiceCount();
    // Test selected voice last/priority, or closest orb within hit radius
    for (int pass = 0; pass < 2; ++pass)
    {
        for (int v = limit - 1; v >= 0; --v)
        {
            if (pass == 0 && v != selectedVoice)
                continue;
            const auto centre = voiceToPoint(v);
            const float r = voiceRadius(v) + 7.0f;
            if (pos.getDistanceFrom(centre) <= r)
                return v;
        }
    }
    return -1;
}

void HarmonyStage::updateHeaderLayout() noexcept
{
    const auto top = getLocalBounds().toFloat().reduced(14.0f, 10.0f).removeFromTop(26.0f);
    const int visible = getVisibleVoiceCount();
    lastVisibleVoices = visible;

    const float pillW = (visible == dsp::kMaxVoices) ? 36.0f : 50.0f;
    const float pillStep = (visible == dsp::kMaxVoices) ? 39.0f : 54.0f;
    float x = top.getX();
    for (int v = 0; v < dsp::kMaxVoices; ++v)
    {
        if (v < visible)
        {
            pillRects[static_cast<size_t>(v)] = juce::Rectangle<float>(x, top.getY(), pillW, 24.0f);
            x += pillStep;
        }
        else
        {
            pillRects[static_cast<size_t>(v)] = {};
        }
    }

    const float lastRight = pillRects[static_cast<size_t>(visible - 1)].getRight();
    bankToggleRect = juce::Rectangle<float>(lastRight + 6.0f, top.getY(), 48.0f, 24.0f);
    voicingBtnRect = juce::Rectangle<float>(bankToggleRect.getRight() + 5.0f, top.getY(), 72.0f, 24.0f);

    constexpr float kSnapW = 26.0f;
    constexpr float kSnapGap = 4.0f;
    constexpr float kTotalSnapsW = dsp::kNumSnapshots * kSnapW + (dsp::kNumSnapshots - 1) * kSnapGap; // 116 px
    float sx = top.getRight() - kTotalSnapsW;
    for (int s = 0; s < dsp::kNumSnapshots; ++s)
    {
        snapPillRects[static_cast<size_t>(s)] = juce::Rectangle<float>(sx, top.getY(), kSnapW, 24.0f);
        sx += kSnapW + kSnapGap;
    }

    snapLabelRect = juce::Rectangle<float>(snapPillRects[0].getX() - 38.0f, top.getY(), 34.0f, 24.0f);
    redoBtnRect   = juce::Rectangle<float>(snapLabelRect.getX() - 32.0f, top.getY(), 22.0f, 24.0f);
    undoBtnRect   = juce::Rectangle<float>(redoBtnRect.getX() - 25.0f, top.getY(), 22.0f, 24.0f);
    hintRect      = juce::Rectangle<float>(voicingBtnRect.getRight() + 6.0f,
                                           top.getY(),
                                           std::max(0.0f, undoBtnRect.getX() - voicingBtnRect.getRight() - 12.0f),
                                           24.0f);
}

void HarmonyStage::resized()
{
    updateHeaderLayout();
}

void HarmonyStage::showVoicingMenu()
{
    juce::PopupMenu menu;
    for (int i = 0; i < static_cast<int>(dsp::kVoicingPresets.size()); ++i)
    {
        if (i == 5)
            menu.addSeparator();
        menu.addItem(i + 1, dsp::kVoicingPresets[static_cast<size_t>(i)].name);
    }

    menu.showMenuAsync(
        juce::PopupMenu::Options().withTargetScreenArea(localAreaToGlobal(voicingBtnRect.toNearestInt())),
        [safeThis = juce::Component::SafePointer<HarmonyStage>(this)](int selectedId)
        {
            if (safeThis != nullptr && selectedId > 0)
            {
                safeThis->processor.getPresetManager().applyVoicingPreset(selectedId - 1);
                safeThis->updateHeaderLayout();
                safeThis->repaint();
            }
        });
}

void HarmonyStage::showSnapshotContextMenu(int slotIndex)
{
    static constexpr std::array<const char*, 4> kLetters = { "A", "B", "C", "D" };
    if (slotIndex < 0 || slotIndex >= dsp::kNumSnapshots)
        return;

    const auto& snapMgr = processor.getSnapshotManager();
    const auto& liveMidi = processor.getLiveMidiController();
    const int activeIdx = snapMgr.getActiveSnapshotIndex();
    const juce::String targetLetter = kLetters[static_cast<size_t>(slotIndex)];
    const auto midiTarget = static_cast<dsp::LiveMidiTarget>(slotIndex);
    const bool isLearning = (liveMidi.getLearningTarget() == slotIndex);
    const bool hasBinding = dsp::isValidBinding(liveMidi.getBinding(midiTarget));

    juce::PopupMenu menu;
    menu.addItem(1, "Store Current into " + targetLetter);
    menu.addSeparator();

    for (int other = 0; other < dsp::kNumSnapshots; ++other)
    {
        if (other != slotIndex && snapMgr.isSnapshotPopulated(other))
            menu.addItem(10 + other, "Copy from " + juce::String(kLetters[static_cast<size_t>(other)]));
    }

    menu.addSeparator();
    const bool canClear = snapMgr.isSnapshotPopulated(slotIndex) && (slotIndex != activeIdx);
    menu.addItem(20, "Clear " + targetLetter, canClear, false);
    menu.addSeparator();
    menu.addItem(100, "MIDI: " + liveMidi.getBindingLabel(midiTarget), false, false);
    menu.addItem(30, isLearning ? "Cancel MIDI Learn" : ("MIDI Learn " + targetLetter), true, isLearning);
    menu.addItem(31, "Reset MIDI to PC " + juce::String(slotIndex + 1));
    menu.addItem(32, "Clear MIDI Binding", hasBinding, false);

    menu.showMenuAsync(
        juce::PopupMenu::Options().withTargetScreenArea(localAreaToGlobal(snapPillRects[static_cast<size_t>(slotIndex)].toNearestInt())),
        [safeThis = juce::Component::SafePointer<HarmonyStage>(this), slotIndex, isLearning, midiTarget](int selectedId)
        {
            if (safeThis == nullptr || selectedId <= 0)
                return;

            auto& mgr = safeThis->processor.getSnapshotManager();
            auto& midi = safeThis->processor.getLiveMidiController();

            if (selectedId == 1)
                mgr.storeSnapshot(slotIndex);
            else if (selectedId >= 10 && selectedId < 10 + dsp::kNumSnapshots)
                mgr.copySnapshot(selectedId - 10, slotIndex);
            else if (selectedId == 20)
                mgr.clearSnapshot(slotIndex);
            else if (selectedId == 30)
            {
                if (isLearning)
                    midi.cancelLearn();
                else
                    midi.startLearn(midiTarget);
            }
            else if (selectedId == 31)
                midi.resetDefaultBinding(midiTarget);
            else if (selectedId == 32)
                midi.clearBinding(midiTarget);

            safeThis->repaint();
        });
}

void HarmonyStage::paint(juce::Graphics& g)
{
    const int visibleVoices = getVisibleVoiceCount();
    if (visibleVoices != lastVisibleVoices)
        updateHeaderLayout();

    const auto card = getLocalBounds().toFloat();
    g.setColour(Theme::bgSurface);
    g.fillRoundedRectangle(card, Theme::kCardRadius);
    g.setColour(Theme::borderSubtle);
    g.drawRoundedRectangle(card.reduced(0.5f), Theme::kCardRadius, 1.0f);

    const auto area = getStageArea();
    const int scaleIdx = static_cast<int>(std::lround(getParamPlain(params::scale, 0.0f)));
    const auto scaleType = static_cast<dsp::ScaleType>(scaleIdx);
    const bool isMidiMode = (latestSnap.activeHarmonyMode == dsp::HarmonyMode::Midi);
    const auto modeAccent = (latestSnap.activeMode == dsp::EngineMode::Live) ? Theme::liveAccent : Theme::studioAccent;

    // Quick Voice Select Pills (V1..V4 or V1..V8)
    for (int v = 0; v < visibleVoices; ++v)
    {
        const auto pr = pillRects[static_cast<size_t>(v)];
        const bool isSelected = (v == selectedVoice);
        const bool isHovered = (!isSelected && hoverZone == HoverZone::VoicePill && hoverIndex == v);
        const bool midiActive = latestSnap.voiceMidiActive[static_cast<size_t>(v)];
        const bool isEnabled = isMidiMode ? midiActive : (getParamPlain(params::voiceParamId(v, params::vEnabled), 0.0f) >= 0.5f);
        const bool isMuted = getParamPlain(params::voiceParamId(v, params::vMute), 0.0f) >= 0.5f;
        const auto col = Theme::getVoiceColour(v);

        g.setColour(isSelected ? Theme::bgElevated : (isHovered ? Theme::bgElevated.withAlpha(0.70f) : Theme::bgCanvas.withAlpha(0.75f)));
        g.fillRoundedRectangle(pr, 6.0f);
        g.setColour(isSelected ? col : (isHovered ? col.withAlpha(0.55f) : Theme::borderSubtle));
        g.drawRoundedRectangle(pr.reduced(0.5f), 6.0f, isSelected ? 1.5f : 1.0f);

        const float dotSize = (visibleVoices > 4) ? 6.0f : 8.0f;
        const float dotX = (visibleVoices > 4) ? 5.0f : 8.0f;
        const auto dotRect = juce::Rectangle<float>(pr.getX() + dotX, pr.getCentreY() - dotSize * 0.5f, dotSize, dotSize);
        if (isEnabled && !isMuted)
        {
            g.setColour(col);
            g.fillEllipse(dotRect);
        }
        else
        {
            g.setColour(isMuted ? Theme::muteAccent : Theme::textMuted);
            g.drawEllipse(dotRect, 1.3f);
        }

        g.setColour(isEnabled ? Theme::textPrimary : Theme::textMuted);
        g.setFont(Theme::makeFont(visibleVoices > 4 ? 10.5f : 11.5f, isSelected));
        g.drawFittedText("V" + juce::String(v + 1),
                         pr.withTrimmedLeft(dotX + dotSize + 3.0f).toNearestInt(),
                         juce::Justification::centredLeft, 1);
    }

    // 4V | 8V segmented bank pill
    {
        const bool bankHovered = (hoverZone == HoverZone::BankToggle);
        g.setColour(Theme::bgCanvas.withAlpha(0.75f));
        g.fillRoundedRectangle(bankToggleRect, 6.0f);
        g.setColour(bankHovered ? Theme::borderFocus.brighter(0.25f) : Theme::borderSubtle);
        g.drawRoundedRectangle(bankToggleRect.reduced(0.5f), 6.0f, 1.0f);

        const float hw = bankToggleRect.getWidth() * 0.5f;
        const auto r4 = bankToggleRect.withWidth(hw).reduced(1.5f);
        const auto r8 = bankToggleRect.withTrimmedLeft(hw).reduced(1.5f);

        g.setColour(modeAccent.withAlpha(0.90f));
        g.fillRoundedRectangle(visibleVoices > 4 ? r8 : r4, 4.5f);

        g.setFont(Theme::makeFont(10.0f, true));
        g.setColour(visibleVoices == 4
            ? (latestSnap.activeMode == dsp::EngineMode::Live ? Theme::bgCanvas : Theme::textPrimary)
            : Theme::textSecondary);
        g.drawFittedText("4V", r4.toNearestInt(), juce::Justification::centred, 1);

        g.setColour(visibleVoices > 4
            ? (latestSnap.activeMode == dsp::EngineMode::Live ? Theme::bgCanvas : Theme::textPrimary)
            : Theme::textSecondary);
        g.drawFittedText("8V", r8.toNearestInt(), juce::Justification::centred, 1);
    }

    // VOICING selector pill
    {
        const bool voicingHovered = (hoverZone == HoverZone::VoicingBtn);
        g.setColour(voicingHovered ? Theme::bgElevated.brighter(0.10f) : Theme::bgElevated);
        g.fillRoundedRectangle(voicingBtnRect, 6.0f);
        g.setColour(voicingHovered ? modeAccent.withAlpha(0.70f) : Theme::borderFocus);
        g.drawRoundedRectangle(voicingBtnRect.reduced(0.5f), 6.0f, 1.0f);

        g.setColour(Theme::textPrimary);
        g.setFont(Theme::makeFont(10.0f, true));
        g.drawFittedText("VOICING", voicingBtnRect.withTrimmedRight(13.0f).reduced(4.0f, 0.0f).toNearestInt(),
                         juce::Justification::centred, 1);

        const float cx = voicingBtnRect.getRight() - 8.5f;
        const float cy = voicingBtnRect.getCentreY();
        juce::Path chev;
        chev.addTriangle(cx - 2.6f, cy - 1.4f, cx + 2.6f, cy - 1.4f, cx, cy + 1.9f);
        g.setColour(Theme::textSecondary);
        g.fillPath(chev);
    }

    // Undo / Redo compact vector buttons
    const auto& um = processor.getUndoManager();
    auto drawUndoRedoBtn = [&](const juce::Rectangle<float>& rect, bool isUndo, bool enabled, bool hovered)
    {
        g.setColour((enabled && hovered) ? Theme::bgElevated : Theme::bgCanvas.withAlpha(0.75f));
        g.fillRoundedRectangle(rect, 5.5f);
        g.setColour(enabled ? (hovered ? modeAccent.withAlpha(0.65f) : Theme::borderFocus) : Theme::borderSubtle.withAlpha(0.6f));
        g.drawRoundedRectangle(rect.reduced(0.5f), 5.5f, 1.0f);

        const auto c = rect.getCentre();
        const float sign = isUndo ? -1.0f : 1.0f; // -1 = points left (Undo), +1 = points right (Redo)
        const float tipX = c.x + sign * 4.8f;
        const float tipY = c.y - 2.0f;
        const float loopX = c.x - sign * 4.2f;

        juce::Path p;
        p.startNewSubPath(c.x, c.y + 4.2f);
        p.lineTo(c.x - sign * 1.2f, c.y + 4.2f);
        p.quadraticTo(loopX, c.y + 4.2f, loopX, c.y + 1.1f);
        p.quadraticTo(loopX, tipY, c.x - sign * 1.2f, tipY);
        p.lineTo(tipX, tipY);

        p.startNewSubPath(tipX - sign * 2.6f, tipY - 2.5f);
        p.lineTo(tipX, tipY);
        p.lineTo(tipX - sign * 2.6f, tipY + 2.5f);

        g.setColour(enabled ? Theme::textPrimary : Theme::textMuted.withAlpha(0.38f));
        g.strokePath(p, juce::PathStrokeType(1.45f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    };

    drawUndoRedoBtn(undoBtnRect, true,  um.canUndo(), hoverZone == HoverZone::UndoBtn);
    drawUndoRedoBtn(redoBtnRect, false, um.canRedo(), hoverZone == HoverZone::RedoBtn);

    // Snapshot Scene Pills (A / B / C / D) top-right
    const auto& snapMgr = processor.getSnapshotManager();
    const auto& liveMidi = processor.getLiveMidiController();
    const int learningTarget = liveMidi.getLearningTarget();
    const int activeSnap = snapMgr.getActiveSnapshotIndex();

    g.setColour(Theme::textMuted);
    g.setFont(Theme::makeFont(9.5f, true));
    g.drawFittedText("SNAP", snapLabelRect.toNearestInt(), juce::Justification::centredRight, 1);

    static constexpr std::array<const char*, 4> kSnapLetters = { "A", "B", "C", "D" };
    for (int s = 0; s < dsp::kNumSnapshots; ++s)
    {
        const auto sr = snapPillRects[static_cast<size_t>(s)];
        const bool isLearning = (learningTarget == s);
        const bool isActive = (s == activeSnap);
        const bool isPopulated = snapMgr.isSnapshotPopulated(s);
        const bool isSnapHovered = (hoverZone == HoverZone::SnapPill && hoverIndex == s);

        if (isLearning)
        {
            g.setColour(Theme::warnAccent.withAlpha(0.28f));
            g.fillRoundedRectangle(sr, 5.5f);
            g.setColour(Theme::warnAccent);
            g.drawRoundedRectangle(sr.reduced(0.5f), 5.5f, 1.5f);
            g.setFont(Theme::makeFont(11.0f, true));
        }
        else if (isActive)
        {
            g.setColour(modeAccent);
            g.fillRoundedRectangle(sr, 5.5f);
            g.setColour(latestSnap.activeMode == dsp::EngineMode::Live ? Theme::bgCanvas : Theme::textPrimary);
            g.setFont(Theme::makeFont(11.0f, true));
        }
        else if (isPopulated)
        {
            g.setColour(isSnapHovered ? Theme::bgElevated.brighter(0.10f) : Theme::bgElevated);
            g.fillRoundedRectangle(sr, 5.5f);
            g.setColour(isSnapHovered ? modeAccent.withAlpha(0.70f) : Theme::borderFocus);
            g.drawRoundedRectangle(sr.reduced(0.5f), 5.5f, 1.0f);

            g.setColour(modeAccent.withAlpha(0.75f));
            g.fillEllipse(sr.getCentreX() - 1.5f, sr.getBottom() - 4.5f, 3.0f, 3.0f);

            g.setColour(Theme::textPrimary);
            g.setFont(Theme::makeFont(11.0f, true));
        }
        else
        {
            g.setColour(isSnapHovered ? Theme::bgElevated.withAlpha(0.70f) : Theme::bgCanvas.withAlpha(0.75f));
            g.fillRoundedRectangle(sr, 5.5f);
            g.setColour(isSnapHovered ? modeAccent.withAlpha(0.55f) : Theme::borderSubtle);
            g.drawRoundedRectangle(sr.reduced(0.5f), 5.5f, 1.0f);

            g.setColour(Theme::textMuted);
            g.setFont(Theme::makeFont(11.0f, false));
        }

        g.drawFittedText(kSnapLetters[static_cast<size_t>(s)],
                         sr.toNearestInt(),
                         juce::Justification::centred, 1);
    }

    // Subtle hint or active MIDI Learn prompt centered cleanly between voice pills and undo/snap
    if (hintRect.getWidth() >= 140.0f)
    {
        juce::String hintText;
        if (learningTarget >= 0 && learningTarget < dsp::kNumSnapshots)
        {
            g.setColour(Theme::warnAccent);
            g.setFont(Theme::makeFont(10.0f, true));
            hintText = "MIDI LEARN: Send PC, CC, or Note for Snap " + juce::String(kSnapLetters[static_cast<size_t>(learningTarget)]);
        }
        else if (learningTarget == static_cast<int>(dsp::LiveMidiTarget::Bypass))
        {
            g.setColour(Theme::warnAccent);
            g.setFont(Theme::makeFont(10.0f, true));
            hintText = "MIDI LEARN: Send PC, CC, or Note for Bypass";
        }
        else
        {
            g.setColour(isMidiMode ? Theme::getVoiceColour(0).withAlpha(0.85f) : Theme::textMuted);
            g.setFont(Theme::makeFont(10.0f, isMidiMode));
            hintText = isMidiMode
                ? "MIDI | Play Chords | Drag X: Pan | Wheel: Level"
                : "Drag: Pan / Pitch | Wheel: Level | Dbl-Click: On/Off";
        }

        g.drawFittedText(hintText,
                         hintRect.toNearestInt(),
                         juce::Justification::centred, 1);
    }

    // Horizontal pitch guide lines
    static constexpr std::array<int, 9> kGridSteps = { -14, -7, -4, -2, 0, 2, 4, 7, 14 };
    g.setFont(Theme::makeFont(10.0f));

    for (int step : kGridSteps)
    {
        const float y = intervalToY(step);
        const bool isLead = (step == 0);

        g.setColour(isLead ? Theme::borderFocus : Theme::borderSubtle.withAlpha(0.65f));
        g.drawHorizontalLine(static_cast<int>(std::lround(y)), area.getX(), area.getRight());

        const juce::String lbl = isLead
            ? juce::String("Lead")
            : juce::String(dsp::ScaleHarmonizer::formatIntervalLabel(step, scaleType));

        g.setColour(isLead ? Theme::textSecondary : Theme::textMuted);
        g.drawFittedText(lbl,
                         juce::Rectangle<int>(static_cast<int>(area.getX()) - 48, static_cast<int>(y) - 8, 42, 16),
                         juce::Justification::centredRight, 1);
    }

    // Vertical pan guide lines (L / Center / R)
    for (float pan : { -100.0f, -50.0f, 0.0f, 50.0f, 100.0f })
    {
        const float x = panToX(pan);
        g.setColour(pan == 0.0f ? Theme::borderFocus : Theme::borderSubtle.withAlpha(0.45f));
        g.drawVerticalLine(static_cast<int>(std::lround(x)), area.getY(), area.getBottom());
    }

    g.setColour(Theme::textMuted);
    g.setFont(Theme::makeFont(10.5f));
    g.drawFittedText("L", juce::Rectangle<int>(static_cast<int>(area.getX()) - 10, static_cast<int>(area.getBottom()) + 3, 20, 14), juce::Justification::centred, 1);
    g.drawFittedText("Center", juce::Rectangle<int>(static_cast<int>(area.getCentreX()) - 28, static_cast<int>(area.getBottom()) + 3, 56, 14), juce::Justification::centred, 1);
    g.drawFittedText("R", juce::Rectangle<int>(static_cast<int>(area.getRight()) - 10, static_cast<int>(area.getBottom()) + 3, 20, 14), juce::Justification::centred, 1);

    // Lead vocal live pulse at (Center, Unison)
    const juce::Point<float> leadPt{ area.getCentreX(), intervalToY(0) };
    const float leadPulse = std::clamp(latestSnap.inputRms * 5.0f, 0.0f, 1.0f);
    if (leadPulse > 0.02f)
    {
        const float haloR = 11.0f + leadPulse * 14.0f;
        g.setColour(Theme::textPrimary.withAlpha(0.14f * leadPulse));
        g.fillEllipse(leadPt.x - haloR, leadPt.y - haloR, haloR * 2.0f, haloR * 2.0f);
    }
    g.setColour(Theme::bgElevated);
    g.fillEllipse(leadPt.x - 8.0f, leadPt.y - 8.0f, 16.0f, 16.0f);
    g.setColour(latestSnap.isVoiced ? Theme::liveAccent : Theme::textSecondary);
    g.drawEllipse(leadPt.x - 8.0f, leadPt.y - 8.0f, 16.0f, 16.0f, 1.5f);

    // Connecting lines & Voice Orbs
    for (int pass = 0; pass < 2; ++pass)
    {
        for (int v = 0; v < visibleVoices; ++v)
        {
            const bool isSelected = (v == selectedVoice);
            if ((pass == 0 && isSelected) || (pass == 1 && !isSelected))
                continue;

            const bool midiActive = latestSnap.voiceMidiActive[static_cast<size_t>(v)];
            const bool isEnabled = isMidiMode ? midiActive : (getParamPlain(params::voiceParamId(v, params::vEnabled), 0.0f) >= 0.5f);
            const bool isMuted = getParamPlain(params::voiceParamId(v, params::vMute), 0.0f) >= 0.5f;
            const bool isFixed = getParamPlain(params::voiceParamId(v, params::vFixed), 0.0f) >= 0.5f;
            const int fixedNote = static_cast<int>(std::lround(getParamPlain(params::voiceParamId(v, params::vFixedNote), 60.0f)));
            const int interval = static_cast<int>(std::lround(getParamPlain(params::voiceParamId(v, params::vInterval), 0.0f)));
            const auto pt = voiceToPoint(v);
            const float r = voiceRadius(v);
            const auto col = Theme::getVoiceColour(v);
            const float bypassScale = latestSnap.bypassed ? 0.42f : 1.0f;
            const float alpha = ((isEnabled && !isMuted) ? 1.0f : 0.32f) * bypassScale;

            // Subtle tether to Lead center
            g.setColour(col.withAlpha(isSelected ? 0.30f * alpha : 0.14f * alpha));
            g.drawLine(leadPt.x, leadPt.y, pt.x, pt.y, isSelected ? 1.5f : 1.0f);

            // Live RMS outer halo
            const float vRms = std::clamp(latestSnap.voiceRms[static_cast<size_t>(v)] * 4.5f, 0.0f, 1.0f);
            if (!latestSnap.bypassed && isEnabled && !isMuted && vRms > 0.02f)
            {
                const float halo = r + 3.0f + vRms * 12.0f;
                g.setColour(col.withAlpha(0.22f * vRms));
                g.fillEllipse(pt.x - halo, pt.y - halo, halo * 2.0f, halo * 2.0f);
            }

            // Orb body
            if (isEnabled && !isMuted)
            {
                g.setColour(col.withAlpha(0.86f * bypassScale));
                g.fillEllipse(pt.x - r, pt.y - r, r * 2.0f, r * 2.0f);
            }
            else
            {
                g.setColour(Theme::bgElevated.withAlpha(0.85f));
                g.fillEllipse(pt.x - r, pt.y - r, r * 2.0f, r * 2.0f);
                g.setColour(col.withAlpha(0.45f * bypassScale));
                g.drawEllipse(pt.x - r, pt.y - r, r * 2.0f, r * 2.0f, 1.5f);
            }

            if (isSelected)
            {
                g.setColour(Theme::textPrimary.withAlpha(bypassScale));
                g.drawEllipse(pt.x - r - 2.0f, pt.y - r - 2.0f, (r + 2.0f) * 2.0f, (r + 2.0f) * 2.0f, 2.0f);
            }
            else if (hoverZone == HoverZone::Orb && hoverIndex == v)
            {
                g.setColour(col.withAlpha(0.65f * bypassScale));
                g.drawEllipse(pt.x - r - 2.0f, pt.y - r - 2.0f, (r + 2.0f) * 2.0f, (r + 2.0f) * 2.0f, 1.5f);
            }

            // Orb label
            g.setColour(((isEnabled && !isMuted) ? Theme::bgCanvas : Theme::textSecondary).withAlpha(bypassScale));
            g.setFont(Theme::makeFont(10.5f, true));
            g.drawFittedText("V" + juce::String(v + 1),
                             juce::Rectangle<int>(static_cast<int>(pt.x - r), static_cast<int>(pt.y - 8), static_cast<int>(r * 2.0f), 16),
                             juce::Justification::centred, 1);

            const juce::String ivLabel = isMidiMode
                ? (midiActive ? formatMidiNoteName(latestSnap.voiceMidiNote[static_cast<size_t>(v)]) : juce::String("--"))
                : (isFixed ? ("FIX " + formatMidiNoteName(fixedNote))
                           : juce::String(dsp::ScaleHarmonizer::formatIntervalLabel(interval, scaleType)));
            g.setColour((isEnabled ? Theme::textPrimary : Theme::textMuted).withAlpha(bypassScale));
            g.setFont(Theme::makeFont(10.0f, isSelected));
            g.drawFittedText(ivLabel,
                             juce::Rectangle<int>(static_cast<int>(pt.x - 28), static_cast<int>(pt.y + r + 2.0f), 56, 13),
                             juce::Justification::centred, 1);
        }
    }

    if (latestSnap.bypassed)
    {
        const auto badge = juce::Rectangle<float>(area.getRight() - 78.0f, area.getBottom() - 24.0f, 70.0f, 18.0f);
        g.setColour(Theme::bgCanvas.withAlpha(0.90f));
        g.fillRoundedRectangle(badge, 4.5f);
        g.setColour(Theme::muteAccent.withAlpha(0.85f));
        g.drawRoundedRectangle(badge.reduced(0.5f), 4.5f, 1.0f);
        g.setColour(Theme::muteAccent);
        g.setFont(Theme::makeFont(9.5f, true));
        g.drawFittedText("BYPASSED", badge.toNearestInt(), juce::Justification::centred, 1);
    }
}

void HarmonyStage::beginDragVoice(int voiceIndex)
{
    if (gestureActive)
        return;
    processor.getUndoManager().beginNewTransaction("Edit Voice");
    draggingVoice = voiceIndex;
    draggingFixed = getParamPlain(params::voiceParamId(voiceIndex, params::vFixed), 0.0f) >= 0.5f;
    if (auto* pPan = processor.getApvts().getParameter(params::voiceParamId(voiceIndex, params::vPan)))
        pPan->beginChangeGesture();
    if (auto* pPitch = processor.getApvts().getParameter(params::voiceParamId(voiceIndex, draggingFixed ? params::vFixedNote : params::vInterval)))
        pPitch->beginChangeGesture();
    gestureActive = true;
}

void HarmonyStage::endDragVoice()
{
    if (!gestureActive || draggingVoice < 0)
        return;
    if (auto* pPan = processor.getApvts().getParameter(params::voiceParamId(draggingVoice, params::vPan)))
        pPan->endChangeGesture();
    if (auto* pPitch = processor.getApvts().getParameter(params::voiceParamId(draggingVoice, draggingFixed ? params::vFixedNote : params::vInterval)))
        pPitch->endChangeGesture();
    gestureActive = false;
    draggingVoice = -1;
}

void HarmonyStage::mouseDown(const juce::MouseEvent& e)
{
    const int visibleVoices = getVisibleVoiceCount();

    // Check top quick-select pills first
    for (int v = 0; v < visibleVoices; ++v)
    {
        if (pillRects[static_cast<size_t>(v)].contains(e.position))
        {
            selectedVoice = v;
            if (onVoiceSelected)
                onVoiceSelected(v);
            if (e.mods.isRightButtonDown())
            {
                processor.getUndoManager().beginNewTransaction("Edit Voice");
                const bool cur = getParamPlain(params::voiceParamId(v, params::vEnabled), 0.0f) >= 0.5f;
                setParamPlain(params::voiceParamId(v, params::vEnabled), cur ? 0.0f : 1.0f, true);
            }
            repaint();
            return;
        }
    }

    if (bankToggleRect.contains(e.position))
    {
        processor.getUndoManager().beginNewTransaction("Switch Voice Bank");
        const float target = (e.position.x >= bankToggleRect.getCentreX()) ? 1.0f : 0.0f;
        setParamPlain(params::voiceBank, target, true);
        updateHeaderLayout();
        if (target < 0.5f && selectedVoice >= dsp::kExposedVoicesV1)
        {
            selectedVoice = 0;
            if (onVoiceSelected)
                onVoiceSelected(selectedVoice);
        }
        repaint();
        return;
    }

    if (voicingBtnRect.contains(e.position))
    {
        showVoicingMenu();
        return;
    }

    if (undoBtnRect.contains(e.position))
    {
        processor.getUndoManager().undo();
        repaint();
        return;
    }

    if (redoBtnRect.contains(e.position))
    {
        processor.getUndoManager().redo();
        repaint();
        return;
    }

    // Check top snapshot pills (A / B / C / D)
    for (int s = 0; s < dsp::kNumSnapshots; ++s)
    {
        if (snapPillRects[static_cast<size_t>(s)].contains(e.position))
        {
            if (e.mods.isRightButtonDown())
                showSnapshotContextMenu(s);
            else if (processor.getLiveMidiController().getLearningTarget() == s)
                processor.getLiveMidiController().cancelLearn();
            else if (e.mods.isShiftDown())
                processor.getSnapshotManager().storeSnapshot(s);
            else
                processor.getSnapshotManager().selectSnapshot(s);

            repaint();
            return;
        }
    }

    const int hit = hitTestOrb(e.position);
    if (hit >= 0)
    {
        selectedVoice = hit;
        if (onVoiceSelected)
            onVoiceSelected(hit);

        if (e.mods.isRightButtonDown())
        {
            processor.getUndoManager().beginNewTransaction("Edit Voice");
            const bool cur = getParamPlain(params::voiceParamId(hit, params::vEnabled), 0.0f) >= 0.5f;
            setParamPlain(params::voiceParamId(hit, params::vEnabled), cur ? 0.0f : 1.0f, true);
            repaint();
            return;
        }

        beginDragVoice(hit);
        repaint();
    }
}

void HarmonyStage::mouseDrag(const juce::MouseEvent& e)
{
    if (!gestureActive || draggingVoice < 0)
        return;

    const float newPan = xToPan(e.position.x);
    setParamPlain(params::voiceParamId(draggingVoice, params::vPan), newPan, false);

    if (latestSnap.activeHarmonyMode != dsp::HarmonyMode::Midi)
    {
        const int newStep = yToInterval(e.position.y);
        if (draggingFixed)
            setParamPlain(params::voiceParamId(draggingVoice, params::vFixedNote), static_cast<float>(std::clamp(60 + newStep, 36, 96)), false);
        else
            setParamPlain(params::voiceParamId(draggingVoice, params::vInterval), static_cast<float>(newStep), false);
    }
    repaint();
}

void HarmonyStage::mouseUp(const juce::MouseEvent&)
{
    endDragVoice();
}

void HarmonyStage::mouseMove(const juce::MouseEvent& e)
{
    HoverZone nextZone = HoverZone::None;
    int nextIndex = -1;
    const int visibleVoices = getVisibleVoiceCount();

    for (int v = 0; v < visibleVoices; ++v)
    {
        if (pillRects[static_cast<size_t>(v)].contains(e.position))
        {
            nextZone = HoverZone::VoicePill;
            nextIndex = v;
            break;
        }
    }

    if (nextZone == HoverZone::None)
    {
        if (bankToggleRect.contains(e.position))
            nextZone = HoverZone::BankToggle;
        else if (voicingBtnRect.contains(e.position))
            nextZone = HoverZone::VoicingBtn;
        else if (undoBtnRect.contains(e.position))
            nextZone = HoverZone::UndoBtn;
        else if (redoBtnRect.contains(e.position))
            nextZone = HoverZone::RedoBtn;
        else
        {
            for (int s = 0; s < dsp::kNumSnapshots; ++s)
            {
                if (snapPillRects[static_cast<size_t>(s)].contains(e.position))
                {
                    nextZone = HoverZone::SnapPill;
                    nextIndex = s;
                    break;
                }
            }
        }
    }

    if (nextZone == HoverZone::None)
    {
        const int hit = hitTestOrb(e.position);
        if (hit >= 0)
        {
            nextZone = HoverZone::Orb;
            nextIndex = hit;
        }
    }

    setMouseCursor(nextZone != HoverZone::None ? juce::MouseCursor::PointingHandCursor
                                               : juce::MouseCursor::NormalCursor);

    if (nextZone != hoverZone || nextIndex != hoverIndex)
    {
        hoverZone = nextZone;
        hoverIndex = nextIndex;
        repaint();
    }
}

void HarmonyStage::mouseExit(const juce::MouseEvent&)
{
    setMouseCursor(juce::MouseCursor::NormalCursor);
    if (hoverZone != HoverZone::None || hoverIndex != -1)
    {
        hoverZone = HoverZone::None;
        hoverIndex = -1;
        repaint();
    }
}

void HarmonyStage::mouseDoubleClick(const juce::MouseEvent& e)
{
    const int visibleVoices = getVisibleVoiceCount();
    for (int v = 0; v < visibleVoices; ++v)
    {
        if (pillRects[static_cast<size_t>(v)].contains(e.position))
        {
            processor.getUndoManager().beginNewTransaction("Edit Voice");
            const bool cur = getParamPlain(params::voiceParamId(v, params::vEnabled), 0.0f) >= 0.5f;
            setParamPlain(params::voiceParamId(v, params::vEnabled), cur ? 0.0f : 1.0f, true);
            repaint();
            return;
        }
    }

    if (bankToggleRect.contains(e.position) || voicingBtnRect.contains(e.position)
        || undoBtnRect.contains(e.position) || redoBtnRect.contains(e.position))
        return;

    for (int s = 0; s < dsp::kNumSnapshots; ++s)
    {
        if (snapPillRects[static_cast<size_t>(s)].contains(e.position))
            return;
    }

    const int hit = hitTestOrb(e.position);
    if (hit >= 0)
    {
        processor.getUndoManager().beginNewTransaction("Edit Voice");
        const bool cur = getParamPlain(params::voiceParamId(hit, params::vEnabled), 0.0f) >= 0.5f;
        setParamPlain(params::voiceParamId(hit, params::vEnabled), cur ? 0.0f : 1.0f, true);
        repaint();
    }
}

void HarmonyStage::mouseWheelMove(const juce::MouseEvent& e, const juce::MouseWheelDetails& wheel)
{
    int target = hitTestOrb(e.position);
    if (target < 0)
        target = selectedVoice;

    processor.getUndoManager().beginNewTransaction("Edit Voice Level");
    const auto id = params::voiceParamId(target, params::vGainDb);
    const float curDb = getParamPlain(id, -6.0f);
    const float delta = (wheel.deltaY >= 0.0f ? 1.5f : -1.5f) * (wheel.isReversed ? -1.0f : 1.0f);
    const float nextDb = std::clamp(curDb + delta, -60.0f, 6.0f);
    setParamPlain(id, nextDb, true);
    repaint();
}

} // namespace harmonizer::ui
