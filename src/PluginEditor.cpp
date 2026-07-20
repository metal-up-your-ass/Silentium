#include "PluginEditor.h"
#include "PluginEditorLayout.h"
#include "PluginProcessor.h"
#include "params/ParameterIds.h"
#include "presets/Localisation.h"

#include <BinaryData.h>

namespace
{
    // Base (@1x, 100% scale) faceplate geometry lives in PluginEditorLayout.h
    // (slnt::layout) rather than here, so tests/gui/EditorLayoutTests.cpp can
    // assert layout invariants against the exact constants this file lays
    // components out with - see that header's docs.
    using namespace slnt::layout;

    struct KnobLayoutEntry
    {
        const char* parameterId;
        const char* labelText; // accessible name only - the master render carries no baked text labels
        int centreX1x;
        int centreY1x;
    };

    // Signal-flow-grouped: row 1 is the primary gate shape (Threshold
    // through Range), row 2 is the voicing/refinement controls (Lookahead,
    // the sidechain filters, Knee) - the same grouping ParameterLayout.cpp's
    // own comments use. Positions are the master render's own STAGGERED knob
    // centres (PluginEditorLayout.h's knobRow1X1x/knobRow2X1x, re-derived
    // from faceplate-metadata.json) - row 2 sits offset ~half a cell right
    // of row 1, not a straight grid, so each entry carries its own explicit
    // centre rather than a (col, row) pair.
    constexpr std::array<KnobLayoutEntry, 9> knobLayout {
        KnobLayoutEntry { ParamIDs::threshold, "Threshold", knobRow1X1x[0], knobRow1Y1x },
        KnobLayoutEntry { ParamIDs::attack, "Attack", knobRow1X1x[1], knobRow1Y1x },
        KnobLayoutEntry { ParamIDs::hold, "Hold", knobRow1X1x[2], knobRow1Y1x },
        KnobLayoutEntry { ParamIDs::release, "Release", knobRow1X1x[3], knobRow1Y1x },
        KnobLayoutEntry { ParamIDs::range, "Range", knobRow1X1x[4], knobRow1Y1x },
        KnobLayoutEntry { ParamIDs::lookahead, "Lookahead", knobRow2X1x[0], knobRow2Y1x },
        KnobLayoutEntry { ParamIDs::scHighpass, "SC HPF", knobRow2X1x[1], knobRow2Y1x },
        KnobLayoutEntry { ParamIDs::scLowpass, "SC LPF", knobRow2X1x[2], knobRow2Y1x },
        KnobLayoutEntry { ParamIDs::knee, "Knee", knobRow2X1x[3], knobRow2Y1x },
    };

    struct ToggleLayoutEntry
    {
        const char* parameterId;
        const char* labelText;
    };

    constexpr std::array<ToggleLayoutEntry, 2> toggleLayout {
        ToggleLayoutEntry { ParamIDs::duck, "Duck" },
        ToggleLayoutEntry { ParamIDs::listen, "Listen" },
    };

    juce::Image loadImage (const char* data, int size)
    {
        return juce::ImageCache::getFromMemory (data, size);
    }

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

    // Non-parameter, per-session UI state: the stepped scale choice (0/1/2)
    // stored as a plain property directly on apvts.state. This ValueTree is
    // exactly what getStateInformation()/setStateInformation() serialise (see
    // PluginProcessor.cpp), so a property set here round-trips through host
    // session save/reload the same way the registered parameters do, without
    // needing its own parameter (a scale step is a view choice, not
    // something that should be host-automatable or appear in a DAW's
    // parameter list).
    constexpr const char* uiScaleStepProperty = "uiScaleStep";

    basilica::gui::AnalogMeter::Assets makeMeterAssets()
    {
        // v0.3.2 (this revision): the dial FACE is baked into the shared
        // master faceplate background (see PluginEditor.h's docs) - only
        // the live rotating needle is still a component-owned asset.
        // assets.face is deliberately left default/invalid, which AnalogMeter::
        // paint() treats as "skip the face draw entirely".
        basilica::gui::AnalogMeter::Assets assets;
        assets.needle = loadImage (BinaryData::vuneedlemasterv3_png, BinaryData::vuneedlemasterv3_pngSize);
        return assets;
    }
}

