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
// v0.3.2 (this revision): the faceplate is now a SINGLE photoreal master
// render (.scaffold/gui-assets/faceplate-silentium-v3/master-03-final.png,
// 1264x848 - obsidian plate, both VU dials, tube grilles, 5+4 brass knobs,
// 2 toggles, and the rose emblem all baked into one image) rather than a
// JUCE-drawn glossy plate plus separate pre-rendered decorations. Every
// constant below is that master's own measured pixel geometry
// (.scaffold/gui-assets/faceplate-silentium-v3/faceplate-metadata.json),
// scaled by plateWidth1x / masterCanvasWidthPx down to this @1x table -
// re-derive both together if the master render is ever replaced.
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

    // plateHeight1x is masterCanvasHeightPx scaled by the SAME factor as
    // plateWidth1x (900 * 848 / 1264 = 603.8, rounded), so the master image
    // fills the plate exactly with no letterboxing in the base (100%) case -
    // paint() still draws it via RectanglePlacement::centred (keeping
    // aspect) defensively, in case that ever changes.
    constexpr int plateWidth1x = 900;
    constexpr int plateHeight1x = 604;

    // Each AnalogMeter's component bounds are a square CENTRED EXACTLY ON
    // ITS PIVOT (the brass hub rivet the needle rotates around in the
    // master render), half-size 111px @1x (156 master px = measured needle
    // reach ~130px + a 20-ish px antialiasing/glow margin, scaled). This is
    // the "meter_component_convention" documented in faceplate-metadata.json:
    // AnalogMeter's pivot fraction is therefore always (0.5, 0.5) regardless
    // of which meter - see AnalogMeter.h.
    constexpr int meterHalfSize1x = 111;
    const juce::Point<int> meterLPivot1x { 229, 249 };
    const juce::Point<int> meterRPivot1x { 673, 249 };

    // Control-bay knobs: a STAGGERED/brick layout baked into the master
    // render (row 2 sits offset ~half a cell right of row 1, not a straight
    // 5-col/2-row grid like the previous JUCE-drawn plate) - explicit
    // per-knob centres rather than derived grid cells, matching
    // faceplate-metadata.json's "knobs" block exactly. Row order/count
    // matches PluginEditor.cpp's knobLayout table (row 1: Threshold, Attack,
    // Hold, Release, Range; row 2: Lookahead, SC HPF, SC LPF, Knee).
    constexpr int knobRow1Y1x = 380;
    constexpr int knobRow2Y1x = 455;
    constexpr int knobDiameter1x = 49;

    constexpr std::array<int, 5> knobRow1X1x { 273, 362, 451, 539, 628 };
    constexpr std::array<int, 4> knobRow2X1x { 315, 405, 495, 584 };

    // Two footer toggles (Duck, Listen), same Y, explicit X centres.
    constexpr int toggleY1x = 513;
    constexpr int toggleSize1x = 28;
    constexpr std::array<int, 2> toggleX1x { 403, 491 };

    // Extra strip above the plate art for the preset bar + scale control -
    // interactive text/menus don't fit the plate's own thin engraved aux
    // strip at any legible size, so they live in their own band instead.
    // Styled as an integrated header strip by BasilicaLookAndFeel (brass
    // buttons + recessed preset-name display), not a raw-JUCE toolbar.
    constexpr int topStripHeight1x = 32;
    constexpr int topStripGap1x = 6;
    constexpr int scaleButtonWidth1x = 64;

    constexpr int baseEditorWidth = plateWidth1x;
    constexpr int baseEditorHeight = topStripHeight1x + topStripGap1x + plateHeight1x;

    constexpr std::array<float, 3> scaleSteps { 1.0f, 1.5f, 2.0f };
}
