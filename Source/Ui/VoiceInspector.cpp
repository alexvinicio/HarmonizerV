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
    for (auto* btn : { &onBtn, &muteBtn, &soloBtn, &fixedBtn })
    {
        btn->setClickingTogglesState(true);
        addAndMakeVisible(btn);
    }

    muteBtn.setColour(juce::TextButton::buttonOnColourId, Theme::muteAccent);
    muteBtn.setColour(juce::TextButton::textColourOnId, Theme::textPrimary);
    soloBtn.setColour(juce::TextButton::buttonOnColourId, Theme::warnAccent);
    soloBtn.setColour(juce::TextButton::textColourOnId, Theme::bgCanvas);
    fixedBtn.setTooltip("Lock voice to a fixed pedal note instead of scale interval");

    intervalKnob.setValueFormatter([this](double v)
    {
        bool isFixed = false;
        if (auto* rawFix = processor.getApvts().getRawParameterValue(params::voiceParamId(selectedVoice, params::vFixed)))
            isFixed = rawFix->load(std::memory_order_relaxed) >= 0.5f;

        if (isFixed)
        {
            static constexpr std::array<const char*, 12> kNotes = {
                "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"
            };
            const int note = std::clamp(static_cast<int>(std::lround(v)), 0, 127);
            return juce::String(kNotes[static_cast<size_t>(note % 12)]) + juce::String((note / 12) - 1);
        }

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

    vibratoKnob.setValueFormatter([](double v)
    {
        return juce::String(std::lround(v)) + "%";
    });

    humanizeKnob.setValueFormatter([](double v)
    {
        return juce::String(std::lround(v)) + "%";
    });

    for (auto* k : { &intervalKnob, &detuneKnob, &formantKnob, &delayKnob,
                     &hpfKnob, &lpfKnob, &panKnob, &levelKnob, &vibratoKnob, &humanizeKnob })
    {
        addAndMakeVisible(k);
    }

    humanizeKnob.attach(processor.getApvts(), params::humanize);
    processor.getApvts().addParameterListener(params::scale, this);
    for (int v = 0; v < dsp::kExposedVoices; ++v)
        processor.getApvts().addParameterListener(params::voiceParamId(v, params::vFixed), this);

    bindSelectedVoice();
}

VoiceInspector::~VoiceInspector()
{
    processor.getApvts().removeParameterListener(params::scale, this);
    for (int v = 0; v < dsp::kExposedVoices; ++v)
        processor.getApvts().removeParameterListener(params::voiceParamId(v, params::vFixed), this);
    onAtt.reset();
    muteAtt.reset();
    soloAtt.reset();
    fixedAtt.reset();
    for (auto* k : { &intervalKnob, &detuneKnob, &formantKnob, &delayKnob,
                     &hpfKnob, &lpfKnob, &panKnob, &levelKnob, &vibratoKnob, &humanizeKnob })
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
    else if (parameterID == params::voiceParamId(selectedVoice, params::vFixed))
    {
        juce::MessageManager::callAsync([safe = juce::Component::SafePointer<VoiceInspector>(this)]()
        {
            if (safe != nullptr)
            {
                safe->bindSelectedVoice();
                safe->repaint();
            }
        });
    }
}

void VoiceInspector::setSelectedVoice(int voiceIndex)
{
    const int clamped = std::clamp(voiceIndex, 0, dsp::kExposedVoices - 1);
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
    fixedAtt.reset();

    intervalKnob.detach();
    detuneKnob.detach();
    formantKnob.detach();
    delayKnob.detach();
    hpfKnob.detach();
    lpfKnob.detach();
    panKnob.detach();
    levelKnob.detach();
    vibratoKnob.detach();

    const auto col = Theme::getVoiceColour(selectedVoice);
    onBtn.setColour(juce::TextButton::buttonOnColourId, col);
    onBtn.setColour(juce::TextButton::textColourOnId, Theme::bgCanvas);
    fixedBtn.setColour(juce::TextButton::buttonOnColourId, col);
    fixedBtn.setColour(juce::TextButton::textColourOnId, Theme::bgCanvas);

    for (auto* k : { &intervalKnob, &detuneKnob, &formantKnob, &delayKnob,
                     &hpfKnob, &lpfKnob, &panKnob, &levelKnob, &vibratoKnob })
    {
        k->setAccentColour(col);
    }

    auto& apvts = processor.getApvts();
    onAtt    = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(apvts, params::voiceParamId(selectedVoice, params::vEnabled), onBtn);
    muteAtt  = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(apvts, params::voiceParamId(selectedVoice, params::vMute), muteBtn);
    soloAtt  = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(apvts, params::voiceParamId(selectedVoice, params::vSolo), soloBtn);
    fixedAtt = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(apvts, params::voiceParamId(selectedVoice, params::vFixed), fixedBtn);

    bool isFixed = false;
    if (auto* rawFix = apvts.getRawParameterValue(params::voiceParamId(selectedVoice, params::vFixed)))
        isFixed = rawFix->load(std::memory_order_relaxed) >= 0.5f;

    intervalKnob.setTitle(isFixed ? "FIX NOTE" : "INTERVAL");
    intervalKnob.attach(apvts, params::voiceParamId(selectedVoice, isFixed ? params::vFixedNote : params::vInterval));
    detuneKnob.attach(apvts, params::voiceParamId(selectedVoice, params::vDetune));
    formantKnob.attach(apvts, params::voiceParamId(selectedVoice, params::vFormant));
    delayKnob.attach(apvts, params::voiceParamId(selectedVoice, params::vDelayMs));
    hpfKnob.attach(apvts, params::voiceParamId(selectedVoice, params::vHpfHz));
    lpfKnob.attach(apvts, params::voiceParamId(selectedVoice, params::vLpfHz));
    panKnob.attach(apvts, params::voiceParamId(selectedVoice, params::vPan));
    levelKnob.attach(apvts, params::voiceParamId(selectedVoice, params::vGainDb));
    vibratoKnob.attach(apvts, params::voiceParamId(selectedVoice, params::vVibrato));
}

void VoiceInspector::resized()
{
    auto r = getLocalBounds().reduced(14, 10);

    // Left voice header + FIX toggle + On/M/S buttons
    auto leftCol = r.removeFromLeft(110);
    auto titleRow = leftCol.removeFromTop(22);
    fixedBtn.setBounds(titleRow.removeFromRight(40).reduced(0, 1));
    leftCol.removeFromTop(3);
    onBtn.setBounds(leftCol.removeFromTop(25).withWidth(98));
    leftCol.removeFromTop(5);
    auto rowBot = leftCol.removeFromTop(24);
    muteBtn.setBounds(rowBot.removeFromLeft(46));
    rowBot.removeFromLeft(6);
    soloBtn.setBounds(rowBot.removeFromLeft(46));

    r.removeFromLeft(8);

    // Right global Humanize knob
    auto rightCol = r.removeFromRight(80);
    humanizeKnob.setBounds(rightCol.reduced(2, 2));

    dividerBounds = r.removeFromRight(10).withSizeKeepingCentre(1, r.getHeight() - 16);

    // 9 voice knobs evenly distributed
    std::array<LabeledKnob*, 9> knobs = {
        &intervalKnob, &detuneKnob, &formantKnob, &delayKnob,
        &hpfKnob, &lpfKnob, &panKnob, &levelKnob, &vibratoKnob
    };
    const int knobW = std::max(52, r.getWidth() / static_cast<int>(knobs.size()));
    for (auto* k : knobs)
        k->setBounds(r.removeFromLeft(knobW).reduced(1, 2));
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
    const auto titleArea = getLocalBounds().reduced(14, 10).withWidth(68).withHeight(20);
    g.setColour(voiceCol);
    g.setFont(Theme::makeFont(12.5f, true));
    g.drawFittedText("VOICE " + juce::String(selectedVoice + 1), titleArea, juce::Justification::centredLeft, 1);

    // Subtle divider before Humanize
    g.setColour(Theme::borderSubtle);
    g.fillRect(dividerBounds);
}

} // namespace harmonizer::ui
