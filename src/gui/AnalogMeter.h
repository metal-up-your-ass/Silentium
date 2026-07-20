#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include <array>
#include <atomic>

// Suite-reusable analog-style meter overlay: a rotating needle drawn on top
// of the SINGLE photoreal master faceplate (see
// .scaffold/gui-assets/faceplate-silentium-v3/) rather than owning its own
// face image. The master render already bakes the dial face, ticks, "VU"
// wordmark, hub, and anchor bar for both meters - this component's job is
// only the two things that must be LIVE: the rotating needle and a subtle
// incandescent pilot-lamp glow behind it (see paint()).
//
// v0.3.2 (this revision): replaces the earlier "static face image + rotating
// needle image" pair (vu-nano-v1) now that the face is baked into the shared
// background. The component's bounds are ALWAYS a square centred exactly on
// the meter's pivot (the brass hub the needle rotates around in the master
// render, see faceplate-metadata.json's "meter_component_convention") - so
// the needle's pivot fraction within this component is always (0.5, 0.5),
// unlike the old per-asset-measured pivotXFraction/pivotYFraction. The
// needle asset itself (vu-needle-master-v3.png) is likewise authored on a
// square canvas with its own pivot dead-centre, rendered at rest pointing
// STRAIGHT UP (0 deg / 12 o'clock) - JUCE applies the measured dB->angle
// value directly as the rotation, no rest-angle subtraction.
namespace basilica::gui
{
    class AnalogMeter : public juce::Component, private juce::Timer
    {
    public:
        struct Assets
        {
            // Optional: only set if a caller still wants this component to
            // draw its own face (kept for flexibility/testability - the
            // face draw is skipped entirely when invalid, see paint()).
            // Silentium's usage (PluginEditor.cpp) deliberately leaves this
            // default/invalid, since the dial face is baked into the shared
            // background image behind the whole plate.
            juce::Image face;
            juce::Image needle;
        };

        // flickerSeedIn: per-instance phase offset for the incandescent
        // glow's flicker (see paint()/timerCallback()) so that two meters
        // sharing the same class never flicker in lockstep - pass a
        // different value per instance (e.g. 0.0f / 1.0f).
        AnalogMeter (Assets assetsIn, juce::String accessibleTitle, float flickerSeedIn = 0.0f);
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
        // interpolated across the master render's own measured tick table
        // (see the .cpp - copied verbatim from
        // .scaffold/gui-assets/faceplate-silentium-v3/faceplate-metadata.json's
        // per-meter dB_angle_table_deg, both meters share the same relative
        // table per that file's provenance notes) and clamped beyond the
        // table's ends. Exposed for unit testing. Degrees are clockwise from
        // straight-up (12 o'clock) - this IS the needle's absolute rotation
        // angle (the needle asset's own rest pose is 0 deg / straight up).
        static float tickAngleDegreesForDb (float db) noexcept;

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
        float currentFlickerMultiplier() const noexcept;

        Assets assets;
        juce::String title;

        std::atomic<float> targetDb { -100.0f };
        float smoothedDb = -100.0f;

        // Needle/glow pivot as a fraction of this component's own bounds -
        // ALWAYS (0.5, 0.5) under the pivot-centred convention documented
        // above (both meters, unlike the old per-asset-measured fractions).
        static constexpr float pivotXFraction = 0.5f;
        static constexpr float pivotYFraction = 0.5f;

        // Incandescent pilot-lamp glow geometry (Yves' brief): centred
        // slightly ABOVE the pivot (offset expressed as a fraction of the
        // component half-size, matching faceplate-metadata.json's
        // glow_overlay block), radius as a fraction of the half-size,
        // tapering to fully transparent.
        static constexpr float glowCentreOffsetYFraction = -0.18f;
        static constexpr float glowRadiusFraction = 0.62f;
        static constexpr float glowAlphaCentre = 0.38f;
        static constexpr float glowAlphaMid = 0.16f;

        // Flicker: sum of three low-frequency sine layers at deliberately
        // non-harmonic frequencies (no common integer ratio) for an
        // irregular, non-periodic-feeling modulation rather than a smooth
        // metronomic pulse - amplitudeFraction is the peak deviation from
        // the base alpha values above (+-4%, within Yves' +-3-5% brief).
        // flickerPhaseSeed offsets each sine layer's phase per-instance so
        // two AnalogMeters never flicker in lockstep.
        float flickerPhaseSeed = 0.0f;
        double startTimeSeconds = 0.0;
        static constexpr float flickerAmplitudeFraction = 0.04f;
        static constexpr std::array<float, 3> flickerFrequenciesHz { 0.63f, 1.13f, 0.29f };
        static constexpr std::array<float, 3> flickerWeights { 0.5f, 0.3f, 0.2f };

        static constexpr double timerHz = 30.0;
        static constexpr float ballisticsTauSeconds = 0.3f;

        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (AnalogMeter)
    };
}
