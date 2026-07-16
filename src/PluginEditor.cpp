#include "PluginEditor.h"
#include "PluginProcessor.h"
#include "params/ParameterIds.h"
#include "presets/Localisation.h"

#include <BinaryData.h>

namespace
{
    constexpr int knobSize = 100;
    constexpr int textBoxHeight = 20;
    constexpr int labelHeight = 20;
    constexpr int margin = 20;
    constexpr int numKnobs = 8;
    constexpr int toggleRowHeight = 24;
    constexpr int presetBarHeight = 28;
    constexpr int editorWidth = margin * 2 + numKnobs * knobSize + (numKnobs - 1) * margin;
    constexpr int editorHeight = margin * 4 + presetBarHeight + labelHeight + knobSize + textBoxHeight + toggleRowHeight;

    // M2 i18n frame (.scaffold/specs/preset-system-m2.md): selects German
    // (resources/i18n/de.txt) or falls through to English, once, at editor
    // construction - see Localisation.h's docs. `presetBar` is a member
    // initialised via the constructor's initialiser list, and its own
    // constructor already calls TRANS() on every button label - member
    // initialisers run in declaration order regardless of the order
    // they're written in, so this helper (called from presetBar's own
    // initialiser expression below) is what actually guarantees
    // installLocalisation() runs before presetBar exists, not an
    // installLocalisation() call in the constructor *body*, which would run
    // too late.
    basilica::presets::PresetManager& initLocalisationThenGetPresetManager (SilentiumAudioProcessor& processor)
    {
        basilica::presets::installLocalisation (BinaryData::de_txt, BinaryData::de_txtSize);
        return processor.presetManager;
    }
}

SilentiumAudioProcessorEditor::SilentiumAudioProcessorEditor (SilentiumAudioProcessor& processorToEdit)
    : juce::AudioProcessorEditor (&processorToEdit),
      audioProcessor (processorToEdit),
      presetBar (initLocalisationThenGetPresetManager (processorToEdit))
{
    addAndMakeVisible (presetBar);

    configureKnob (thresholdKnob, ParamIDs::threshold, "Threshold");
    configureKnob (attackKnob, ParamIDs::attack, "Attack");
    configureKnob (holdKnob, ParamIDs::hold, "Hold");
    configureKnob (releaseKnob, ParamIDs::release, "Release");
    configureKnob (rangeKnob, ParamIDs::range, "Range");
    configureKnob (lookaheadKnob, ParamIDs::lookahead, "Lookahead");
    configureKnob (scHighpassKnob, ParamIDs::scHighpass, "SC HPF");
    configureKnob (kneeKnob, ParamIDs::knee, "Knee");

    configureToggle (duckToggle, ParamIDs::duck, "Duck");
    configureToggle (listenToggle, ParamIDs::listen, "Listen");

    setResizable (false, false);
    setSize (editorWidth, editorHeight);
}

SilentiumAudioProcessorEditor::~SilentiumAudioProcessorEditor() = default;

void SilentiumAudioProcessorEditor::configureKnob (Knob& knob, const juce::String& parameterId, const juce::String& labelText)
{
    knob.slider.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
    knob.slider.setTextBoxStyle (juce::Slider::TextBoxBelow, false, knobSize, textBoxHeight);
    addAndMakeVisible (knob.slider);

    knob.label.setText (labelText, juce::dontSendNotification);
    knob.label.setJustificationType (juce::Justification::centred);
    // false => label sits above the slider it tracks; JUCE repositions it
    // automatically whenever the slider's bounds change, so resized() only
    // needs to place the sliders themselves.
    knob.label.attachToComponent (&knob.slider, false);
    addAndMakeVisible (knob.label);

    knob.attachment = std::make_unique<SliderAttachment> (audioProcessor.apvts, parameterId, knob.slider);
}

void SilentiumAudioProcessorEditor::configureToggle (Toggle& toggle, const juce::String& parameterId, const juce::String& labelText)
{
    toggle.button.setButtonText (labelText);
    addAndMakeVisible (toggle.button);

    toggle.attachment = std::make_unique<ButtonAttachment> (audioProcessor.apvts, parameterId, toggle.button);
}

void SilentiumAudioProcessorEditor::resized()
{
    auto bounds = getLocalBounds().reduced (margin);

    presetBar.setBounds (bounds.removeFromTop (presetBarHeight));
    bounds.removeFromTop (margin);

    // Duck/Listen toggles: a row along the bottom, reserved first so the
    // knob row above gets the remaining height.
    auto toggleRow = bounds.removeFromBottom (toggleRowHeight);
    bounds.removeFromBottom (margin / 2);

    const auto toggleWidth = toggleRow.getWidth() / 2;
    duckToggle.button.setBounds (toggleRow.removeFromLeft (toggleWidth).reduced (margin / 2, 0));
    listenToggle.button.setBounds (toggleRow.reduced (margin / 2, 0));

    bounds.removeFromTop (labelHeight); // room for the attached labels above each knob

    const auto slotWidth = bounds.getWidth() / numKnobs;

    for (auto* knob : { &thresholdKnob, &attackKnob, &holdKnob, &releaseKnob, &rangeKnob, &lookaheadKnob, &scHighpassKnob, &kneeKnob })
        knob->slider.setBounds (bounds.removeFromLeft (slotWidth).reduced (margin / 2, 0));
}
