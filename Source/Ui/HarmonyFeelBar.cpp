#include "Ui/HarmonyFeelBar.h"
#include "Plugin/ParameterIds.h"
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
                                               std::function<juce::String(float)> formatter)
    : label(labelText),
      formatValue(std::move(formatter)),
      accent(Theme::liveAccent)
{
    param = apvts.getParameter(paramId);
    if (param != nullptr)
    {
        attachment = std::make_unique<juce::ParameterAttachment>(
            *param,
            [this](float newValue) { handleValueChanged(newValue); },
            nullptr);
        attachment->sendInitialUpdate();
    }
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
        g.setColour(accent.withAlpha(0.24f));
        g.fillRoundedRectangle(fillArea.withWidth(fillW), 4.5f);

        g.setColour(accent.withAlpha(0.85f));
        g.fillRoundedRectangle(juce::Rectangle<float>(fillArea.getX(), fillArea.getBottom() - 2.2f, fillW, 2.2f), 1.1f);
    }

    g.setColour(Theme::borderSubtle);
    g.drawRoundedRectangle(bounds, 6.0f, 1.0f);

    const auto textRect = getLocalBounds().reduced(10, 0);
    g.setColour(Theme::textSecondary);
    g.setFont(Theme::makeFont(10.5f, true));
    g.drawFittedText(label, textRect, juce::Justification::centredLeft, 1);

    const juce::String valStr = formatValue ? formatValue(currentPlain) : juce::String(currentPlain, 1);
    g.setColour(Theme::textPrimary);
    g.setFont(Theme::makeFont(11.0f, true));
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
        [](float v) { return (v < 0.5f) ? juce::String("OFF") : (juce::String(std::lround(v)) + "%"); });

    pills[1] = std::make_unique<FeelPillSlider>(
        apvts, params::glideMs, "GLIDE",
        [](float v) { return juce::String(std::lround(v)) + " ms"; });

    pills[2] = std::make_unique<FeelPillSlider>(
        apvts, params::naturalness, "NATURAL",
        [](float v) { return juce::String(std::lround(v)) + "%"; });

    pills[3] = std::make_unique<FeelPillSlider>(
        apvts, params::deEssAmount, "DE-ESS",
        [](float v) { return juce::String(std::lround(v)) + "%"; });

    pills[4] = std::make_unique<FeelPillSlider>(
        apvts, params::spaceAmount, "SPACE",
        [](float v) { return juce::String(std::lround(v)) + "%"; });

    for (auto& p : pills)
        addAndMakeVisible(*p);
}

void HarmonyFeelBar::updateModeAccent(dsp::EngineMode mode)
{
    const auto col = (mode == dsp::EngineMode::Live) ? Theme::liveAccent : Theme::studioAccent;
    for (auto& p : pills)
        p->setAccentColour(col);
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
