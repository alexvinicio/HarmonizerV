#pragma once

#include "Ui/Theme.h"

#include <juce_gui_basics/juce_gui_basics.h>
#include <juce_audio_processors/juce_audio_processors.h>
#include <functional>
#include <memory>

namespace harmonizer::ui
{

class ModernLookAndFeel final : public juce::LookAndFeel_V4
{
public:
    ModernLookAndFeel();

    void drawRotarySlider(juce::Graphics& g,
                          int x, int y, int width, int height,
                          float sliderPosProportional,
                          float rotaryStartAngle,
                          float rotaryEndAngle,
                          juce::Slider& slider) override;

    void drawComboBox(juce::Graphics& g,
                      int width, int height,
                      bool isButtonDown,
                      int buttonX, int buttonY, int buttonW, int buttonH,
                      juce::ComboBox& box) override;

    juce::Font getComboBoxFont(juce::ComboBox& box) override;

    void drawPopupMenuBackground(juce::Graphics& g, int width, int height) override;
    void drawPopupMenuItem(juce::Graphics& g, const juce::Rectangle<int>& area,
                           bool isSeparator, bool isActive, bool isHighlighted,
                           bool isTicked, bool hasSubMenu,
                           const juce::String& text, const juce::String& shortcutKeyText,
                           const juce::Drawable* icon, const juce::Colour* textColour) override;

    void drawButtonBackground(juce::Graphics& g,
                              juce::Button& button,
                              const juce::Colour& backgroundColour,
                              bool shouldDrawButtonAsHighlighted,
                              bool shouldDrawButtonAsDown) override;

    juce::Font getTextButtonFont(juce::TextButton&, int buttonHeight) override;
};

class LabeledKnob final : public juce::Component
{
public:
    explicit LabeledKnob(const juce::String& labelText,
                         juce::Colour accentColour = Theme::liveAccent);

    void setAccentColour(juce::Colour c);
    void setValueFormatter(std::function<juce::String(double)> formatter);

    void attach(juce::AudioProcessorValueTreeState& apvts, const juce::String& paramId);
    void detach();
    void refreshDisplay();

    void resized() override;
    void paint(juce::Graphics& g) override;

private:
    class FormatSlider final : public juce::Slider
    {
    public:
        std::function<juce::String(double)> customFormatter;

        juce::String getTextFromValue(double value) override
        {
            if (customFormatter)
                return customFormatter(value);
            return juce::Slider::getTextFromValue(value);
        }
    };

    juce::String title;
    juce::Colour accent;
    FormatSlider slider;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> attachment;
};

} // namespace harmonizer::ui
