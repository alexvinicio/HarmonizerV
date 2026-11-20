#include "Ui/ModernLookAndFeel.h"

#include <algorithm>
#include <cmath>

namespace harmonizer::ui
{

ModernLookAndFeel::ModernLookAndFeel()
{
    setColour(juce::ComboBox::backgroundColourId, Theme::bgElevated);
    setColour(juce::ComboBox::textColourId, Theme::textPrimary);
    setColour(juce::ComboBox::outlineColourId, Theme::borderSubtle);
    setColour(juce::ComboBox::arrowColourId, Theme::textSecondary);
    setColour(juce::PopupMenu::backgroundColourId, Theme::bgSurface);
    setColour(juce::PopupMenu::textColourId, Theme::textPrimary);
    setColour(juce::PopupMenu::highlightedBackgroundColourId, Theme::bgElevated);
    setColour(juce::PopupMenu::highlightedTextColourId, Theme::textPrimary);
    setColour(juce::TextButton::buttonColourId, Theme::bgElevated);
    setColour(juce::TextButton::buttonOnColourId, Theme::liveAccent);
    setColour(juce::TextButton::textColourOffId, Theme::textSecondary);
    setColour(juce::TextButton::textColourOnId, Theme::bgCanvas);
    setColour(juce::Slider::rotarySliderFillColourId, Theme::liveAccent);
}

void ModernLookAndFeel::drawRotarySlider(juce::Graphics& g,
                                         int x, int y, int width, int height,
                                         float sliderPos,
                                         float rotaryStartAngle,
                                         float rotaryEndAngle,
                                         juce::Slider& slider)
{
    const auto bounds = juce::Rectangle<float>(static_cast<float>(x), static_cast<float>(y),
                                               static_cast<float>(width), static_cast<float>(height)).reduced(3.0f);
    const float radius = std::min(bounds.getWidth(), bounds.getHeight()) * 0.5f;
    if (radius <= 2.0f)
        return;

    const auto centre = bounds.getCentre();
    const float arcRadius = radius - 3.0f;
    const float strokeWidth = std::clamp(radius * 0.14f, 2.2f, 3.6f);
    const float angle = rotaryStartAngle + sliderPos * (rotaryEndAngle - rotaryStartAngle);

    const auto accent = slider.findColour(juce::Slider::rotarySliderFillColourId);
    const bool enabled = slider.isEnabled();

    // Background arc track
    juce::Path trackArc;
    trackArc.addCentredArc(centre.x, centre.y, arcRadius, arcRadius, 0.0f, rotaryStartAngle, rotaryEndAngle, true);
    g.setColour(Theme::borderSubtle);
    g.strokePath(trackArc, juce::PathStrokeType(strokeWidth, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

    // Active value arc (bipolar if centered property is set)
    const bool bipolar = slider.getProperties().getWithDefault("bipolar", false);
    const float startAngle = bipolar ? (rotaryStartAngle + 0.5f * (rotaryEndAngle - rotaryStartAngle)) : rotaryStartAngle;

    if (std::abs(angle - startAngle) > 0.01f)
    {
        juce::Path valueArc;
        valueArc.addCentredArc(centre.x, centre.y, arcRadius, arcRadius, 0.0f,
                               std::min(startAngle, angle), std::max(startAngle, angle), true);
        g.setColour(enabled ? accent : accent.withAlpha(0.35f));
        g.strokePath(valueArc, juce::PathStrokeType(strokeWidth, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    }

    // Inner knob cap
    const float capRadius = std::max(4.0f, arcRadius - strokeWidth * 1.9f);
    g.setColour(Theme::bgElevated);
    g.fillEllipse(centre.x - capRadius, centre.y - capRadius, capRadius * 2.0f, capRadius * 2.0f);
    g.setColour(Theme::borderFocus);
    g.drawEllipse(centre.x - capRadius, centre.y - capRadius, capRadius * 2.0f, capRadius * 2.0f, 1.0f);

    // Pointer indicator line / dot
    const float pointerLen = capRadius * 0.72f;
    const float px = centre.x + std::sin(angle) * pointerLen;
    const float py = centre.y - std::cos(angle) * pointerLen;
    g.setColour(enabled ? Theme::textPrimary : Theme::textMuted);
    g.drawLine(centre.x + std::sin(angle) * (capRadius * 0.25f),
               centre.y - std::cos(angle) * (capRadius * 0.25f),
               px, py, 2.0f);
}

void ModernLookAndFeel::drawComboBox(juce::Graphics& g,
                                     int width, int height,
                                     bool /*isButtonDown*/,
                                     int /*buttonX*/, int /*buttonY*/, int /*buttonW*/, int /*buttonH*/,
                                     juce::ComboBox& box)
{
    const auto bounds = juce::Rectangle<float>(0.0f, 0.0f, static_cast<float>(width), static_cast<float>(height)).reduced(0.5f);
    g.setColour(Theme::bgElevated);
    g.fillRoundedRectangle(bounds, Theme::kPillRadius);
    g.setColour(box.hasKeyboardFocus(true) ? Theme::borderFocus : Theme::borderSubtle);
    g.drawRoundedRectangle(bounds, Theme::kPillRadius, 1.0f);

    // Subtle chevron
    const float arrowX = static_cast<float>(width) - 13.0f;
    const float arrowY = static_cast<float>(height) * 0.5f;
    juce::Path chevron;
    chevron.startNewSubPath(arrowX - 3.5f, arrowY - 1.5f);
    chevron.lineTo(arrowX, arrowY + 2.0f);
    chevron.lineTo(arrowX + 3.5f, arrowY - 1.5f);
    g.setColour(Theme::textSecondary);
    g.strokePath(chevron, juce::PathStrokeType(1.5f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
}

juce::Font ModernLookAndFeel::getComboBoxFont(juce::ComboBox&)
{
    return Theme::makeFont(12.5f);
}

void ModernLookAndFeel::drawPopupMenuBackground(juce::Graphics& g, int width, int height)
{
    g.fillAll(Theme::bgSurface);
    g.setColour(Theme::borderSubtle);
    g.drawRect(0, 0, width, height, 1);
}

void ModernLookAndFeel::drawPopupMenuItem(juce::Graphics& g, const juce::Rectangle<int>& area,
                                          bool isSeparator, bool isActive, bool isHighlighted,
                                          bool isTicked, bool /*hasSubMenu*/,
                                          const juce::String& text, const juce::String& /*shortcutKeyText*/,
                                          const juce::Drawable* /*icon*/, const juce::Colour* /*textColour*/)
{
    if (isSeparator)
    {
        g.setColour(Theme::borderSubtle);
        g.fillRect(area.reduced(6, 0).withHeight(1).withCentre(area.getCentre()));
        return;
    }

    const auto r = area.reduced(3, 1);
    if (isHighlighted && isActive)
    {
        g.setColour(Theme::bgElevated);
        g.fillRoundedRectangle(r.toFloat(), 4.0f);
    }

    g.setColour(isActive ? (isTicked ? Theme::liveAccent : Theme::textPrimary) : Theme::textMuted);
    g.setFont(Theme::makeFont(12.5f, isTicked));
    g.drawFittedText(text, r.reduced(8, 0), juce::Justification::centredLeft, 1);
}

void ModernLookAndFeel::drawButtonBackground(juce::Graphics& g,
                                             juce::Button& button,
                                             const juce::Colour& backgroundColour,
                                             bool shouldDrawButtonAsHighlighted,
                                             bool shouldDrawButtonAsDown)
{
    const auto bounds = button.getLocalBounds().toFloat().reduced(0.5f);
    auto fill = button.getToggleState()
                    ? button.findColour(juce::TextButton::buttonOnColourId)
                    : backgroundColour;

    if (shouldDrawButtonAsDown)
        fill = fill.darker(0.15f);
    else if (shouldDrawButtonAsHighlighted)
        fill = fill.brighter(0.10f);

    g.setColour(fill);
    g.fillRoundedRectangle(bounds, Theme::kPillRadius);

    g.setColour(button.getToggleState() ? fill.brighter(0.2f) : Theme::borderSubtle);
    g.drawRoundedRectangle(bounds, Theme::kPillRadius, 1.0f);
}

juce::Font ModernLookAndFeel::getTextButtonFont(juce::TextButton&, int)
{
    return Theme::makeFont(11.5f, true);
}

// ============================================================================
// LabeledKnob
// ============================================================================

LabeledKnob::LabeledKnob(const juce::String& labelText, juce::Colour accentColour)
    : title(labelText), accent(accentColour)
{
    slider.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
    slider.setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
    slider.setColour(juce::Slider::rotarySliderFillColourId, accent);
    slider.setDoubleClickReturnValue(true, 0.0);
    slider.onValueChange = [this]() { repaint(); };
    addAndMakeVisible(slider);
}

void LabeledKnob::setAccentColour(juce::Colour c)
{
    accent = c;
    slider.setColour(juce::Slider::rotarySliderFillColourId, accent);
    repaint();
}

void LabeledKnob::setTitle(const juce::String& newTitle)
{
    if (title != newTitle)
    {
        title = newTitle;
        repaint();
    }
}

void LabeledKnob::setTooltip(const juce::String& text)
{
    slider.setTooltip(text);
}

void LabeledKnob::setValueFormatter(std::function<juce::String(double)> formatter)
{
    slider.customFormatter = std::move(formatter);
    repaint();
}

void LabeledKnob::attach(juce::AudioProcessorValueTreeState& apvts, const juce::String& paramId)
{
    attachment.reset();
    if (auto* param = apvts.getParameter(paramId))
    {
        const double defPlain = static_cast<double>(param->convertFrom0to1(param->getDefaultValue()));
        slider.setDoubleClickReturnValue(true, defPlain);

        const auto range = param->getNormalisableRange();
        const bool isBipolar = (range.start < 0.0f && range.end > 0.0f);
        slider.getProperties().set("bipolar", isBipolar);
    }
    attachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(apvts, paramId, slider);
    repaint();
}

void LabeledKnob::detach()
{
    attachment.reset();
}

void LabeledKnob::refreshDisplay()
{
    repaint();
}

void LabeledKnob::resized()
{
    auto r = getLocalBounds();
    r.removeFromTop(14);
    r.removeFromBottom(15);
    slider.setBounds(r);
}

void LabeledKnob::paint(juce::Graphics& g)
{
    auto r = getLocalBounds();
    const auto topArea = r.removeFromTop(14);
    const auto bottomArea = r.removeFromBottom(15);

    g.setColour(Theme::textSecondary);
    g.setFont(Theme::makeFont(10.5f, true));
    g.drawFittedText(title, topArea, juce::Justification::centred, 1);

    g.setColour(Theme::textPrimary);
    g.setFont(Theme::makeFont(10.5f));
    g.drawFittedText(slider.getTextFromValue(slider.getValue()), bottomArea, juce::Justification::centred, 1);
}

} // namespace harmonizer::ui