SilentiumAudioProcessorEditor::SilentiumAudioProcessorEditor (SilentiumAudioProcessor& processorToEdit)
    : juce::AudioProcessorEditor (&processorToEdit),
      audioProcessor (processorToEdit),
      presetBar (initLocalisationThenGetPresetManager (processorToEdit)),
      gainReductionMeter (makeMeterAssets(), "Gain Reduction meter", 0.0f),
      inputLevelMeter (makeMeterAssets(), "Input Level meter", 1.0f)
{
    setLookAndFeel (&lookAndFeel);

    faceplateImage = loadImage (BinaryData::faceplatesilentiumv3_png, BinaryData::faceplatesilentiumv3_pngSize);

    // Creation order below doubles as the accessibility/keyboard focus
    // order (JUCE's default FocusTraverser walks children in z-order,
    // i.e. creation order, when no custom traverser is installed) - kept
    // deliberately matching the visual reading order: preset bar + scale
    // control, meters (GR then Input), knob grid row-by-row, then the two
    // footer toggles.
    addAndMakeVisible (presetBar);

    // A-05 fix (M3 a11y review): button text/title are set from
    // applyScaleStep() below, which runs once here at construction (with
    // the stored/default step) and again on every subsequent click, so the
    // accessible name always reflects the CURRENT scale instead of a static
    // "Window scale" that never updates (see applyScaleStep()'s docs).
    // componentID is set purely so tests/gui/EditorAccessibilityTests.cpp
    // can find this button without depending on its (now dynamic) title.
    scaleButton.setComponentID ("scaleButton");
    scaleButton.onClick = [this] { cycleScale(); };
    addAndMakeVisible (scaleButton);

    addAndMakeVisible (gainReductionMeter);
    addAndMakeVisible (inputLevelMeter);

    const auto knobStrip1x = loadImage (BinaryData::knob_brass_v2_strip_160px_128f_png,
                                        BinaryData::knob_brass_v2_strip_160px_128f_pngSize);
    const auto knobStrip2x = loadImage (BinaryData::knob_brass_v2_strip_320px_128f_png,
                                        BinaryData::knob_brass_v2_strip_320px_128f_pngSize);

    for (size_t i = 0; i < knobLayout.size(); ++i)
    {
        auto& entry = knobLayout[i];
        knobs[i].slider = std::make_unique<basilica::gui::FilmstripKnob> (knobStrip1x, knobStrip2x, 128);
        configureKnob (knobs[i], entry.parameterId, entry.labelText);
    }

    const auto toggleStrip1x = loadImage (BinaryData::toggle_brass_v2_strip_40px_4f_png,
                                          BinaryData::toggle_brass_v2_strip_40px_4f_pngSize);
    const auto toggleStrip2x = loadImage (BinaryData::toggle_brass_v2_strip_80px_4f_png,
                                          BinaryData::toggle_brass_v2_strip_80px_4f_pngSize);

    for (size_t i = 0; i < toggleLayout.size(); ++i)
    {
        auto& entry = toggleLayout[i];
        toggles[i].button = std::make_unique<basilica::gui::FilmstripToggle> (entry.labelText, toggleStrip1x, toggleStrip2x);
        configureToggle (toggles[i], entry.parameterId, entry.labelText);
    }

    setResizable (false, false);

    const auto storedStep = (int) audioProcessor.apvts.state.getProperty (uiScaleStepProperty, 0);
    applyScaleStep (juce::jlimit (0, (int) scaleSteps.size() - 1, storedStep));

    startTimerHz (30);
}

SilentiumAudioProcessorEditor::~SilentiumAudioProcessorEditor()
{
    setLookAndFeel (nullptr);
}

