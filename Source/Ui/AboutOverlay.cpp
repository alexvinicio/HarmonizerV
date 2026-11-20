#include "Ui/AboutOverlay.h"
#include "Plugin/AppInfo.h"
#include "Ui/Localization.h"
#include "Ui/Theme.h"

#include <algorithm>

namespace harmonizer::ui
{

AboutOverlay::AboutOverlay()
{
    setInterceptsMouseClicks(true, true);

    coffeeButton.setColour(juce::TextButton::buttonColourId, Theme::warnAccent);
    coffeeButton.setColour(juce::TextButton::textColourOffId, Theme::bgCanvas);
    coffeeButton.setMouseCursor(juce::MouseCursor::PointingHandCursor);
    coffeeButton.onClick = []()
    {
        juce::URL(appinfo::kBuyMeACoffeeUrl).launchInDefaultBrowser();
    };
    addAndMakeVisible(coffeeButton);

    closeButton.setColour(juce::TextButton::buttonColourId, Theme::bgElevated);
    closeButton.setColour(juce::TextButton::textColourOffId, Theme::textPrimary);
    closeButton.setMouseCursor(juce::MouseCursor::PointingHandCursor);
    closeButton.onClick = [this]() { hideOverlay(); };
    addAndMakeVisible(closeButton);

    refreshLocalization();
}

void AboutOverlay::showOverlay()
{
    refreshLocalization();
    setVisible(true);
    toFront(true);
    resized();
    repaint();
}

void AboutOverlay::hideOverlay()
{
    setVisible(false);
}

void AboutOverlay::refreshLocalization()
{
    coffeeButton.setButtonText(tr(dsp::TextKey::AboutCoffeeButton) + " - " + appinfo::kBuyMeACoffeeText);
    closeButton.setButtonText(tr(dsp::TextKey::CloseBtn));
    repaint();
}

void AboutOverlay::resized()
{
    const int cardW = std::min(440, std::max(320, getWidth() - 40));
    const int cardH = std::min(308, std::max(250, getHeight() - 40));
    cardBounds = getLocalBounds().withSizeKeepingCentre(cardW, cardH);

    auto inner = cardBounds.reduced(20, 16);
    inner.removeFromTop(104); // Header, subtitle, and feature summary

    // Support box area (bottom ~144 px above footer)
    auto footer = inner.removeFromBottom(30);
    closeButton.setBounds(footer.removeFromRight(92).reduced(0, 2));

    inner.removeFromBottom(10);
    auto supportArea = inner.reduced(12, 10);
    supportArea.removeFromTop(58);
    if (!appinfo::kSupportLinkFinalized)
        supportArea.removeFromBottom(18);
    coffeeButton.setBounds(supportArea.removeFromTop(30));
}

void AboutOverlay::paint(juce::Graphics& g)
{
    g.fillAll(Theme::bgCanvas.withAlpha(0.76f));

    if (cardBounds.isEmpty())
        return;

    const auto cardF = cardBounds.toFloat();
    g.setColour(Theme::bgSurface);
    g.fillRoundedRectangle(cardF, Theme::kCardRadius);
    g.setColour(Theme::borderFocus);
    g.drawRoundedRectangle(cardF.reduced(0.5f), Theme::kCardRadius, 1.5f);

    auto inner = cardBounds.reduced(20, 16);

    // Top Header: Title + Version badge
    auto topRow = inner.removeFromTop(32);
    g.setColour(Theme::textPrimary);
    g.setFont(Theme::makeFont(20.0f, true));
    g.drawFittedText(appinfo::kAppName, topRow.removeFromLeft(152), juce::Justification::centredLeft, 1);

    const juce::String verText = "v" + juce::String(appinfo::kVersionString);
    const auto badge = juce::Rectangle<float>(static_cast<float>(topRow.getX()),
                                              topRow.toFloat().getCentreY() - 10.0f,
                                              56.0f, 20.0f);
    g.setColour(Theme::liveAccent);
    g.fillRoundedRectangle(badge, 5.0f);
    g.setColour(Theme::bgCanvas);
    g.setFont(Theme::makeFont(11.0f, true));
    g.drawFittedText(verText, badge.toNearestInt(), juce::Justification::centred, 1);

    // Subtitle & Features
    inner.removeFromTop(4);
    g.setColour(Theme::textPrimary);
    g.setFont(Theme::makeFont(12.0f, true));
    g.drawFittedText(tr(dsp::TextKey::AboutSubtitle), inner.removeFromTop(22), juce::Justification::centredLeft, 1);

    g.setColour(Theme::textSecondary);
    g.setFont(Theme::makeFont(11.0f));
    g.drawFittedText(tr(dsp::TextKey::AboutFeatures), inner.removeFromTop(38), juce::Justification::topLeft, 2);

    // Footer Company Name
    auto footer = inner.removeFromBottom(30);
    footer.removeFromRight(96);
    g.setColour(Theme::textMuted);
    g.setFont(Theme::makeFont(10.5f));
    g.drawFittedText(appinfo::kCompanyName, footer, juce::Justification::centredLeft, 1);

    inner.removeFromBottom(10);

    // Support & Buy Me a Coffee Card
    const auto supportF = inner.toFloat();
    g.setColour(Theme::bgElevated);
    g.fillRoundedRectangle(supportF, 8.0f);
    g.setColour(Theme::warnAccent.withAlpha(0.45f));
    g.drawRoundedRectangle(supportF.reduced(0.5f), 8.0f, 1.0f);

    auto supportText = inner.reduced(12, 10);
    g.setColour(Theme::warnAccent);
    g.setFont(Theme::makeFont(12.0f, true));
    g.drawFittedText(tr(dsp::TextKey::AboutSupportTitle), supportText.removeFromTop(20), juce::Justification::centredLeft, 1);

    g.setColour(Theme::textPrimary);
    g.setFont(Theme::makeFont(11.0f));
    g.drawFittedText(tr(dsp::TextKey::AboutSupportDesc), supportText.removeFromTop(36), juce::Justification::topLeft, 2);

    if (!appinfo::kSupportLinkFinalized)
    {
        g.setColour(Theme::warnAccent.withAlpha(0.85f));
        g.setFont(Theme::makeFont(9.5f));
        g.drawFittedText(tr(dsp::TextKey::AboutPendingLinkNote),
                         supportText.removeFromBottom(16),
                         juce::Justification::centred, 1);
    }
}

void AboutOverlay::mouseDown(const juce::MouseEvent& e)
{
    if (!cardBounds.contains(e.getPosition()))
        hideOverlay();
}

} // namespace harmonizer::ui
