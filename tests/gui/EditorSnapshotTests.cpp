#include "PluginEditor.h"
#include "PluginEditorLayout.h"
#include "PluginProcessor.h"
#include "gui/AnalogMeter.h"

#include <catch2/catch_test_macros.hpp>

#include <array>
#include <cmath>
#include <iostream>
#include <vector>

// GUI smoke tests for the master-05 baseline editor (src/PluginEditor.h,
// src/gui/). juce::ScopedJuceInitialiser_GUI is installed once for the
// whole test binary in tests/TestMain.cpp, so Components/Timers are safe to
// construct here even though this is a headless console executable with no
// running message loop (timers simply never fire, which is fine - these
// tests only exercise synchronous construction/paint/destruction).
TEST_CASE ("Editor constructs, lays out, and destroys cleanly", "[gui]")
{
    SilentiumAudioProcessor processor;
    processor.prepareToPlay (48000.0, 512);

    {
        SilentiumAudioProcessorEditor editor (processor);

        CHECK (editor.getWidth() > 0);
        CHECK (editor.getHeight() > 0);
    }
    // editor destroyed here - JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR
    // (used throughout src/gui/ and on the editor itself) asserts at process
    // exit in Debug builds if any tagged instance was ever leaked, so a
    // clean run of this whole test binary is itself the leak check.
}

namespace
{
    // Local copy of EditorAccessibilityTests.cpp's findChildByTitle helper -
    // deliberately not shared/exported between test files (each Catch2
    // .cpp here is meant to be readable standalone), see that file's own
    // docs for why a flat, non-recursive scan is sufficient (every control
    // this touches is a direct child of the editor, never nested in a
    // sub-container).
    template <typename ComponentType>
    ComponentType* findChildByTitle (juce::Component& parent, const juce::String& title)
    {
        for (int i = 0; i < parent.getNumChildComponents(); ++i)
            if (auto* typed = dynamic_cast<ComponentType*> (parent.getChildComponent (i)))
                if (typed->getTitle() == title)
                    return typed;

        return nullptr;
    }

    // Configures a deliberately "alive-looking" state before snapshotting -
    // Yves' explicit request for the master-05 baseline revision: Duck OFF
    // (exercises the master-06 toggle-zone crop-swap overlay), Listen ON
    // (stays at master-05's own baked UP artwork), the two VU needles at
    // different, non-zero positions with the Input meter's peak LED lit,
    // the vent-glow mix partway between master-glow-dim and master-05's own
    // ceiling, and the knob grid at varied positions rather than every
    // control sitting at a uniform 12 o'clock rest pose (the knobs
    // themselves never visibly rotate - see PluginEditor.h's docs - but
    // exercising real, varied APVTS values here is still worthwhile: it's
    // what the accessible value-popup text and the SliderAttachment wiring
    // actually reflect).
    //
    // AnalogMeter::setTargetDb() + this component's own ~300ms ballistic
    // ramp, and the editor's own timerCallback()-driven vent-glow ballistics,
    // would need real timer ticks pumped through a running message loop to
    // actually reach these values - this headless test binary has no such
    // loop (see the top-of-file docs), so setImmediateDbForPreview() /
    // setVentGlowMixForPreview() (test/preview-only, see AnalogMeter.h's and
    // PluginEditor.h's docs) seed the ballistic-smoothed readings, the
    // peak-LED state, and the vent-glow mix directly instead.
    void configureLiveLookingState (SilentiumAudioProcessorEditor& editor)
    {
        if (auto* gr = findChildByTitle<basilica::gui::AnalogMeter> (editor, "Gain Reduction meter"))
            gr->setImmediateDbForPreview (-7.0f);

        if (auto* input = findChildByTitle<basilica::gui::AnalogMeter> (editor, "Input Level meter"))
            input->setImmediateDbForPreview (2.0f); // >= AnalogMeter::peakLedThresholdDb - lights the peak LED

        if (auto* duck = findChildByTitle<juce::ToggleButton> (editor, "Duck"))
            duck->setToggleState (false, juce::dontSendNotification); // OFF -> master-06 crop at this toggle

        if (auto* listen = findChildByTitle<juce::ToggleButton> (editor, "Listen"))
            listen->setToggleState (true, juce::dontSendNotification); // ON -> master-05's own baked UP artwork

        editor.setVentGlowMixForPreview (0.6f); // between master-glow-dim and master-05's own ceiling

        struct KnobValue
        {
            const char* label;
            double normalisedValue; // 0..1 proportion of the slider's own range, deliberately varied (not all 0.5)
        };

        const KnobValue knobValues[] = {
            { "Threshold", 0.30 }, { "Attack", 0.70 }, { "Hold", 0.15 },
            { "Release", 0.55 }, { "Range", 0.85 }, { "Lookahead", 0.40 },
            { "SC HPF", 0.60 }, { "SC LPF", 0.25 }, { "Knee", 0.75 },
        };

        for (const auto& kv : knobValues)
            if (auto* knob = findChildByTitle<juce::Slider> (editor, kv.label))
                knob->setValue (knob->proportionOfLengthToValue (kv.normalisedValue), juce::dontSendNotification);
    }
}

