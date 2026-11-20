#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

namespace harmonizer::ui
{

class AboutOverlay final : public juce::Component
{
public:
    AboutOverlay();

    void showOverlay();
    void hideOverlay();
    void refreshLocalization();

    void resized() override;
    void paint(juce::Graphics& g) override;
    void mouseDown(const juce::MouseEvent& e) override;

private:
    juce::Rectangle<int> cardBounds;
    juce::TextButton coffeeButton;
    juce::TextButton closeButton;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(AboutOverlay)
};

} // namespace harmonizer::ui
