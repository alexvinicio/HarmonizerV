#include "Ui/HarmonyStage.h"
#include "Dsp/ScaleHarmonizer.h"
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
}

void HarmonyStage::setSelectedVoice(int voiceIndex)
{
    const int clamped = std::clamp(voiceIndex, 0, dsp::kExposedVoicesV1 - 1);
    if (selectedVoice != clamped)
    {
        selectedVoice = clamped;
        repaint();
    }
}

void HarmonyStage::updateTelemetry(const dsp::TelemetrySnapshot& snap)
{
    latestSnap = snap;
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
    const int interval = static_cast<int>(std::lround(getParamPlain(params::voiceParamId(voiceIndex, params::vInterval), 0.0f)));
    return { panToX(pan), intervalToY(interval) };
}

int HarmonyStage::hitTestOrb(juce::Point<float> pos) const noexcept
{
    // Test selected voice last/priority, or closest orb within hit radius
    for (int pass = 0; pass < 2; ++pass)
    {
        for (int v = dsp::kExposedVoicesV1 - 1; v >= 0; --v)
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

void HarmonyStage::resized()
{
    const auto top = getLocalBounds().toFloat().reduced(14.0f, 10.0f).removeFromTop(26.0f);
    float x = top.getX();
    for (int v = 0; v < dsp::kExposedVoicesV1; ++v)
    {
        pillRects[static_cast<size_t>(v)] = juce::Rectangle<float>(x, top.getY(), 54.0f, 24.0f);
        x += 60.0f;
    }
}

void HarmonyStage::paint(juce::Graphics& g)
{
    const auto card = getLocalBounds().toFloat();
    g.setColour(Theme::bgSurface);
    g.fillRoundedRectangle(card, Theme::kCardRadius);
    g.setColour(Theme::borderSubtle);
    g.drawRoundedRectangle(card.reduced(0.5f), Theme::kCardRadius, 1.0f);

    const auto area = getStageArea();
    const int scaleIdx = static_cast<int>(std::lround(getParamPlain(params::scale, 0.0f)));
    const auto scaleType = static_cast<dsp::ScaleType>(scaleIdx);

    // Quick Voice Select Pills (V1..V4)
    for (int v = 0; v < dsp::kExposedVoicesV1; ++v)
    {
        const auto pr = pillRects[static_cast<size_t>(v)];
        const bool isSelected = (v == selectedVoice);
        const bool isEnabled = getParamPlain(params::voiceParamId(v, params::vEnabled), 0.0f) >= 0.5f;
        const bool isMuted = getParamPlain(params::voiceParamId(v, params::vMute), 0.0f) >= 0.5f;
        const auto col = Theme::getVoiceColour(v);

        g.setColour(isSelected ? Theme::bgElevated : Theme::bgCanvas.withAlpha(0.75f));
        g.fillRoundedRectangle(pr, 6.0f);
        g.setColour(isSelected ? col : Theme::borderSubtle);
        g.drawRoundedRectangle(pr.reduced(0.5f), 6.0f, isSelected ? 1.5f : 1.0f);

        const auto dotRect = juce::Rectangle<float>(pr.getX() + 8.0f, pr.getCentreY() - 4.0f, 8.0f, 8.0f);
        if (isEnabled && !isMuted)
        {
            g.setColour(col);
            g.fillEllipse(dotRect);
        }
        else
        {
            g.setColour(isMuted ? Theme::muteAccent : Theme::textMuted);
            g.drawEllipse(dotRect, 1.4f);
        }

        g.setColour(isEnabled ? Theme::textPrimary : Theme::textMuted);
        g.setFont(Theme::makeFont(11.5f, isSelected));
        g.drawFittedText("V" + juce::String(v + 1),
                         pr.withTrimmedLeft(18.0f).toNearestInt(),
                         juce::Justification::centredLeft, 1);
    }

    // Subtle hint top-right
    g.setColour(Theme::textMuted);
    g.setFont(Theme::makeFont(10.5f));
    g.drawFittedText("Drag: Pan / Pitch  ·  Wheel: Level  ·  Double-Click: On/Off",
                     getLocalBounds().reduced(16, 12).removeFromTop(22),
                     juce::Justification::centredRight, 1);

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
        for (int v = 0; v < dsp::kExposedVoicesV1; ++v)
        {
            const bool isSelected = (v == selectedVoice);
            if ((pass == 0 && isSelected) || (pass == 1 && !isSelected))
                continue;

            const bool isEnabled = getParamPlain(params::voiceParamId(v, params::vEnabled), 0.0f) >= 0.5f;
            const bool isMuted = getParamPlain(params::voiceParamId(v, params::vMute), 0.0f) >= 0.5f;
            const int interval = static_cast<int>(std::lround(getParamPlain(params::voiceParamId(v, params::vInterval), 0.0f)));
            const auto pt = voiceToPoint(v);
            const float r = voiceRadius(v);
            const auto col = Theme::getVoiceColour(v);
            const float alpha = (isEnabled && !isMuted) ? 1.0f : 0.32f;

            // Subtle tether to Lead center
            g.setColour(col.withAlpha(isSelected ? 0.30f * alpha : 0.14f * alpha));
            g.drawLine(leadPt.x, leadPt.y, pt.x, pt.y, isSelected ? 1.5f : 1.0f);

            // Live RMS outer halo
            const float vRms = std::clamp(latestSnap.voiceRms[static_cast<size_t>(v)] * 4.5f, 0.0f, 1.0f);
            if (isEnabled && !isMuted && vRms > 0.02f)
            {
                const float halo = r + 3.0f + vRms * 12.0f;
                g.setColour(col.withAlpha(0.22f * vRms));
                g.fillEllipse(pt.x - halo, pt.y - halo, halo * 2.0f, halo * 2.0f);
            }

            // Orb body
            if (isEnabled && !isMuted)
            {
                g.setColour(col.withAlpha(0.86f));
                g.fillEllipse(pt.x - r, pt.y - r, r * 2.0f, r * 2.0f);
            }
            else
            {
                g.setColour(Theme::bgElevated.withAlpha(0.85f));
                g.fillEllipse(pt.x - r, pt.y - r, r * 2.0f, r * 2.0f);
                g.setColour(col.withAlpha(0.45f));
                g.drawEllipse(pt.x - r, pt.y - r, r * 2.0f, r * 2.0f, 1.5f);
            }

            if (isSelected)
            {
                g.setColour(Theme::textPrimary);
                g.drawEllipse(pt.x - r - 2.0f, pt.y - r - 2.0f, (r + 2.0f) * 2.0f, (r + 2.0f) * 2.0f, 2.0f);
            }

            // Orb label
            g.setColour((isEnabled && !isMuted) ? Theme::bgCanvas : Theme::textSecondary);
            g.setFont(Theme::makeFont(10.5f, true));
            g.drawFittedText("V" + juce::String(v + 1),
                             juce::Rectangle<int>(static_cast<int>(pt.x - r), static_cast<int>(pt.y - 8), static_cast<int>(r * 2.0f), 16),
                             juce::Justification::centred, 1);

            const juce::String ivLabel = dsp::ScaleHarmonizer::formatIntervalLabel(interval, scaleType);
            g.setColour(isEnabled ? Theme::textPrimary : Theme::textMuted);
            g.setFont(Theme::makeFont(10.0f, isSelected));
            g.drawFittedText(ivLabel,
                             juce::Rectangle<int>(static_cast<int>(pt.x - 28), static_cast<int>(pt.y + r + 2.0f), 56, 13),
                             juce::Justification::centred, 1);
        }
    }
}

void HarmonyStage::beginDragVoice(int voiceIndex)
{
    if (gestureActive)
        return;
    draggingVoice = voiceIndex;
    if (auto* pPan = processor.getApvts().getParameter(params::voiceParamId(voiceIndex, params::vPan)))
        pPan->beginChangeGesture();
    if (auto* pIv = processor.getApvts().getParameter(params::voiceParamId(voiceIndex, params::vInterval)))
        pIv->beginChangeGesture();
    gestureActive = true;
}

void HarmonyStage::endDragVoice()
{
    if (!gestureActive || draggingVoice < 0)
        return;
    if (auto* pPan = processor.getApvts().getParameter(params::voiceParamId(draggingVoice, params::vPan)))
        pPan->endChangeGesture();
    if (auto* pIv = processor.getApvts().getParameter(params::voiceParamId(draggingVoice, params::vInterval)))
        pIv->endChangeGesture();
    gestureActive = false;
    draggingVoice = -1;
}

void HarmonyStage::mouseDown(const juce::MouseEvent& e)
{
    // Check top quick-select pills first
    for (int v = 0; v < dsp::kExposedVoicesV1; ++v)
    {
        if (pillRects[static_cast<size_t>(v)].contains(e.position))
        {
            selectedVoice = v;
            if (onVoiceSelected)
                onVoiceSelected(v);
            if (e.mods.isRightButtonDown())
            {
                const bool cur = getParamPlain(params::voiceParamId(v, params::vEnabled), 0.0f) >= 0.5f;
                setParamPlain(params::voiceParamId(v, params::vEnabled), cur ? 0.0f : 1.0f, true);
            }
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
    const int newInterval = yToInterval(e.position.y);

    setParamPlain(params::voiceParamId(draggingVoice, params::vPan), newPan, false);
    setParamPlain(params::voiceParamId(draggingVoice, params::vInterval), static_cast<float>(newInterval), false);
    repaint();
}

void HarmonyStage::mouseUp(const juce::MouseEvent&)
{
    endDragVoice();
}

void HarmonyStage::mouseDoubleClick(const juce::MouseEvent& e)
{
    for (int v = 0; v < dsp::kExposedVoicesV1; ++v)
    {
        if (pillRects[static_cast<size_t>(v)].contains(e.position))
        {
            const bool cur = getParamPlain(params::voiceParamId(v, params::vEnabled), 0.0f) >= 0.5f;
            setParamPlain(params::voiceParamId(v, params::vEnabled), cur ? 0.0f : 1.0f, true);
            repaint();
            return;
        }
    }

    const int hit = hitTestOrb(e.position);
    if (hit >= 0)
    {
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

    const auto id = params::voiceParamId(target, params::vGainDb);
    const float curDb = getParamPlain(id, -6.0f);
    const float delta = (wheel.deltaY >= 0.0f ? 1.5f : -1.5f) * (wheel.isReversed ? -1.0f : 1.0f);
    const float nextDb = std::clamp(curDb + delta, -60.0f, 6.0f);
    setParamPlain(id, nextDb, true);
    repaint();
}

} // namespace harmonizer::ui
