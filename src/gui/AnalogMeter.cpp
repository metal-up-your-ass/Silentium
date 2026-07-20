#include "AnalogMeter.h"

#include <cmath>

namespace
{
    // Copied verbatim from
    // .scaffold/gui-assets/faceplate-silentium-v3/faceplate-metadata.json's
    // per-meter "dB_angle_table_deg" (both meters share this same relative
    // table - see that file's "_provenance" notes: measured directly on
    // master-03-raw.png via polar-unwrap + fine pixel-grid cross-validation
    // for the -20dB/0dB anchors, with the -20 and 0 tick radii from the
    // pivot matching to within 0.1px as the confidence check; the remaining
    // seven ticks are proportionally rescaled from the previous vu-nano-v1
    // asset's reference table, scale factor 0.800, since a clean independent
    // re-measurement of every intermediate tick was not achievable at this
    // render's resolution). NOT interchangeable with vu-nano-v1's old table
    // - this master render's arc sits at measurably different angles.
    struct Tick
    {
        float db;
        float deg;
    };

    constexpr std::array<Tick, 9> ticks {
        Tick { -20.0f, -26.80f }, Tick { -10.0f, -15.40f }, Tick { -7.0f, -6.29f },
        Tick { -5.0f, 1.58f }, Tick { -3.0f, 9.31f }, Tick { 0.0f, 18.02f },
        Tick { 1.0f, 25.47f }, Tick { 2.0f, 32.92f }, Tick { 3.0f, 40.39f }
    };
}

namespace basilica::gui
{
    AnalogMeter::AnalogMeter (Assets assetsIn, juce::String accessibleTitle, float flickerSeedIn)
        : assets (std::move (assetsIn)), title (std::move (accessibleTitle)), flickerPhaseSeed (flickerSeedIn)
    {
        setTitle (title);
        setDescription (title);

        // Pure display - never steals mouse events from controls that may
        // sit under this component's (partly transparent) bounds.
        setInterceptsMouseClicks (false, false);

        // Both layers this component still draws (the incandescent glow
        // gradient and the needle) are cheap to re-rasterise every frame at
        // this component's small on-screen size, and the glow's flicker
        // needs to repaint continuously - a cached StandardCachedComponentImage
        // (setBufferedToImage) would just be invalidated on nearly every
        // timer tick anyway, so it is not used here (unlike the old
        // face+needle version, which had a large static face layer worth
        // caching).
        startTimeSeconds = juce::Time::getMillisecondCounterHiRes() / 1000.0;

        startTimerHz ((int) timerHz);
    }

    AnalogMeter::~AnalogMeter()
    {
        stopTimer();
    }

    float AnalogMeter::tickAngleDegreesForDb (float db) noexcept
    {
        if (db <= ticks.front().db)
            return ticks.front().deg;

        if (db >= ticks.back().db)
            return ticks.back().deg;

        for (size_t i = 1; i < ticks.size(); ++i)
        {
            if (db <= ticks[i].db)
            {
                const auto& lo = ticks[i - 1];
                const auto& hi = ticks[i];
                const auto span = hi.db - lo.db;
                const auto t = span > 0.0f ? (db - lo.db) / span : 0.0f;
                return lo.deg + t * (hi.deg - lo.deg);
            }
        }

        return ticks.back().deg;
    }

    float AnalogMeter::stepBallistics (float currentSmoothed, float target, float dtSeconds, float tauSeconds) noexcept
    {
        if (tauSeconds <= 0.0f || dtSeconds <= 0.0f)
            return target;

        const auto alpha = 1.0f - std::exp (-dtSeconds / tauSeconds);
        return currentSmoothed + (target - currentSmoothed) * alpha;
    }

    float AnalogMeter::currentFlickerMultiplier() const noexcept
    {
        const auto now = juce::Time::getMillisecondCounterHiRes() / 1000.0;
        const auto t = (float) (now - startTimeSeconds);

        float sum = 0.0f;

        for (size_t i = 0; i < flickerFrequenciesHz.size(); ++i)
        {
            const auto phase = flickerPhaseSeed * 3.7f + (float) i * 2.1f;
            sum += flickerWeights[i] * std::sin (juce::MathConstants<float>::twoPi * flickerFrequenciesHz[i] * t + phase);
        }

        // flickerWeights sum to 1.0, so sum is already normalised to [-1, 1].
        return 1.0f + flickerAmplitudeFraction * sum;
    }

    void AnalogMeter::timerCallback()
    {
        const auto target = targetDb.load (std::memory_order_relaxed);
        const auto next = stepBallistics (smoothedDb, target, 1.0f / (float) timerHz, ballisticsTauSeconds);

        const auto dbChanged = ! juce::approximatelyEqual (next, smoothedDb);
        smoothedDb = next;

        // The incandescent glow's flicker needs continuous repaints even
        // when the dB reading is stable - skip entirely when not on screen
        // (host bypass / hidden window), per Yves' brief, rather than
        // spending repaint cycles on an invisible component.
        if (isShowing())
            repaint();
        else if (dbChanged)
            repaint();
    }

