#include "Ui/VoiceInspector.h"
#include "Dsp/ScaleHarmonizer.h"
#include "Plugin/ParameterIds.h"
#include "Ui/Theme.h"

#include <algorithm>
#include <cmath>

namespace harmonizer::ui
{

VoiceInspector::VoiceInspector(HarmonizerVProcessor& proc)
    : processor(proc)
{
    for (auto* btn : { &onBtn, &muteBtn, &soloBtn })
    {
        btn->setClickingTogglesState(true);
        addAndMakeVisible(btn);
    }

    muteBtn.setColour(juce::TextButton::buttonOnColourId, Theme::muteAccent);
    muteBtn.setColour(juce::TextButton::textColourOnId, Theme::textPrimary);
    soloBtn.setColour(juce::TextButton::buttonOnColourId, Theme::warnAccent);
    soloBtn.setColour(juce::TextButton::textColourOnId, Theme::bgCanvas);

    intervalKnob.setValueFormatter([this](double v)
    {
        int scaleIdx = 0;
        if (auto* raw = processor.getApvts().getRawParameterValue(params::scale))
            scaleIdx = static_cast<int>(std::lround(raw->load(std::memory_order_relaxed)));
        return juce::String(dsp::ScaleHarmonizer::formatIntervalLabel(
            static_cast<int>(std::lround(v)),
            static_cast<dsp::ScaleType>(scaleIdx)));
    });

    detuneKnob.setValueFormatter([](double v)
    {
        const int c = static_cast<int>(std::lround(v));
        return (c > 0 ? "+" : "") + juce::String(c) + " ct";
    });

    formantKnob.setValueFormatter([](double v)
    {
        return (v > 0.05 ? "+" : "") + juce::String(v, 1) + " st";
    });

    delayKnob.setValueFormatter([](double v)
    {
        return juce::String(v, 1) + " ms";
    });

    hpfKnob.setValueFormatter([](double v)
    {
        return juce::String(std::lround(v)) + " Hz";
    });

    lpfKnob.setValueFormatter([](double v)
    {
        if (v >= 1000.0)
            return juce::String(v / 1000.0, 1) + " kHz";
        return juce::String(std::lround(v)) + " Hz";
    });

    panKnob.setValueFormatter([](double v)
    {
        const int p = static_cast<int>(std::lround(v));
        if (p == 0)
            return juce::String("C");
        return (p < 0 ? "L" : "R") + juce::String(std::abs(p));
    });

    levelKnob.setValueFormatter([](double v)
    {
        if (v <= -59.5)
            return juce::String("-inf");
        return (v > 0.05 ? "+" : "") + juce::String(v, 1) + " dB";
    });

    humanizeKnob.setValueFormatter([](double v)
    {
        return juce::String(std::lround(v)) + "%";
    });

    for (auto* k : { &intervalKnob, &detuneKnob, &formantKnob, &delayKnob,
                     &hpfKnob, &lpfKnob, &panKnob, &levelKnob, &humanizeKnob })
    {
        addAndMakeVisible(k);
    }

    humanizeKnob.attach(processor.getApvts(), params::humanize);
    processor.getApvts().addParameterListener(params::scale, this);

    bindSelectedVoice();
}

VoiceInspector::~VoiceInspector()
{
    processor.getApvts().removeParameterListener(params::scale, this);
    onAtt.reset();
    muteAtt.reset();
    soloAtt.reset();
    for (auto* k : { &intervalKnob, &detuneKnob, &formantKnob, &delayKnob,
                     &hpfKnob, &lpfKnob, &panKnob, &levelKnob, &humanizeKnob })
    {
        k->detach();
    }
}

void VoiceInspector::parameterChanged(const juce::String& parameterID, float)
{
    if (parameterID == params::scale)
    {
        juce::MessageManager::callAsync([safe = juce::Component::SafePointer<VoiceInspector>(this)]()
        {
            if (safe != nullptr)
                safe->intervalKnob.refreshDisplay();
        });
    }
}

void VoiceInspector::setSelectedVoice(int voiceIndex)
{
    const int clamped = std::clamp(voiceIndex, 0, dsp::kExposedVoicesV1 - 1);
    if (selectedVoice == clamped)
        return;
    selectedVoice = clamped;
    bindSelectedVoice();
    repaint();
}

void VoiceInspector::bindSelectedVoice()
{
    // Always tear down previous attachments before creating new ones
    onAtt.reset();
    muteAtt.reset();
    soloAtt.reset();

    intervalKnob.detach();
    detuneKnob.detach();
    formantKnob.detach();
    delayKnob.detach();
    hpfKnob.detach();
    lpfKnob.detach();
    panKnob.detach();
    levelKnob.detach();

    const auto col = Theme::getVoiceColour(selectedVoice);
    onBtn.setColour(juce::TextButton::buttonOnColourId, col);
    onBtn.setColour(juce::TextButton::textColourOnId, Theme::bgCanvas);

    for (auto* k : { &intervalKnob, &detuneKnob, &formantKnob, &delayKnob,
                     &hpfKnob, &lpfKnob, &panKnob, &levelKnob })
    {
        k->setAccentColour(col);
    }

    auto& apvts = processor.getApvts();
    onAtt   = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(apvts, params::voiceParamId(selectedVoice, params::vEnabled), onBtn);
    muteAtt = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(apvts, params::voiceParamId(selectedVoice, params::vMute), muteBtn);
    soloAtt = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(apvts, params::voiceParamId(selectedVoice, params::vSolo), soloBtn);

    intervalKnob.attach(apvts, params::voiceParamId(selectedVoice, params::vInterval));
    detuneKnob.attach(apvts, params::voiceParamId(selectedVoice, params::vDetune));
    formantKnob.attach(apvts, params::voiceParamId(selectedVoice, params::vFormant));
    delayKnob.attach(apvts, params::voiceParamId(selectedVoice, params::vDelayMs));
    hpfKnob.attach(apvts, params::voiceParamId(selectedVoice, params::vHpfHz));
    lpfKnob.attach(apvts, params::voiceParamId(selectedVoice, params::vLpfHz));
    panKnob.attach(apvts, params::voiceParamId(selectedVoice, params::vPan));
    levelKnob.attach(apvts, params::voiceParamId(selectedVoice, params::vGainDb));
}

void VoiceInspector::resized()
{
    auto r = getLocalBounds().reduced(14, 10);

    // Left voice header + On/M/S buttons
    auto leftCol = r.removeFromLeft(110);
    leftCol.removeFromTop(24); // Title painted here
    auto rowTop = leftCol.removeFromTop(26);
    onBtn.setBounds(rowTop.withWidth(94));
    leftCol.removeFromTop(6);
    auto rowBot = leftCol.removeFromTop(24);
    muteBtn.setBounds(rowBot.removeFromLeft(44));
    rowBot.removeFromLeft(6);
    soloBtn.setBounds(rowBot.removeFromLeft(44));

    r.removeFromLeft(10);

    // Right global Humanize knob
    auto rightCol = r.removeFromRight(84);
    humanizeKnob.setBounds(rightCol.reduced(4, 2));

    dividerBounds = r.removeFromRight(12).withSizeKeepingCentre(1, r.getHeight() - 16);

    // 8 voice knobs evenly distributed
    std::array<LabeledKnob*, 8> knobs = {
        &intervalKnob, &detuneKnob, &formantKnob, &delayKnob,
        &hpfKnob, &lpfKnob, &panKnob, &levelKnob
    };
    const int knobW = std::max(54, r.getWidth() / static_cast<int>(knobs.size()));
    for (auto* k : knobs)
        k->setBounds(r.removeFromLeft(knobW).reduced(2, 2));
}

void VoiceInspector::paint(juce::Graphics& g)
{
    const auto card = getLocalBounds().toFloat();
    g.setColour(Theme::bgSurface);
    g.fillRoundedRectangle(card, Theme::kCardRadius);
    g.setColour(Theme::borderSubtle);
    g.drawRoundedRectangle(card.reduced(0.5f), Theme::kCardRadius, 1.0f);

    const auto voiceCol = Theme::getVoiceColour(selectedVoice);

    // Left accent stripe
    g.setColour(voiceCol);
    g.fillRoundedRectangle(juce::Rectangle<float>(card.getX() + 1.0f, card.getY() + 12.0f, 3.5f, card.getHeight() - 24.0f), 2.0f);

    // Selected voice label
    const auto titleArea = getLocalBounds().reduced(14, 10).withWidth(104).withHeight(20);
    g.setColour(voiceCol);
    g.setFont(Theme::makeFont(12.5f, true));
    g.drawFittedText("VOICE " + juce::String(selectedVoice + 1), titleArea, juce::Justification::centredLeft, 1);

    // Subtle divider before Humanize
    g.setColour(Theme::borderSubtle);
    g.fillRect(dividerBounds);
}

} // namespace harmonizer::ui