void SilentiumAudioProcessorEditor::configureKnob (Knob& knob, const juce::String& parameterId, const juce::String& labelText)
{
    knob.slider->setPopupDisplayEnabled (true, true, this);
    knob.slider->setTitle (labelText);
    knob.slider->setName (labelText);
    addAndMakeVisible (*knob.slider);

    if (auto* param = audioProcessor.apvts.getParameter (parameterId))
    {
        const auto defaultValue = param->getNormalisableRange().convertFrom0to1 (param->getDefaultValue());
        knob.slider->setDoubleClickReturnValue (true, defaultValue);
    }

    // SliderAttachment MUST be constructed before the textFromValueFunction
    // override below, not after: JUCE 8.0.14's SliderParameterAttachment
    // constructor (juce_ParameterAttachments.cpp:128) itself assigns
    // `slider.textFromValueFunction = [&param] (double v) { return
    // param.getText (...); }` (no unit) as part of wiring the attachment -
    // setting our own function BEFORE this point would be silently
    // clobbered the moment the attachment is created.
    knob.attachment = std::make_unique<SliderAttachment> (audioProcessor.apvts, parameterId, *knob.slider);

    if (auto* param = audioProcessor.apvts.getParameter (parameterId))
    {
        // A-02 fix (M3 a11y review): every parameter declares its unit via
        // .withLabel() in ParameterLayout.cpp (dB/ms/Hz), but that metadata
        // was never read by the GUI layer - SliderAttachment's own
        // textFromValueFunction (see above) formats the value but drops the
        // unit entirely. This feeds BOTH the popup value display
        // (setPopupDisplayEnabled above) and the accessibility value string
        // (juce_Slider.cpp:1811's
        // SliderAccessibilityHandler::ValueInterface::getCurrentValueAsString()
        // calls Slider::getTextFromValue(), which calls this same function),
        // so one fix here covers both surfaces. Still uses the parameter's
        // own getText() (not just a raw suffix) so the reported precision/
        // rounding matches what the host itself would display.
        knob.slider->textFromValueFunction = [param] (double v)
        {
            return param->getText (param->convertTo0to1 ((float) v), 0) + " " + param->getLabel();
        };
        knob.slider->updateText();
    }
}

void SilentiumAudioProcessorEditor::configureToggle (Toggle& toggle, const juce::String& parameterId, const juce::String& labelText)
{
    toggle.button->setTitle (labelText);
    toggle.button->setName (labelText);
    addAndMakeVisible (*toggle.button);

    toggle.attachment = std::make_unique<ButtonAttachment> (audioProcessor.apvts, parameterId, *toggle.button);
}

void SilentiumAudioProcessorEditor::cycleScale()
{
    applyScaleStep ((scaleStepIndex + 1) % (int) scaleSteps.size());
}

void SilentiumAudioProcessorEditor::applyScaleStep (int newStepIndex)
{
    scaleStepIndex = juce::jlimit (0, (int) scaleSteps.size() - 1, newStepIndex);
    audioProcessor.apvts.state.setProperty (uiScaleStepProperty, scaleStepIndex, nullptr);

    const auto percentText = juce::String ((int) (scaleSteps[(size_t) scaleStepIndex] * 100.0f)) + "%";
    scaleButton.setButtonText (percentText);

    // A-05 fix (M3 a11y review): an explicitly-set AccessibilityHandler
    // title always wins over the button's own text for screen readers
    // (JUCE 8.0.14 juce_ButtonAccessibilityHandler.h:67-75), so a title set
    // once at construction and never updated would silently strand AT users
    // on "Window scale" forever, with no way to learn the plugin is now at
    // 150%/200%. Re-setting the title here, alongside the visible text, on
    // every step change (construction included, since this runs from the
    // constructor too) keeps both surfaces in sync.
    scaleButton.setTitle ("Window scale, " + percentText);

    const auto scale = scaleSteps[(size_t) scaleStepIndex];

    setSize ((int) std::lround ((float) baseEditorWidth * scale),
             (int) std::lround ((float) baseEditorHeight * scale));
}

void SilentiumAudioProcessorEditor::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colours::black);

    const auto scale = scaleSteps[(size_t) scaleStepIndex];

    // The top strip is an integrated dark header band (matching the
    // near-black plate) with a thin warm gold rule under it, not raw black
    // behind floating default-grey buttons - the preset bar's brass buttons
    // and the recessed name display (BasilicaLookAndFeel) sit on this band.
    const auto stripHeight = (float) topStripHeight1x * scale;
    g.setGradientFill (juce::ColourGradient (juce::Colour (0xff17141a), 0.0f, 0.0f,
                                             juce::Colour (0xff0b090d), 0.0f, stripHeight, false));
    g.fillRect (juce::Rectangle<float> (0.0f, 0.0f, (float) getWidth(), stripHeight));
    g.setColour (juce::Colour (0xff5a4420));
    g.fillRect (juce::Rectangle<float> (0.0f, stripHeight - 1.0f * scale, (float) getWidth(), 1.0f * scale));

    const auto plateBounds = juce::Rectangle<float> (0.0f, (float) topStripHeight1x * scale + (float) topStripGap1x * scale,
                                                      (float) plateWidth1x * scale, (float) plateHeight1x * scale);

    // v0.3.2 (this revision): the ENTIRE plate - obsidian plate, both VU
    // dials, tube-vent grilles, all 9 knobs, both toggles, and the rose
    // emblem - is a single photoreal master render
    // (resources/gui/faceplate-silentium-v3.png), drawn scaled-to-fit here.
    // Replaces every previous JUCE-drawn plate layer (glossy-black base
    // gradient, upper-left softbox reflection, hairline bevel, header
    // roundel/brand icon) - all of that is now baked into the art itself,
    // and painting a synthetic gradient/shadow/bevel ON TOP of a photoreal
    // render is exactly the "Frankenstein" mismatch Yves rejected.
    // RectanglePlacement::centred preserves the master's own aspect ratio
    // and letterboxes rather than stretching if plateBounds' aspect ever
    // drifts from the master's (it does not in the current @1x table -
    // plateHeight1x was chosen to match masterCanvasHeightPx / masterCanvasWidthPx
    // exactly - this is purely defensive for future edits).
    if (faceplateImage.isValid())
        g.drawImage (faceplateImage, plateBounds, juce::RectanglePlacement::centred, false);
}

