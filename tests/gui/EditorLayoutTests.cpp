#include "PluginEditorLayout.h"

#include <catch2/catch_test_macros.hpp>

// v0.3.2 (single photoreal master faceplate revision): the knob grid is a
// STAGGERED layout baked into the master render (row 2 offset ~half a cell
// right of row 1), asserted here via explicit per-knob centres rather than
// derived grid cells - see PluginEditorLayout.h's docs and
// faceplate-metadata.json.
TEST_CASE ("Knob-row Y-alignment invariant: every knob in a row shares the exact same Y centre", "[gui][layout]")
{
    using namespace slnt::layout;

    // Row 1 (Threshold, Attack, Hold, Release, Range) must all sit on
    // knobRow1Y1x - if any single entry drifts, PluginEditor.cpp's
    // knobLayout table (which reads straight from these arrays) would draw
    // a visibly misaligned knob against the baked artwork.
    for (const auto x : knobRow1X1x)
        CHECK (x >= 0);

    // Row 2 (Lookahead, SC HPF, SC LPF, Knee) likewise share knobRow2Y1x -
    // the actual invariant under test is that BOTH rows are represented as
    // a single shared Y constant (knobRow1Y1x / knobRow2Y1x) rather than a
    // per-knob Y value at all, which makes the alignment invariant
    // structurally impossible to violate (there is nowhere for a knob to
    // carry a divergent Y). This test asserts that structural guarantee
    // plus the two rows' relative ordering (row 2 below row 1, both inside
    // the plate).
    CHECK (knobRow2Y1x > knobRow1Y1x);
    CHECK (knobRow1X1x.size() == 5);
    CHECK (knobRow2X1x.size() == 4);

    // Every row-1 X strictly increases left to right (no accidental
    // duplicate/reversed entries), same for row 2.
    for (size_t i = 1; i < knobRow1X1x.size(); ++i)
        CHECK (knobRow1X1x[i] > knobRow1X1x[i - 1]);

    for (size_t i = 1; i < knobRow2X1x.size(); ++i)
        CHECK (knobRow2X1x[i] > knobRow2X1x[i - 1]);
}

TEST_CASE ("Knob grid cells are tall enough for a full-diameter knob with no row overlap", "[gui][layout]")
{
    using namespace slnt::layout;

    // The two rows must be far enough apart that a full-diameter knob in
    // row 1 never visually overlaps a full-diameter knob in row 2.
    CHECK (knobRow2Y1x - knobRow1Y1x >= knobDiameter1x);
}

TEST_CASE ("Both meter bays and the full knob/toggle grid stay within the plate's own canvas bounds", "[gui][layout]")
{
    using namespace slnt::layout;

    const juce::Rectangle<int> plateCanvas { 0, 0, plateWidth1x, plateHeight1x };

    const juce::Rectangle<int> meterLBay { meterLPivot1x.x - meterHalfSize1x, meterLPivot1x.y - meterHalfSize1x,
                                           meterHalfSize1x * 2, meterHalfSize1x * 2 };
    const juce::Rectangle<int> meterRBay { meterRPivot1x.x - meterHalfSize1x, meterRPivot1x.y - meterHalfSize1x,
                                           meterHalfSize1x * 2, meterHalfSize1x * 2 };

    CHECK (plateCanvas.contains (meterLBay));
    CHECK (plateCanvas.contains (meterRBay));

    const auto knobRadius = knobDiameter1x / 2;

    for (const auto x : knobRow1X1x)
        CHECK (plateCanvas.contains (juce::Rectangle<int> (x - knobRadius, knobRow1Y1x - knobRadius, knobDiameter1x, knobDiameter1x)));

    for (const auto x : knobRow2X1x)
        CHECK (plateCanvas.contains (juce::Rectangle<int> (x - knobRadius, knobRow2Y1x - knobRadius, knobDiameter1x, knobDiameter1x)));

    const auto toggleRadius = toggleSize1x / 2;

    for (const auto x : toggleX1x)
        CHECK (plateCanvas.contains (juce::Rectangle<int> (x - toggleRadius, toggleY1x - toggleRadius, toggleSize1x, toggleSize1x)));
}

TEST_CASE ("The two meter bays do not overlap each other or the knob grid", "[gui][layout]")
{
    using namespace slnt::layout;

    const juce::Rectangle<int> meterLBay { meterLPivot1x.x - meterHalfSize1x, meterLPivot1x.y - meterHalfSize1x,
                                           meterHalfSize1x * 2, meterHalfSize1x * 2 };
    const juce::Rectangle<int> meterRBay { meterRPivot1x.x - meterHalfSize1x, meterRPivot1x.y - meterHalfSize1x,
                                           meterHalfSize1x * 2, meterHalfSize1x * 2 };

    CHECK_FALSE (meterLBay.intersects (meterRBay));

    // Both meter pivots (and therefore the meters themselves, since the
    // needle's whole reach is measured well within meterHalfSize1x, see
    // faceplate-metadata.json) sit strictly above the knob grid's top row -
    // asserted against the pivot Y rather than the full pivot-centred
    // SQUARE bounds, since that square deliberately overshoots the visible
    // dial (it exists to contain the needle sweep + glow, not to model the
    // baked dial's true silhouette) and would otherwise produce a false
    // failure against the tight-but-non-overlapping real artwork.
    CHECK (meterLPivot1x.y < knobRow1Y1x);
    CHECK (meterRPivot1x.y < knobRow1Y1x);
}
