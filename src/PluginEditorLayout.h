#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include <array>

// Silentium's own @1x faceplate/control-bay geometry table - lives in its
// own header, rather than as an anonymous-namespace block inside
// PluginEditor.cpp, so tests/gui/EditorLayoutTests.cpp can assert layout
// invariants directly against the SAME numbers PluginEditor.cpp actually
// lays components out with, instead of a second hand-copied set of
// constants that could silently drift out of sync.
//
// v0.3.4 (this revision): MASTER-05 BASELINE ARCHITECTURE, per Yves' final
// art direction - a full replacement of the three prior "component
// composition" attempts (v0.3.1's bare JUCE-drawn background, v0.3.2's
// single-master faceplate, v0.3.3's true component assembly). master-05.png
// (.scaffold/gui-assets/faceplate-silentium-v3/master-05.png, 1264x848) is
// now the SOLE baked faceplate: obsidian plate, brass bevel, 4 corner
// screws, rose flourish, both VU dial faces at rest (empty - no needle, no
// LED), all 9 knobs at 12 o'clock, the 2 toggles UP/on, and both tube-vent
// grilles at their normal (mid-intensity) glow are ALL part of this one
// image. Every constant below was re-measured DIRECTLY against master-05.png
// by .scaffold/gui-assets/faceplate-silentium-v3/analysis/measure_master_05.py
// (HoughCircles for the VU bezels/knobs/screws, HSV colour-threshold blob
// detection for the toggles/rose/vents - see that script's own docs for the
// exact technique per element family, and analysis/master_05_measurements.json
// for its raw output) - NOT copied from the master-04 table this file
// previously held (a different render generation; master-04 was a
// deliberately BARE plate for the now-abandoned "true component assembly"
// approach, so its measurements do not apply here even though the overall
// plate geometry is otherwise unchanged from master-03 onward, per Yves).
// Every constant is this master's own pixel geometry scaled by
// plateWidth1x / masterCanvasWidthPx down to this @1x table - re-derive all
// of them together (by re-running measure_master_05.py) if the master
// render is ever replaced.
namespace slnt::layout
{
    // juce::Rectangle/Point's constructors are not constexpr (JUCE 8.0.14),
    // so the rects below are plain namespace-scope consts rather than true
    // constexpr - still zero-initialisation-order risk since they only
    // depend on integer literals.

    // Master render's own canvas size, kept purely for documentation/
    // re-derivation purposes (the scale factor below is plateWidth1x /
    // masterCanvasWidthPx = 900 / 1264).
    constexpr int masterCanvasWidthPx = 1264;
    constexpr int masterCanvasHeightPx = 848;

    constexpr int plateWidth1x = 900;
    constexpr int plateHeight1x = 604; // masterCanvasHeightPx scaled by the same factor as plateWidth1x

    // Each AnalogMeter's component bounds are sized/positioned so the
    // needle/glow/LED overlay lands correctly on master-05's own BAKED dial
    // face - AnalogMeter no longer draws a face image of its own (see
    // AnalogMeter.h's v0.3.4 docs), so this box is purely a coordinate frame
    // for the overlay elements, not "the box a face asset is drawn into" as
    // in prior revisions. Measured: outer brass-bezel bounding box, master
    // px centre (321.0, 292.5)/(942.0, 292.0), diameter ~358px (mean of the
    // two meters' independently-measured diameters, which agree to within
    // 0.5px). meterPivotXFraction/meterPivotYFraction locate the needle-
    // pivot hub (the hex hub on the anchor bar) as a fraction of this box -
    // measured directly (HoughCircles on the right meter's own face bbox,
    // cross-checked by eye against the source render) rather than assumed;
    // both meters share this single fraction pair (mirrored-duplicate dial
    // design).
    constexpr int meterComponentSize1x = 255;
    const juce::Point<int> meterLTopLeft1x { 101, 81 };
    const juce::Point<int> meterRTopLeft1x { 543, 81 };
    constexpr float meterPivotXFraction = 0.478f;
    constexpr float meterPivotYFraction = 0.666f;