TEST_CASE ("Editor snapshot at 100% is non-blank and is written for PR review", "[gui]")
{
    SilentiumAudioProcessor processor;
    processor.prepareToPlay (48000.0, 512);

    SilentiumAudioProcessorEditor editor (processor);
    REQUIRE (editor.getWidth() > 0);
    REQUIRE (editor.getHeight() > 0);

    configureLiveLookingState (editor);

    // SoftwareImageType (rather than the default NativeImageType) avoids any
    // dependency on an actual native graphics context/window, which keeps
    // this robust on headless CI runners.
    const auto snapshot = editor.createComponentSnapshot (editor.getLocalBounds(), true, 1.0f, juce::SoftwareImageType {});

    REQUIRE (snapshot.isValid());
    CHECK (snapshot.getWidth() == editor.getWidth());
    CHECK (snapshot.getHeight() == editor.getHeight());

    // Non-blank: sample a small grid of points and confirm they are not all
    // identical to the top-left corner - a completely blank/solid-fill
    // render (e.g. every asset failing to decode) would fail this.
    const auto reference = snapshot.getPixelAt (0, 0);
    bool foundDifference = false;

    for (int y = 0; y < snapshot.getHeight() && ! foundDifference; y += juce::jmax (1, snapshot.getHeight() / 20))
        for (int x = 0; x < snapshot.getWidth() && ! foundDifference; x += juce::jmax (1, snapshot.getWidth() / 20))
            if (snapshot.getPixelAt (x, y) != reference)
                foundDifference = true;

    CHECK (foundDifference);

    // Written for local/PR review (see docs/gui-preview.png, a committed
    // static copy of a run of this test) - path is relative to the test
    // binary's current working directory, which `ctest --test-dir build`
    // sets to the build directory, landing this at build/gui-preview.png.
    juce::PNGImageFormat pngFormat;
    const auto outFile = juce::File::getCurrentWorkingDirectory().getChildFile ("gui-preview.png");

    if (auto stream = std::unique_ptr<juce::FileOutputStream> (outFile.createOutputStream()))
    {
        stream->setPosition (0);
        stream->truncate();
        CHECK (pngFormat.writeImageToStream (snapshot, *stream));
    }
}