    void AnalogMeter::paint (juce::Graphics& g)
    {
        const auto bounds = getLocalBounds().toFloat();

        g.setImageResamplingQuality (juce::Graphics::highResamplingQuality);

        // Optional face draw - skipped when invalid (Silentium's usage
        // always leaves this default, see Assets' docs), kept only so this
        // reusable component still works stand-alone/in tests without a
        // baked background behind it.
        if (assets.face.isValid())
            g.drawImage (assets.face, bounds);

        const auto pivotX = bounds.getWidth() * pivotXFraction;
        const auto pivotY = bounds.getHeight() * pivotYFraction;
        const auto halfSize = 0.5f * juce::jmin (bounds.getWidth(), bounds.getHeight());

        // Incandescent pilot-lamp glow - drawn UNDER the needle, matching a
        // grain-of-wheat pilot lamp sitting behind the dial just above the
        // hub. Flicker gently modulates both alpha stops in lockstep (a
        // single scalar multiplier keeps the two-stop gradient's relative
        // shape constant while its overall brightness breathes).
        {
            const auto flicker = currentFlickerMultiplier();
            const auto glowCx = pivotX;
            const auto glowCy = pivotY + glowCentreOffsetYFraction * halfSize;
            const auto glowRadius = glowRadiusFraction * halfSize;

            juce::ColourGradient glowGradient (
                juce::Colour::fromRGB (255, 200, 120).withAlpha (juce::jlimit (0.0f, 1.0f, glowAlphaCentre * flicker)),
                glowCx, glowCy,
                juce::Colours::transparentBlack,
                glowCx, glowCy + glowRadius,
                true);
            glowGradient.addColour (0.5, juce::Colour::fromRGB (255, 170, 90)
                                             .withAlpha (juce::jlimit (0.0f, 1.0f, glowAlphaMid * flicker)));

            g.setGradientFill (glowGradient);
            g.fillRect (bounds);
        }

        if (assets.needle.isValid())
        {
            const auto sx = bounds.getWidth() / (float) assets.needle.getWidth();
            const auto sy = bounds.getHeight() / (float) assets.needle.getHeight();

            // The needle asset is rendered at rest pointing straight up
            // (0 deg) with its own pivot dead-centre on its square canvas -
            // the measured tick angle IS the absolute rotation, no
            // rest-angle delta to subtract.
            const auto targetDeg = tickAngleDegreesForDb (smoothedDb);
            const auto radians = juce::degreesToRadians (targetDeg);

            const auto transform = juce::AffineTransform::scale (sx, sy)
                                        .rotated (radians, pivotX, pivotY);

            g.drawImageTransformed (assets.needle, transform);
        }
    }

    // A-07 fix (M3 a11y review): a read-only text value interface exposing
    // the current ballistic-smoothed reading, mirroring the shape of JUCE's
    // own ButtonValueInterface pattern (juce_ButtonAccessibilityHandler.h).
    // Reads the SAME smoothedDb the paint() method just drew, updated at
    // this component's own 30 Hz timer - queried on demand by AT clients,
    // never pushed/announced proactively.
    class AnalogMeter::MeterValueInterface final : public juce::AccessibilityTextValueInterface
    {
    public:
        explicit MeterValueInterface (const AnalogMeter& ownerIn) noexcept : owner (ownerIn) {}

        bool isReadOnly() const override { return true; }

        juce::String getCurrentValueAsString() const override
        {
            return juce::String (owner.smoothedDb, 1) + " dB";
        }

        // Read-only: assistive-technology clients cannot set a meter
        // reading, so this is intentionally a no-op rather than throwing or
        // asserting - matches isReadOnly() == true's documented contract.
        void setValueAsString (const juce::String&) override {}

    private:
        const AnalogMeter& owner;
    };

    std::unique_ptr<juce::AccessibilityHandler> AnalogMeter::createAccessibilityHandler()
    {
        // Read-only display, not an interactive control - AccessibilityRole::
        // label (rather than Component's default ::unspecified) is the
        // closer semantic match for a screen reader. The value interface
        // (A-07 fix) lets AT clients query the current reading on demand
        // without this component ever pushing unsolicited announcements.
        return std::make_unique<juce::AccessibilityHandler> (
            *this,
            juce::AccessibilityRole::label,
            juce::AccessibilityActions {},
            juce::AccessibilityHandler::Interfaces { std::make_unique<MeterValueInterface> (*this) });
    }
}
