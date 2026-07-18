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
// v0.3.2: the plate background switched from a pre-rendered photoreal PNG
// (resources/gui/faceplate_silentium_v2_*.png, still bundled via BinaryData
// but no longer loaded/drawn anywhere - Yves' call, left in place for manual
// cleanup) to a JUCE-drawn glossy-black surface painted directly in paint()
// - base gradient, a broad soft upper-left reflection, and a hairline outer
// bevel, styled after brand/mock-raytrace-1-frontal.png. See paint()'s docs.
//
// TYPOGRAPHY / LABELS: since v0.3.1 every static caption (title, knob
// labels, toggle labels) was ENGRAVED into the faceplate PNG itself (EB
// Garamond, gold inlay, rendered by the Blender pipeline) - there are no
// juce::Label captions in this editor. Since the v0.3.2 background swap
// those baked captions are GONE (the plate art that carried them is no
// longer drawn) - only the knob-grid/aux-bay geometry from the same
// slnt::layout table remains. Accessible names are unaffected: every
// control still carries its parameter name via setTitle().
//
// Window scaling is STEPPED (100/150/200%, a UA-style corner control next to
// the preset bar, persisted as a plain property on the APVTS state tree) -
// not a free/continuous resize, because the backing art is pre-rendered at
// fixed density tiers (see src/gui/ImageDensity.h).
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

    juce::Image brandIconImage;

    // v0.3.2: shared drop-shadow, applied via setComponentEffect() to every
    // meter and knob so they read as objects sitting ON the glossy plate
    // rather than pasted flat onto it (per brand/mock-raytrace-1-frontal.png
    // - see the constructor and applyScaleStep()). One instance is shared
    // across all of them since DropShadowEffect is stateless between
    // applyEffect() calls and painting is always single-threaded on the
    // message thread - not per-component state.
    juce::DropShadowEffect controlShadowEffect;

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