    // Control-bay knobs: a STAGGERED/brick layout baked into the master
    // render (row 2 sits offset right of row 1, not a straight 5-col/2-row
    // grid) - explicit per-knob centres rather than derived grid cells.
    // Row order/count matches PluginEditor.cpp's knobLayout table (row 1:
    // Threshold, Attack, Hold, Release, Range; row 2: Lookahead, SC HPF,
    // SC LPF, Knee). All 9 knobs are BAKED into master-05 at their 12
    // o'clock rest pose - these centres now position a transparent,
    // undecorated juce::Slider overlay (mouse + APVTS only, see
    // PluginEditor.cpp's Knob struct) rather than a rotating image.
    constexpr int knobRow1Y1x = 381;
    constexpr int knobRow2Y1x = 456;
    constexpr int knobDiameter1x = 48;

    constexpr std::array<int, 5> knobRow1X1x { 270, 360, 449, 539, 628 };
    constexpr std::array<int, 4> knobRow2X1x { 316, 405, 494, 584 };

    // Two footer toggles (Duck, Listen), same Y, explicit X centres - both
    // baked into master-05 in the UP/on position. toggleZoneSize1x is the
    // (deliberately generous) square crop PluginEditor.cpp blits from
    // master-06.png over master-05.png when a toggle is OFF (see that
    // file's paint() docs) - sized well beyond toggleSize1x (the toggle
    // body's own measured diameter) so the full lever-pivot arc is covered
    // with no visible seam; harmless to oversize since master-05/master-06
    // are pixel-identical everywhere outside the toggle mechanism itself.
    constexpr int toggleY1x = 514;
    constexpr int toggleSize1x = 32;
    constexpr int toggleZoneSize1x = 56;
    constexpr std::array<int, 2> toggleX1x { 405, 494 };

    // Rose flourish ornament - BAKED into master-05, no draw call (unlike
    // the master-04 generation's separate rose-emblem-v4.png overlay). Kept
    // here (same names as before) purely so
    // tests/gui/EditorLayoutTests.cpp's containment assertions keep
    // compiling against a real measurement; master-05's ornament is a thin
    // horizontal flourish line rather than the older circular/squircle
    // medallion, so roseDiameter1x is its bounding box's larger (width)
    // extent, not a true circle diameter.
    const juce::Point<int> roseCentre1x { 451, 323 };
    constexpr int roseDiameter1x = 150;

    // Four corner screws - BAKED into master-05, no draw call. Kept for the
    // same reason as roseCentre1x/roseDiameter1x above.
    const std::array<juce::Point<int>, 4> screwCentres1x {
        juce::Point<int> { 76, 77 },  // top-left
        juce::Point<int> { 815, 83 }, // top-right
        juce::Point<int> { 77, 525 }, // bottom-left
        juce::Point<int> { 820, 527 } // bottom-right
    };
    constexpr int screwDiameter1x = 21;

    // Tube-vent glow banks: unlike the master-04 generation (4 independently
    // flickering discrete tube-glow instances composited via a separate
    // tube-glow-v4.png asset), master-05 bakes the vent grille AND its
    // normal-intensity glow directly into the plate. The only remaining
    // dynamic behaviour is a SUBTLE, signal-driven cross-blend of this
    // WHOLE region between master-glow-dim.png (low signal) and master-05
    // itself (the approved baseline "normal" glow, t=1 - see
    // PluginEditor.cpp's paint() docs for the hard ceiling this cross-blend
    // must never exceed). Bounds measured directly (brightness-threshold
    // blob detection unioning the 6 individual slat bounding boxes) on the
    // RIGHT bank; the LEFT bank's bounds are the RIGHT bank's own box
    // mirrored across the plate's horizontal centre, because a direct
    // threshold on the left half is defeated by the diagonal softbox-
    // reflection sheen baked into that side of the render (see
    // measure_master_05.py's docs) - the two banks are a mirrored asset
    // pair by construction, so this is a measurement of the same geometry,
    // not an approximation of a different one.
    const juce::Rectangle<int> ventLBankBounds1x { 106, 355, 214 - 106, 487 - 355 };
    const juce::Rectangle<int> ventRBankBounds1x { 686, 355, 794 - 686, 487 - 355 };

    constexpr int topStripHeight1x = 32;
    constexpr int topStripGap1x = 6;
    constexpr int scaleButtonWidth1x = 64;

    constexpr int baseEditorWidth = plateWidth1x;
    constexpr int baseEditorHeight = topStripHeight1x + topStripGap1x + plateHeight1x;

    constexpr std::array<float, 3> scaleSteps { 1.0f, 1.5f, 2.0f };
}
