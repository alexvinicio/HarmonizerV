#pragma once

#include "Dsp/DspConstants.h"
#include "Plugin/PluginProcessor.h"

#include <juce_gui_basics/juce_gui_basics.h>
#include <juce_audio_processors/juce_audio_processors.h>
#include <array>
#include <functional>
#include <memory>

namespace harmonizer::ui
{

class HarmonyFeelBar final : public juce::Component
{
public:
    explicit HarmonyFeelBar(HarmonizerVProcessor& proc);

    void updateModeAccent(dsp::EngineMode mode);
    void updateTelemetry(const dsp::TelemetrySnapshot& snap);
    void resized() override;
    void paint(juce::Graphics& g) override;

private:
    class FeelPillSlider final : public juce::Component,
                                 public juce::SettableTooltipClient
    {
    public:
        enum class MeterKind
        {
            None = 0,
            TuneCents,
            DeEssGrDb
        };

        FeelPillSlider(juce::AudioProcessorValueTreeState& apvts,
                       const juce::String& paramId,
                       const juce::String& labelText,
                       std::function<juce::String(float)> formatter,
                       MeterKind kind = MeterKind::None,
                       const juce::String& tooltipText = {});

        void setAccentColour(juce::Colour c);
        void setMeterValue(float value, bool active);
        void paint(juce::Graphics& g) override;
        void mouseEnter(const juce::MouseEvent& e) override;
        void mouseExit(const juce::MouseEvent& e) override;
        void mouseDown(const juce::MouseEvent& e) override;
        void mouseDrag(const juce::MouseEvent& e) override;
        void mouseUp(const juce::MouseEvent& e) override;
        void mouseDoubleClick(const juce::MouseEvent& e) override;

    private:
        void handleValueChanged(float newPlainValue);
        float xToPlain(float x) const noexcept;

        juce::RangedAudioParameter* param = nullptr;
        std::unique_ptr<juce::ParameterAttachment> attachment;
        juce::String label;
        std::function<juce::String(float)> formatValue;
        juce::Colour accent;
        MeterKind meterKind = MeterKind::None;
        float meterValue = 0.0f;
        bool meterActive = false;
        float currentPlain = 0.0f;
        float currentNorm = 0.0f;
        bool dragging = false;
        bool hovered = false;
    };

    std::array<std::unique_ptr<FeelPillSlider>, 5> pills;
    float displayTuneCents = 0.0f;
    float displayDeEssGrDb = 0.0f;
};

} // namespace harmonizer::ui