// Motion proof, from the REAL app render pipeline (not the offline
// needle_diff analysis scripts): sweeps the Input Level meter's needle
// through {-20,-7,-5,0,+2} dB, captures a real editor snapshot at each
// position, crops to that meter's own component bounds, and checks that the
// per-pixel range across the 5 crops (the "needle fan") stays essentially
// zero inside a small disc around PluginEditorLayout.h's meterRPivotX/
// YFraction - i.e. the needle's base doesn't wobble as it sweeps, which is
// exactly the defect the 2026-07-23 per-meter pivot fix (see that file's
// docs) was meant to close. writePngProof() below is a static-write helper
// (no threads/timers), so this stays a synchronous, headless test like the
// rest of this file.
TEST_CASE ("Input meter needle fan pivots cleanly on its own true hub across a dB sweep", "[gui]")
{
    SilentiumAudioProcessor processor;
    processor.prepareToPlay (48000.0, 512);

    SilentiumAudioProcessorEditor editor (processor);
    REQUIRE (editor.getWidth() > 0);
    REQUIRE (editor.getHeight() > 0);

    auto* input = findChildByTitle<basilica::gui::AnalogMeter> (editor, "Input Level meter");
    REQUIRE (input != nullptr);

    constexpr std::array<float, 5> sweepDb { -20.0f, -7.0f, -5.0f, 0.0f, 2.0f };

    std::vector<juce::Image> crops;
    crops.reserve (sweepDb.size());

    for (const auto db : sweepDb)
    {
        input->setImmediateDbForPreview (db);

        const auto snapshot = editor.createComponentSnapshot (editor.getLocalBounds(), true, 1.0f, juce::SoftwareImageType {});
        REQUIRE (snapshot.isValid());

        // input->getBounds() is this meter's own bounds within the editor -
        // exactly the region the snapshot above was rendered into, so this
        // crop lines up pixel-for-pixel with the component's own coordinate
        // space (which is what PluginEditorLayout.h's meterRPivotXFraction/
        // meterRPivotYFraction are expressed as fractions of).
        crops.push_back (snapshot.getClippedImage (input->getBounds()));
    }

    REQUIRE (crops.size() == sweepDb.size());
    for (const auto& crop : crops)
    {
        REQUIRE (crop.isValid());
        REQUIRE (crop.getWidth() == crops.front().getWidth());
        REQUIRE (crop.getHeight() == crops.front().getHeight());
    }

    const auto cropWidth = crops.front().getWidth();
    const auto cropHeight = crops.front().getHeight();

    // --- (a) build/needle-sweep.png: the 5 crops side by side ---
    constexpr int gap = 10;
    juce::Image sweepImage (juce::Image::RGB, (int) (cropWidth * (int) crops.size() + gap * ((int) crops.size() + 1)),
                             cropHeight + gap * 2, true);
    {
        juce::Graphics g (sweepImage);
        g.fillAll (juce::Colours::black);
        for (size_t i = 0; i < crops.size(); ++i)
            g.drawImageAt (crops[i], gap + (int) i * (cropWidth + gap), gap);
    }

    // --- (b) build/needle-pivot-overlay.png: per-pixel MAX-DIFFERENCE image
    // across the 5 crops (the needle fan) - value at each pixel is the
    // largest per-channel range (max - min across the 5 frames), summed over
    // R/G/B, i.e. exactly zero wherever the pixel never changes across the
    // whole sweep (the static face/bezel/hub) and large wherever the needle
    // blade sweeps through. ---
    juce::Image diffImage (juce::Image::RGB, cropWidth, cropHeight, true);

    // PluginEditorLayout.h's per-meter pivot fraction for THIS (right/input)
    // dial, expressed in this crop's own local pixel space (crop size ==
    // meterComponentSize1x at the editor's default 100% scale step, see
    // PluginEditor.cpp's resized()/scaleStepIndex docs).
    const auto pivotXLocal = slnt::layout::meterRPivotXFraction * (float) slnt::layout::meterComponentSize1x;
    const auto pivotYLocal = slnt::layout::meterRPivotYFraction * (float) slnt::layout::meterComponentSize1x;

    // v0.3.7 (through-pivot rod + hub occluder): the rod now legitimately
    // sweeps VISIBLY through the recess annulus around the hub (exactly as
    // master-03 bakes it), so the old "everything near the hub is static"
    // radius (29.2px, the shadow-disc ring) no longer holds. What IS
    // guaranteed static now is the hub-OCCLUDER cap disc that
    // AnalogMeter::paint() draws on top of the needle frame: 21 master px
    // (AnalogMeter.cpp's occluder geometry) * plateWidth1x/
    // masterCanvasWidthPx (900/1264) ~= 14.95px @1x - probed at 13px to
    // stay safely inside the occluder's own ~1.3px alpha feather + edge AA.
    constexpr float hubCapRadius1x = 13.0f;

    double totalDiffEnergy = 0.0;
    double diskDiffEnergy = 0.0;

    {
        juce::Image::BitmapData diffWrite (diffImage, juce::Image::BitmapData::writeOnly);

        for (int y = 0; y < cropHeight; ++y)
        {
            for (int x = 0; x < cropWidth; ++x)
            {
                float minR = 255.0f, maxR = 0.0f;
                float minG = 255.0f, maxG = 0.0f;
                float minB = 255.0f, maxB = 0.0f;

                for (const auto& crop : crops)
                {
                    const auto c = crop.getPixelAt (x, y);
                    minR = juce::jmin (minR, (float) c.getRed());
                    maxR = juce::jmax (maxR, (float) c.getRed());
                    minG = juce::jmin (minG, (float) c.getGreen());
                    maxG = juce::jmax (maxG, (float) c.getGreen());
                    minB = juce::jmin (minB, (float) c.getBlue());
                    maxB = juce::jmax (maxB, (float) c.getBlue());
                }

                const float diff = (maxR - minR) + (maxG - minG) + (maxB - minB); // 0..765
                const auto vis = (juce::uint8) juce::jlimit (0, 255, (int) std::lround (diff));
                diffWrite.setPixelColour (x, y, juce::Colour::fromRGB (vis, vis, vis));

                totalDiffEnergy += (double) diff;

                const auto dx = (float) x - pivotXLocal;
                const auto dy = (float) y - pivotYLocal;
                if (dx * dx + dy * dy <= hubCapRadius1x * hubCapRadius1x)
                    diskDiffEnergy += (double) diff;
            }
        }
    }

    const auto diskEnergyFraction = totalDiffEnergy > 0.0 ? diskDiffEnergy / totalDiffEnergy : 0.0;

    std::cout << "[needle-fan] hub-disc pivot @ (" << pivotXLocal << ", " << pivotYLocal << ") r=" << hubCapRadius1x
               << "px; disk/total diff energy = " << diskDiffEnergy << "/" << totalDiffEnergy << " = "
               << (diskEnergyFraction * 100.0) << "%\n";

    INFO ("hub-disc pivot @ (" << pivotXLocal << ", " << pivotYLocal << ") r=" << hubCapRadius1x
                                << "; disk/total diff energy = " << diskDiffEnergy << "/" << totalDiffEnergy
                                << " = " << (diskEnergyFraction * 100.0) << "%");

    // v0.3.7: inside the hub-occluder cap disc the pixels are the OCCLUDER's
    // (drawn after the needle frame), so the sweep difference there must be
    // essentially zero - this guards the occluder's presence AND placement:
    // a missing or misplaced occluder would let the rod's bridge span sweep
    // visibly right at the pivot (the "disconnected needle" defect family
    // this whole revision closes), and a mispositioned pivot would smear
    // difference energy into the disc the same way. The only non-zero
    // residue inside the probe disc is edge AA where the disc's boundary
    // pixels graze the occluder feather.
    //
    // Threshold calibration (measured locally, this revision): correct
    // occluder = exactly 0% (the opaque occluder core fully covers the
    // 13px probe disc). 0.5% keeps a wide CI-stable margin for renderer
    // AA differences while still failing hard on a missing/misplaced
    // occluder or pivot regression.
    CHECK (totalDiffEnergy > 0.0); // sanity: the needle actually moved
    CHECK (diskEnergyFraction < 0.005);

    juce::PNGImageFormat pngFormat;

    const auto writePng = [&pngFormat] (const juce::Image& image, const char* filename)
    {
        const auto outFile = juce::File::getCurrentWorkingDirectory().getChildFile (filename);
        if (auto stream = std::unique_ptr<juce::FileOutputStream> (outFile.createOutputStream()))
        {
            stream->setPosition (0);
            stream->truncate();
            CHECK (pngFormat.writeImageToStream (image, *stream));
        }
        else
        {
            FAIL ("could not open output stream for " << filename);
        }
    };

    // Written for local/PR review, same convention as gui-preview.png above
    // - paths are relative to the test binary's cwd, which `ctest
    // --test-dir build` sets to the build directory (build/needle-sweep.png,
    // build/needle-pivot-overlay.png).
    writePng (sweepImage, "needle-sweep.png");
    writePng (diffImage, "needle-pivot-overlay.png");
}
