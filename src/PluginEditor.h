#pragma once

#include <juce_audio_processors/juce_audio_processors.h>

#include <array>

#include "gui/AnalogMeter.h"
#include "gui/BasilicaLookAndFeel.h"
#include "gui/FilmstripKnob.h"
#include "gui/FilmstripToggle.h"
#include "presets/PresetBar.h"

class SilentiumAudioProcessor;

// v0.3.1 visual overhaul editor: photoreal skeuomorphic UI built from the
// reusable src/gui/ component family (FilmstripKnob, FilmstripToggle,
// AnalogMeter, BasilicaLookAndFeel). Every visible control is wired to a
// real APVTS parameter or a real metering value - no dead decoration, per
// the basilica-gui-design skill's binding spec.
//
// v0.3.2 (this revision): the plate is a SINGLE photoreal MASTER faceplate
// render (resources/gui/faceplate-silentium-v3.png - obsidian plate, both VU
// dials, tube-vent grilles, all 9 knobs, both toggles, and the rose emblem
// all baked into one image) drawn once in paint(), replacing both the
// earlier pre-rendered faceplate_silentium_v2 PNG AND the JUCE-drawn
// glossy-black plate (gradient/reflection/bevel/header-roundel) that
// preceded it. FilmstripKnob/FilmstripToggle/AnalogMeter are positioned
// directly on top of the baked artwork (see PluginEditorLayout.h,
// re-derived from
// .scaffold/gui-assets/faceplate-silentium-v3/faceplate-metadata.json) -
// the baked knob/toggle art underneath simply holds the visual position
// until the interactive component (sized identically) draws over it every
// frame. AnalogMeter no longer draws a face image at all in this usage -
// only the live rotating needle plus a subtle incandescent glow, see
// AnalogMeter.h/.cpp.
//
// TYPOGRAPHY / LABELS: every static caption (title, knob labels, toggle
// labels) was engraved into earlier faceplate art generations; the current
// master render carries NO baked text labels at all (Yves' art direction -
// dial numerals/VU wordmark only). Accessible names are unaffected: every
// control still carries its parameter name via setTitle().
//
// Window scaling is STEPPED (100/150/200%, a UA-style corner control next to
// the preset bar, persisted as a plain property on the APVTS state tree) -
// not a free/continuous resize, because the backing art is pre-rendered at
// a fixed resolution (resources/gui/faceplate-silentium-v3.png, drawn scaled
// via RectanglePlacement::centred - see paint()).
class SilentiumAudioProcessorEditor final : public juce::AudioProcessorEditor,
                                             private juce::Timer
{
public:
    explicit SilentiumAudioProcessorEditor (SilentiumAudioProcessor& processorToEdit);
    ~SilentiumAudioProcessorEditor() override;

    void paint (juce::Graphics& g) override;
    void resized() override;

private:
    // Re-reads the processor's metering atomics and feeds AnalogMeter -
    // driven by this editor's own juce::Timer (same pattern PresetBar
    // already uses) so the audio thread never touches GUI components
    // directly (see PluginProcessor::getGainReductionDb()/getInputLevelDb()).
    // AnalogMeter's own internal timer then does the actual ~300ms
    // ballistic integration independently of this refresh rate.
    void timerCallback() override;

    using SliderAttachment = juce::AudioProcessorValueTreeState::SliderAttachment;
    using ButtonAttachment = juce::AudioProcessorValueTreeState::ButtonAttachment;

    struct Knob
    {
        std::unique_ptr<basilica::gui::FilmstripKnob> slider;
        std::unique_ptr<SliderAttachment> attachment;
    };

    struct Toggle
    {
        std::unique_ptr<basilica::gui::FilmstripToggle> button;
        std::unique_ptr<ButtonAttachment> attachment;
    };

    void configureKnob (Knob& knob, const juce::String& parameterId, const juce::String& labelText);
    void configureToggle (Toggle& toggle, const juce::String& parameterId, const juce::String& labelText);
    void applyScaleStep (int newStepIndex);
    void cycleScale();

    SilentiumAudioProcessor& audioProcessor;

    basilica::gui::BasilicaLookAndFeel lookAndFeel;

    // The single master faceplate render, drawn scaled-to-fit in paint() -
    // see PluginEditor.h's top-of-file docs and PluginEditorLayout.h. No
    // synthetic drop-shadow/gradient/bevel is applied to any control
    // anymore: the master render already bakes plausible lighting/shadowing
    // for every knob/toggle/meter, and layering a JUCE-drawn shadow on top
    // read as exactly the "Frankenstein" mismatch Yves rejected.
    juce::Image faceplateImage;

    basilica::presets::PresetBar presetBar;
    juce::TextButton scaleButton;
    int scaleStepIndex = 0; // 0 = 100%, 1 = 150%, 2 = 200%

    basilica::gui::AnalogMeter gainReductionMeter;
    basilica::gui::AnalogMeter inputLevelMeter;

    static constexpr int numKnobs = 9;
    std::array<Knob, numKnobs> knobs;

    static constexpr int numToggles = 2;
    std::array<Toggle, numToggles> toggles;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SilentiumAudioProcessorEditor)
};
