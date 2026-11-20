#include "Ui/HarmonyFeelBar.h"
#include "Plugin/ParameterIds.h"
#include "Ui/Localization.h"
#include "Ui/Theme.h"

#include <algorithm>
#include <cmath>

namespace harmonizer::ui
{

// ============================================================================
// FeelPillSlider
// ============================================================================

HarmonyFeelBar::FeelPillSlider::FeelPillSlider(juce::AudioProcessorValueTreeState& apvts,
                                               const juce::String& paramId,
                                               const juce::String& labelText,
                                               std::function<juce::String(float)> formatter,
                                               MeterKind kind,
                                               const juce::String& tooltipText)
    : label(labelText),
      formatValue(std::move(formatter)),
      accent(Theme::liveAccent),
      meterKind(kind)
{
    param = apvts.getParameter(paramId);
    if (param != nullptr)
    {
        attachment = std::make_unique<juce::ParameterAttachment>(
            *param,
            [this](float newValue) { handleValueChanged(newValue); },
            apvts.undoManager);
        attachment->sendInitialUpdate();
    }
    if (tooltipText.isNotEmpty())
        setTooltip(tooltipText);
    setMouseCursor(juce::MouseCursor::LeftRightResizeCursor);
}

void HarmonyFeelBar::FeelPillSlider::setAccentColour(juce::Colour c)
{
    if (accent != c)
    {
        accent = c;
        repaint();
    }
}

void HarmonyFeelBar::FeelPillSlider::setMeterValue(float value, bool active)
{
    if (std::abs(value - meterValue) >= 0.05f || active != meterActive)
    {
        meterValue = value;
        meterActive = active;
        repaint();
    }
}

void HarmonyFeelBar::FeelPillSlider::mouseEnter(const juce::MouseEvent&)
{
    if (!hovered)
    {
        hovered = true;
        repaint();
    }
}

void HarmonyFeelBar::FeelPillSlider::mouseExit(const juce::MouseEvent&)
{
    if (hovered)
    {
        hovered = false;
        repaint();
    }
}

void HarmonyFeelBar::FeelPillSlider::handleValueChanged(float newPlainValue)
{
    currentPlain = newPlainValue;
    currentNorm = (param != nullptr) ? param->convertTo0to1(newPlainValue) : 0.0f;
    repaint();
}

float HarmonyFeelBar::FeelPillSlider::xToPlain(float x) const noexcept
{
    if (param == nullptr)
        return 0.0f;
    const float norm = std::clamp((x - 4.0f) / std::max(1.0f, static_cast<float>(getWidth()) - 8.0f), 0.0f, 1.0f);
    return param->convertFrom0to1(norm);
}

void HarmonyFeelBar::FeelPillSlider::paint(juce::Graphics& g)
{
    const auto bounds = getLocalBounds().toFloat().reduced(0.5f);

    g.setColour(Theme::bgCanvas);
    g.fillRoundedRectangle(bounds, 6.0f);

    const auto fillArea = bounds.reduced(2.0f);
    const float fillW = std::max(0.0f, fillArea.getWidth() * std::clamp(currentNorm, 0.0f, 1.0f));
    if (fillW > 1.0f)
    {
        g.setColour(accent.withAlpha(0.20f));
        g.fillRoundedRectangle(fillArea.withWidth(fillW), 4.5f);
    }

    if (meterKind == MeterKind::None)
    {
        if (fillW > 1.0f)
        {
            g.setColour(accent.withAlpha(0.85f));
            g.fillRoundedRectangle(juce::Rectangle<float>(fillArea.getX(), fillArea.getBottom() - 2.2f, fillW, 2.2f), 1.1f);
        }
    }
    else
    {
        const auto track = juce::Rectangle<float>(bounds.getX() + 6.0f,
                                                  bounds.getBottom() - 7.0f,
                                                  std::max(10.0f, bounds.getWidth() - 12.0f),
                                                  4.0f);
        g.setColour(Theme::bgElevated);
        g.fillRoundedRectangle(track, 2.0f);

        if (meterKind == MeterKind::TuneCents)
        {
            // Center 0 ct reference tick
            g.setColour(Theme::borderFocus);
            g.fillRect(track.getCentreX() - 0.5f, track.getY(), 1.0f, track.getHeight());

            if (currentPlain >= 0.5f && meterActive)
            {
                const float norm = std::clamp(meterValue / 60.0f, -1.0f, 1.0f);
                const float halfW = track.getWidth() * 0.5f;
                const float barW = std::abs(norm) * halfW;
                if (barW >= 1.0f)
                {
                    const float barX = (norm >= 0.0f) ? track.getCentreX() : (track.getCentreX() - barW);
                    g.setColour(std::abs(meterValue) >= 15.0f ? Theme::warnAccent : Theme::liveAccent);
                    g.fillRoundedRectangle(juce::Rectangle<float>(barX, track.getY(), barW, track.getHeight()), 1.5f);
                }
            }
        }
        else if (meterKind == MeterKind::DeEssGrDb)
        {
            if (currentPlain >= 0.5f && meterValue >= 0.2f)
            {
                const float norm = std::clamp(meterValue / 12.0f, 0.0f, 1.0f);
                const float barW = track.getWidth() * norm;
                if (barW >= 1.0f)
                {
                    g.setColour(meterValue >= 6.0f ? Theme::muteAccent : Theme::warnAccent);
                    g.fillRoundedRectangle(juce::Rectangle<float>(track.getRight() - barW, track.getY(), barW, track.getHeight()), 1.5f);
                }
            }
        }
    }

    g.setColour(hovered ? accent.withAlpha(0.65f) : Theme::borderSubtle);
    g.drawRoundedRectangle(bounds, 6.0f, 1.0f);

    const auto textRect = getLocalBounds().reduced(10, 0).withTrimmedBottom(meterKind == MeterKind::None ? 2 : 7);
    g.setColour(Theme::textSecondary);
    g.setFont(Theme::makeFont(10.0f, true));
    g.drawFittedText(label, textRect, juce::Justification::centredLeft, 1);

    if (meterKind == MeterKind::TuneCents && currentPlain >= 0.5f && meterActive && std::abs(meterValue) >= 1.5f)
    {
        const int roundedCents = static_cast<int>(std::lround(meterValue));
        const juce::String centStr = (roundedCents > 0 ? "+" : "") + juce::String(roundedCents) + "c";
        g.setColour(std::abs(meterValue) >= 15.0f ? Theme::warnAccent : Theme::liveAccent);
        g.setFont(Theme::makeFont(9.5f, true));
        g.drawFittedText(centStr, textRect, juce::Justification::centred, 1);
    }
    else if (meterKind == MeterKind::DeEssGrDb && currentPlain >= 0.5f && meterValue >= 0.4f)
    {
        const juce::String grStr = "-" + juce::String(meterValue, 1) + "dB";
        g.setColour(meterValue >= 6.0f ? Theme::muteAccent : Theme::warnAccent);
        g.setFont(Theme::makeFont(9.5f, true));
        g.drawFittedText(grStr, textRect, juce::Justification::centred, 1);
    }

    const juce::String valStr = formatValue ? formatValue(currentPlain) : juce::String(currentPlain, 1);
    g.setColour(Theme::textPrimary);
    g.setFont(Theme::makeFont(10.5f, true));
    g.drawFittedText(valStr, textRect, juce::Justification::centredRight, 1);
}

void HarmonyFeelBar::FeelPillSlider::mouseDown(const juce::MouseEvent& e)
{
    if (attachment == nullptr || param == nullptr)
        return;

    dragging = true;
    attachment->beginGesture();
    attachment->setValueAsPartOfGesture(xToPlain(e.position.x));
}

void HarmonyFeelBar::FeelPillSlider::mouseDrag(const juce::MouseEvent& e)
{
    if (!dragging || attachment == nullptr)
        return;
    attachment->setValueAsPartOfGesture(xToPlain(e.position.x));
}

void HarmonyFeelBar::FeelPillSlider::mouseUp(const juce::MouseEvent&)
{
    if (dragging && attachment != nullptr)
    {
        attachment->endGesture();
        dragging = false;
    }
}

void HarmonyFeelBar::FeelPillSlider::mouseDoubleClick(const juce::MouseEvent&)
{
    if (attachment == nullptr || param == nullptr)
        return;
    const float defPlain = param->convertFrom0to1(param->getDefaultValue());
    attachment->setValueAsCompleteGesture(defPlain);
}

// ============================================================================
// HarmonyFeelBar
// ============================================================================

HarmonyFeelBar::HarmonyFeelBar(HarmonizerVProcessor& proc)
{
    auto& apvts = proc.getApvts();

    pills[0] = std::make_unique<FeelPillSlider>(
        apvts, params::inputTune, "TUNE",
        [](float v) { return (v < 0.5f) ? juce::String("OFF") : (juce::String(std::lround(v)) + "%"); },
        FeelPillSlider::MeterKind::TuneCents,
        tr(dsp::TextKey::TuneTooltip));

    pills[1] = std::make_unique<FeelPillSlider>(
        apvts, params::glideMs, "GLIDE",
        [](float v) { return juce::String(std::lround(v)) + " ms"; },
        FeelPillSlider::MeterKind::None,
        tr(dsp::TextKey::GlideTooltip));

    pills[2] = std::make_unique<FeelPillSlider>(
        apvts, params::naturalness, "NATURAL",
        [](float v) { return juce::String(std::lround(v)) + "%"; },
        FeelPillSlider::MeterKind::None,
        tr(dsp::TextKey::NaturalTooltip));

    pills[3] = std::make_unique<FeelPillSlider>(
        apvts, params::deEssAmount, "DE-ESS",
        [](float v) { return juce::String(std::lround(v)) + "%"; },
        FeelPillSlider::MeterKind::DeEssGrDb,
        tr(dsp::TextKey::DeEssTooltip));

    pills[4] = std::make_unique<FeelPillSlider>(
        apvts, params::spaceAmount, "SPACE",
        [](float v) { return juce::String(std::lround(v)) + "%"; },
        FeelPillSlider::MeterKind::None,
        tr(dsp::TextKey::SpaceTooltip));

    for (auto& p : pills)
        addAndMakeVisible(*p);
}

void HarmonyFeelBar::refreshLocalization()
{
    pills[0]->setTooltip(tr(dsp::TextKey::TuneTooltip));
    pills[1]->setTooltip(tr(dsp::TextKey::GlideTooltip));
    pills[2]->setTooltip(tr(dsp::TextKey::NaturalTooltip));
    pills[3]->setTooltip(tr(dsp::TextKey::DeEssTooltip));
    pills[4]->setTooltip(tr(dsp::TextKey::SpaceTooltip));
    repaint();
}

void HarmonyFeelBar::updateModeAccent(dsp::EngineMode mode)
{
    const auto col = (mode == dsp::EngineMode::Live) ? Theme::liveAccent : Theme::studioAccent;
    for (auto& p : pills)
        p->setAccentColour(col);
}

void HarmonyFeelBar::updateTelemetry(const dsp::TelemetrySnapshot& snap)
{
    updateModeAccent(snap.activeMode);

    const float targetTune = (!snap.bypassed && snap.isVoiced) ? snap.inputTuneCorrectionCents : 0.0f;
    displayTuneCents += 0.35f * (targetTune - displayTuneCents);
    if (std::abs(displayTuneCents) < 0.25f)
        displayTuneCents = 0.0f;
    pills[0]->setMeterValue(displayTuneCents, !snap.bypassed && snap.isVoiced);

    const float targetGr = snap.bypassed ? 0.0f : snap.deEssReductionDb;
    displayDeEssGrDb = std::max(targetGr, displayDeEssGrDb * 0.80f);
    if (displayDeEssGrDb < 0.1f)
        displayDeEssGrDb = 0.0f;
    pills[3]->setMeterValue(displayDeEssGrDb, !snap.bypassed && displayDeEssGrDb >= 0.25f);
}

void HarmonyFeelBar::resized()
{
    auto r = getLocalBounds().reduced(10, 6);
    constexpr int gap = 8;
    const int count = static_cast<int>(pills.size());
    const int pillW = std::max(55, (r.getWidth() - gap * (count - 1)) / count);
    for (int i = 0; i < count; ++i)
    {
        pills[static_cast<size_t>(i)]->setBounds(r.removeFromLeft(pillW));
        if (i < count - 1)
            r.removeFromLeft(gap);
    }
}

void HarmonyFeelBar::paint(juce::Graphics& g)
{
    const auto card = getLocalBounds().toFloat();
    g.setColour(Theme::bgSurface);
    g.fillRoundedRectangle(card, Theme::kCardRadius);
    g.setColour(Theme::borderSubtle);
    g.drawRoundedRectangle(card.reduced(0.5f), Theme::kCardRadius, 1.0f);
}

} // namespace harmonizer::ui
