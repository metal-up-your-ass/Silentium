#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include <array>
#include <atomic>

// Suite-reusable analog-style meter: a static face + glass overlay
// (pre-rendered Blender PNGs - since v0.3.1 the circular glass-dome family,
// see .scaffold/gui-assets/vu-dome-v1/README.md) with a needle image rotated
// live via juce::AffineTransform around the face's baked pivot rivet. The
// needle image is rendered once, at rest, pointing at the face's lowest
// scale tick - JUCE only ever applies an ADDITIONAL rotation delta on top of
// that baked rest orientation, so at the lowest tick the needle draws with
// zero extra rotation and lands exactly on the rivet mark the asset was
// authored against.
namespace basilica::gui
{
    class AnalogMeter : public juce::Component, private juce::Timer
    {
    public:
        struct Assets
        {
            juce::Image face1x, face2x;
            juce::Image needle1x, needle2x;
            juce::Image glass1x, glass2x;
        };

        AnalogMeter (Assets assetsIn, juce::String accessibleTitle);
        ~AnalogMeter() override;

        // Thread-safe (plain atomic store): the instantaneous value in dB,
        // written from the audio thread every processBlock() call. Ballistic
        // smoothing is applied separately, on the GUI thread's timer - never
        // here, so this is real-time safe to call from anywhere.
        void setTargetDb (float newTargetDb) noexcept { targetDb.store (newTargetDb, std::memory_order_relaxed); }

        void paint (juce::Graphics& g) override;
        std::unique_ptr<juce::AccessibilityHandler> createAccessibilityHandler() override;

        // One-pole ballistic integration step, exposed as a pure/static
        // function so it is directly unit-testable (see
        // tests/gui/AnalogMeterBallisticsTests.cpp's step-response test)
        // without needing a running juce::Timer/message loop. tauSeconds
        // ~0.3s approximates a real VU meter coil's mechanical inertia (see
        // the basilica-gui-design skill).
        static float stepBallistics (float currentSmoothed, float target, float dtSeconds, float tauSeconds) noexcept;

        // dB -> face-relative rotation angle in degrees, piecewise-linearly
        // interpolated across the asset's own baked tick table (see the .cpp
        // - copied verbatim from render_vu_meter.py's TICKS, the ground
        // truth for where the engraved arc's ticks actually sit) and clamped
        // beyond the table's ends. Exposed for unit testing.
        static float tickAngleDegreesForDb (float db) noexcept;

        // vu-dome-v1's visible content (bezel outer edge) spans 95% of the
        // rendered canvas (render_vu_dome_v1.py's BEZEL_OUTER_R = 0.95 under
        // ortho_scale 2.0), with a thin fully transparent margin around it.
        // A layout that wants the visible dial to fill a given rectangle
        // must size this component 1/contentFractionOfCanvas larger than
        // that rectangle, centred on it (the margin is transparent and this
        // component never intercepts mouse events, so the tiny overhang is
        // harmless). See SilentiumAudioProcessorEditor::resized(). v0.3.1:
        // was 0.5 for vu-brass-v1, whose dial only filled the central half
        // of its canvas - the ~2x runtime upscale that implied was a direct
        // cause of the rejected "unscharf" meter rendering.
        static constexpr float contentFractionOfCanvas = 0.95f;

    private:
        // A-07 fix (M3 a11y review): read-only accessibility value
        // interface, so AT users can query the current ballistic-smoothed
        // reading on demand (VoiceOver's "read value" gesture / NVDA's
        // report-value key) - see the .cpp for the implementation and
        // createAccessibilityHandler() below. Deliberately NOT wired to any
        // live/continuous announcement: this component's own 30 Hz repaint
        // timer must never trigger AT notifications, which would produce
        // constant chatter far worse than the previous silence.
        class MeterValueInterface;

        void timerCallback() override;
        const juce::Image& faceForCurrentWidth() const noexcept;
        const juce::Image& needleForCurrentWidth() const noexcept;
        const juce::Image& glassForCurrentWidth() const noexcept;

        Assets assets;
        juce::String title;

        std::atomic<float> targetDb { -100.0f };
        float smoothedDb = -100.0f;

        // Needle pivot as a fraction of the layer canvas, derived from
        // render_vu_dome_v1.py's own scene: PIVOT world (0, -0.42) on a
        // square canvas spanning world y [-1, 1] (ortho_scale 2.0) ->
        // fraction from the top = (1 + 0.42) / 2 = 0.71. The needle layer
        // is rendered at rest pointing at the lowest tick (-20 dB / -50deg);
        // JUCE only ever applies an ADDITIONAL rotation delta on top of
        // that baked rest orientation, around this pivot point.
        static constexpr float pivotXFraction = 0.5f;
        static constexpr float pivotYFraction = 0.71f;

        static constexpr double timerHz = 30.0;
        static constexpr float ballisticsTauSeconds = 0.3f;

        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (AnalogMeter)
    };
}
