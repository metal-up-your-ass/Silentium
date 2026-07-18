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
// AnalogMeter, BasilicaLookAndFeel) plus the pre-rendered faceplate PNG (see
// .scaffold/gui-assets/faceplate-silentium-v2/README.md). Every visible
// control is wired to a real APVTS parameter or a real metering value - no
// dead decoration, per the basilica-gui-design skill's binding spec.
//
// TYPOGRAPHY / LABELS: since v0.3.1 every static caption (title, knob
// labels, toggle labels) is ENGRAVED into the faceplate PNG itself (EB
// Garamond, gold inlay, rendered by the Blender pipeline) - there are no
// juce::Label captions in this editor any more. The controls' positions come
// from the same slnt::layout table the plate art was authored against
// (src/PluginEditorLayout.h documents that contract), so the baked labels
// always line up with the live controls. Accessible names are unaffected:
// every control still carries its parameter name via setTitle().
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

    juce::Image facePlateImage1x, facePlateImage2x;
    juce::Image brandIconImage;

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
