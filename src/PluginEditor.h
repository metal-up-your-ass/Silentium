#pragma once

#include <juce_audio_processors/juce_audio_processors.h>

class SilentiumAudioProcessor;

// A simple, functional v0.1 editor: one rotary slider per parameter, bound
// to the APVTS via SliderAttachment. A custom vector-drawn GUI is a later
// milestone; this is deliberately plain but fully wired and usable.
class SilentiumAudioProcessorEditor final : public juce::AudioProcessorEditor
{
public:
    explicit SilentiumAudioProcessorEditor (SilentiumAudioProcessor& processorToEdit);
    ~SilentiumAudioProcessorEditor() override;

    void resized() override;

private:
    using SliderAttachment = juce::AudioProcessorValueTreeState::SliderAttachment;
    using ButtonAttachment = juce::AudioProcessorValueTreeState::ButtonAttachment;

    // One knob + label per float parameter, in signal-flow order.
    struct Knob
    {
        juce::Slider slider;
        juce::Label label;
        std::unique_ptr<SliderAttachment> attachment;
    };

    // One toggle button per bool parameter (Duck, Listen).
    struct Toggle
    {
        juce::ToggleButton button;
        std::unique_ptr<ButtonAttachment> attachment;
    };

    void configureKnob (Knob& knob, const juce::String& parameterId, const juce::String& labelText);
    void configureToggle (Toggle& toggle, const juce::String& parameterId, const juce::String& labelText);

    SilentiumAudioProcessor& audioProcessor;

    Knob thresholdKnob;
    Knob attackKnob;
    Knob holdKnob;
    Knob releaseKnob;
    Knob rangeKnob;
    Knob lookaheadKnob;
    Knob scHighpassKnob;
    Knob kneeKnob;

    Toggle duckToggle;
    Toggle listenToggle;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SilentiumAudioProcessorEditor)
};