void SilentiumAudioProcessorEditor::resized()
{
    const auto scale = scaleSteps[(size_t) scaleStepIndex];
    const auto s = [scale] (int v) { return (int) std::lround ((float) v * scale); };

    auto bounds = getLocalBounds();
    auto topStrip = bounds.removeFromTop (s (topStripHeight1x));

    scaleButton.setBounds (topStrip.removeFromRight (s (scaleButtonWidth1x)).reduced (0, s (2)));
    presetBar.setBounds (topStrip.reduced (0, s (2)));

    // Everything below is expressed in plate-local coordinates (the base
    // @1x table in PluginEditorLayout.h), then offset by the top strip +
    // gap and scaled.
    //
    // Each AnalogMeter's bounds are a square CENTRED EXACTLY ON ITS PIVOT
    // (the baked hub rivet in the master render) - see
    // PluginEditorLayout.h's meterHalfSize1x/meterLPivot1x/meterRPivot1x and
    // AnalogMeter.h's "meter_component_convention" docs. This is what makes
    // the needle's pivot fraction always (0.5, 0.5) regardless of which
    // meter.
    const auto toPlatePoint = [&] (juce::Point<int> plateLocal)
    {
        return juce::Point<int> (s (plateLocal.x),
                                 s (topStripHeight1x + topStripGap1x) + s (plateLocal.y));
    };

    const auto meterHalf = s (meterHalfSize1x);
    gainReductionMeter.setBounds (juce::Rectangle<int> (meterHalf * 2, meterHalf * 2).withCentre (toPlatePoint (meterLPivot1x)));
    inputLevelMeter.setBounds (juce::Rectangle<int> (meterHalf * 2, meterHalf * 2).withCentre (toPlatePoint (meterRPivot1x)));

    // Knobs: explicit STAGGERED centres baked into the master render (row 2
    // offset ~half a cell right of row 1) - see PluginEditorLayout.h's
    // knobRow1X1x/knobRow2X1x. The interactive FilmstripKnob is sized
    // identically to the baked knob's own diameter (knobDiameter1x,
    // measured from faceplate-metadata.json) so it exactly covers the baked
    // art underneath with no visible seam/mismatch.
    const auto knobDiam = s (knobDiameter1x);

    for (size_t i = 0; i < knobLayout.size(); ++i)
    {
        auto& entry = knobLayout[i];
        knobs[i].slider->setBounds (juce::Rectangle<int> (knobDiam, knobDiam)
                                        .withCentre (toPlatePoint ({ entry.centreX1x, entry.centreY1x })));
    }

    // Two footer toggles (Duck, Listen), explicit centres, sized identically
    // to the baked housing (toggleSize1x).
    const auto toggleSize = s (toggleSize1x);

    for (size_t i = 0; i < toggleLayout.size(); ++i)
    {
        toggles[i].button->setBounds (juce::Rectangle<int> (toggleSize, toggleSize)
                                          .withCentre (toPlatePoint ({ toggleX1x[i], toggleY1x })));
    }
}

void SilentiumAudioProcessorEditor::timerCallback()
{
    gainReductionMeter.setTargetDb (audioProcessor.getGainReductionDb());
    inputLevelMeter.setTargetDb (audioProcessor.getInputLevelDb());
}
