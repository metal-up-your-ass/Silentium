# Changelog

All notable changes to this project will be documented in this file.

The format is based on [Keep a Changelog 1.1.0](https://keepachangelog.com/en/1.1.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [Unreleased]

## [0.3.1] - 2026-07-18

### Changed

- **Full visual overhaul (v2 asset pass)** - the v0.3.0 GUI's assets were rejected in design review (soft meters, cheap-looking knob material, floating unhoused toggles, gray label chips, raw-JUCE preset bar). Every asset family is re-rendered from a mock-graded Blender pipeline, iterated against the approved "Raytrace-Basilica" mocks through three proof-sheet review rounds before production:
  - **Faceplate v2**: glossy near-black plate (mock-calibrated: measured mean luminance ~28 vs the mock plate's 15-47 range) with anisotropic brushed micro-texture, a fine gold pinstripe edge frame, and ALL captions (title nameplate, 9 knob labels, DUCK/LISTEN) ENGRAVED into the plate as EB Garamond gold-inlay lettering - the interim JUCE-drawn label chips are gone entirely.
  - **Circular glass-dome VU meters** (vu-dome-v1): round near-black bezels with a gold accent ring, warm amber-backlit dials (classic ~93° arc, 0 dB right-of-centre, solid red overload band), EB Garamond numerals and centre "VU" wordmark, substantial hub needle, and genuinely curved glass streak reflections. Rendered at the meter bay's exact pixel size - the v0.3.0 meters' ~2x runtime upscale (the "soft meter" root cause) is eliminated, and the @1x/@2x tier switch now never upscales (ImageDensity margin 1.25 -> 1.0).
  - **Knob filmstrip v2**: circular-anisotropic brushed brass with lacquer coat and edge wear, deeper/warmer gold, finer knurl pitch (128 ridges), and a wide engraved gold-inlay pointer readable at 100% scale. The v1 strip's baked shadow-catcher wash (the gray square behind every knob) is gone - v2 frames have verified fully transparent backgrounds.
  - **Toggle v2**: a complete housed switch (boolean-cut brass frame around a recessed slot, brass lever, state lamp) at 40px/80px tiers, replacing the tiny unhoused levers.
  - **Preset bar reskin** (suite-shared `src/presets/PresetBar` + `BasilicaLookAndFeel`): brass-rimmed 3-slice button caps with a baked near-black face panel (normal/hover baked states + runtime pressed darken; the dark face exists because - measured - no ink colour on bare midtone brass can clear WCAG's 4.5:1), warm-gold button lettering, the preset name in a recessed dark display window (componentID-selected draw path), and a dark header band with a gold hairline rule in the editor - no more raw default-grey toolbar.
  - **Typography system**: EB Garamond (OFL, embedded via BinaryData) for every JUCE-drawn text surface, matching the engraved plate lettering; chosen over Cormorant Garamond in a 12-14px legibility comparison.
- `AnalogMeter`: new ~93° tick table (matching the vu-dome-v1 face art), pivot fractions (0.5, 0.71), and `contentFractionOfCanvas` 0.95 (was 0.5). Layout table (`PluginEditorLayout.h`) re-authored for the circular meter bays; layout-invariant and tick-angle tests updated with it.
- **Accessibility preserved**: engraved labels replace visual `juce::Label`s only - every control keeps its accessible title/name, keyboard operability, focus rings (now also drawn on the brass preset-bar buttons), the dynamic scale-button title, and the meter value interfaces. A new WCAG contrast test covers the preset-bar button text against the button face's brightest tone.

## [0.3.0] - 2026-07-17

### Added

- **Photoreal skeuomorphic GUI (M3 pilot)** - the suite's first full custom editor, replacing the v0.1 functional slider/toggle layout. Built from pre-rendered Blender assets (the suite's gui-pipeline renders, copied into `resources/gui/` and embedded via BinaryData so the repo stays self-contained): a stone/gunmetal faceplate with engraved section bays, brass filmstrip knobs (128 frames, -135°..+135°), brass lever toggles with hover states, and two glass-covered analog needle VU meters (Gain Reduction + Input Level) with spring ballistics (~300 ms integration) driven by new lock-free metering atomics on the processor. See `docs/gui-preview.png` for the rendered result and `docs/gui-components.md` for the component architecture.
- **Suite-reusable GUI component family** (`src/gui/`): `FilmstripKnob` (filmstrip-backed `juce::Slider`, Shift = fine drag, double-click resets to the parameter default, mouse-wheel support), `FilmstripToggle` (4-frame `juce::Button`), `AnalogMeter` (face + vectorially rotated needle + glass overlay, unit-testable ballistics/tick-angle math), `BasilicaLookAndFeel` (gold serif labels with an engraved dual-shadow look - the interim JUCE-drawn label solution until per-control text is baked into the faceplate art), and `ImageDensity.h` (@1x/@2x asset tier selection). All Silentium-agnostic; the plugin-specific layout lives in a single coordinate table in `PluginEditor.cpp`.
- **Stepped window scaling** (100/150/200%, via a control next to the preset bar) - no free resize, because the artwork is pre-rendered at fixed density tiers. The chosen step persists in the plugin state (a plain `uiScaleStep` property on the APVTS tree) and round-trips through host session save/reload.
- **Accessibility**: all controls derive from stock `juce::Slider`/`juce::Button`, so JUCE's accessibility handlers, keyboard operation, and host parameter attachments work unchanged; accessible titles are set from parameter names, meters expose a label role, and creation order matches the visual reading order for focus traversal.
- Six new GUI test cases (76 total, up from 70): filmstrip frame-math edges, toggle frame-table mapping, meter ballistics step response and monotonic approach, tick-angle interpolation, editor construct/destroy, and an offscreen editor snapshot (written to `build/gui-preview.png`, committed as `docs/gui-preview.png`) verified non-blank.

### Changed

- `GateEngine` exposes `getCurrentGainDb()` (the gain currently applied to the main path) for the gain-reduction meter; `processBlock()` publishes it plus the pre-gate input peak level via relaxed atomics. No DSP behaviour change - all 70 pre-existing tests are unchanged and green.
- CMake project version bumped to 0.3.0.

## [0.2.0] - 2026-07-16

### Added

- **Research-derived voicing pass** (`docs/design-brief.md`, `docs/research-notes.md`): a deep-dive gap analysis against the reference class (ISP Decimator, dbx 166 series, FabFilter Pro-G, plus Nail The Mix/Fortin Amps workflow lore) identified Silentium's biggest gap versus the category as a lack of program-dependence in the gain computer, and a sidechain path that rejected hum but never emphasized the guitar pick-attack transient band. Both are addressed below. Every numeric default/range change is individually sourced in `docs/research-notes.md`; see `docs/design-brief.md`'s honesty section for what is and isn't independently verified against real hardware (nothing was - this is manual/practitioner-article-derived, not measured).
- **Program-dependent attack/release ramp**: replaces the v0.1.0 fixed dB/sample linear slope with an exponential approach whose per-sample convergence rate is calibrated so a full Range-span transition still completes in the stated Attack/Release time, but a partial transition (e.g. a brief dip that reopens before fully closing) now converges to the same absolute tolerance in proportionally less wall-clock time - the defining behaviour both ISP's "Time Vector Integration" and dbx's "AutoDynamic" cite as their headline differentiator. This is this brief's own plausible, testable mechanism, not a reproduction of either vendor's undisclosed proprietary algorithm - flagged as the riskiest change in the brief and recommended for validation against real playing.
- **SC LPF** parameter (`scLowpass`, 1000-16000 Hz, default 16000 Hz/effectively off): a second sidechain-only low-pass filter in series after SC HPF, letting the detection path be narrowed toward the documented 2-5 kHz guitar pick-attack transient band. Inert at its default, so a v0.1.0 session reproduces v0.1.0 behaviour exactly unless this new parameter is touched.
- **M2 preset system** (`.scaffold/specs/preset-system-m2.md`, copied from `basilica-audio/nave`'s pilot implementation): `src/presets/{PresetManager,PresetBar}` - factory presets (embedded via BinaryData), user presets (`~/Library/Audio/Presets/Yves Vogl/Silentium/` on macOS), save/save-as/rename/delete, single-file and zip-bank import/export, dirty-state tracking, prev/next navigation, and default-preset resolution (user Default > factory Default > built-in parameter defaults). Nine factory presets ship (`docs/presets.md`): Default, Surgical Mute, Natural Decay, Pick Attack Focus, DI-Keyed Workflow, Ambient Sustain, Chug Lock, Duck Under Lead, and Listen Check.
- **DE localisation frame** (`src/presets/Localisation.{h,cpp}`, `resources/i18n/de.txt`): every user-facing preset-bar frame string (buttons, menus, dialogs, error messages) is wrapped in `TRANS()`/`juce::translate()` and automatically switches to German when the system language is German, falling back to English otherwise. Core/DSP terminology (parameter names, units) is never translated.
- `docs/design-brief.md`, `docs/research-notes.md`, `docs/presets.md`: the full sourcing/reasoning behind this release's voicing changes and factory preset content.
- Test suite grew from 43 to 70 Catch2 cases: the seven design-brief test guarantees (SC LPF null test, a measured SC HPF/LPF band-pass curve, the program-dependent ramp proof, the Attack-floor-reaches-instantaneously proof, Hold-clamps-at-250ms, tolerant v0.1.0 state import, and a spectral proof that the Surgical Mute/Natural Decay preset pair are audibly distinct), sixteen M2 preset-system tests, and three i18n frame tests.

### Changed

- **Attack** floor lowered from 0.1 ms to 0 ms, so lookahead-assisted instantaneous (within-one-sample) gate opening is now reachable, matching documented practitioner guidance ("0.1ms to 1ms, sometimes even 0ms if your gate allows lookahead"). Default (1 ms) unchanged.
- **Hold** ceiling lowered from 500 ms to 250 ms, matching the best-documented software reference's own ceiling (v0.1.0's 500 ms had no found justification). Default (20 ms) unchanged. A hand-edited/future state with Hold above 250 ms clamps to the new ceiling on load rather than asserting or wrapping.
- Parameter count grew from 10 to 11 (`scLowpass` added); every pre-existing v0.1.0 parameter ID, range, and default not called out above is unchanged, and a v0.1.0 state tree loads with the new ID populated at its v0.2.0 default (tolerant import - no value remapping needed, since no existing ID was renamed or rescaled).
- CMake project version bumped to 0.2.0; `ICON_BIG` app icon wiring (already present from v0.1.1) verified unchanged.

### Fixed

- `docs/architecture.md`/`docs/manual.md`/`README.md` updated to describe the full v0.2.0 signal path (SC LPF, program-dependent ramp), parameter ranges, the M2 preset system, and the i18n frame.

## [0.1.1] - 2026-07-16

### Changed

- Housekeeping: new icon motif with canonical squircle cutout embedded into the plugin binary (`ICON_BIG`) and README/manual, org link sweep, heavy-music copy reframe, README pointed at GitHub Releases, and the signed tag-triggered release CI workflow added.

### Fixed

- **GateEngine: clamp/chunk `numSamples` against prepared capacity to prevent heap overflow on oversized host blocks** ([#12](https://github.com/basilica-audio/Silentium/issues/12)). `GateEngine::process()` now chunks any host block larger than the size promised to `prepare()` into pieces of at most that prepared capacity before touching `detectionBuffer`/`monoEnvelopeBuffer`, instead of trusting the host-supplied sample count directly. A block within capacity is unaffected (still exactly one iteration of the new chunking loop).

## [0.1.0] - 2026-07-14

### Added

- Project bootstrap: README, license, contributing guide, architecture and build docs, ADRs, and CI workflow.
- DSP core: initial working Silentium signal path (sidechain-filtered detection, stereo-linked peak envelope, hysteresis comparator + hold timer, dB-domain attack/release gain ramp, exact-integer lookahead delay with latency reporting) with unit tests.
- Knee parameter (0-24 dB, default 0 dB): soft-knee blend of the gain computer's target across a band centred on Threshold, in place of the original instant on/off snap; `Knee = 0 dB` reproduces v0.1 behaviour exactly, and the Hold timer still guarantees a fully open target for its whole duration regardless of Knee.
- Duck parameter (off by default): inverts the gain computer into a ducker - attenuate above Threshold instead of opening above it - reusing the same detection/hysteresis/hold/knee machinery.
- Listen parameter (off by default): routes the sidechain-filtered detection signal (post SC HPF, pre envelope-follower) directly to the output, bypassing the gain computer, for auditioning what the gate's detector hears.
- Optional external sidechain input bus (`"Sidechain"`, stereo, disabled by default): lets the detection path be keyed from another track (e.g. a kick drum or a reference DI) instead of the main input; `isBusesLayoutSupported` accepts the bus disabled, mono, or stereo independent of the main bus's own channel count, and a disabled/unconnected sidechain falls back to self-detection automatically.
- `docs/manual.md`: a full user manual (what the plugin is, where it sits in a symphonic-metal chain, signal-flow description, complete parameter reference, mixing tips).
- Broadened Catch2 suite: dedicated coverage for Knee/Duck/Listen/external sidechain (engine and processor level), bus-layout negotiation (mono/stereo main bus, sidechain enabled/disabled/mono/stereo/rejected), a 44.1-192 kHz sample-rate sweep, and a long-run (~21 s of audio) NaN/Inf stability test with continuously varying parameters/content; existing null/reference, hysteresis, latency, and state round-trip tests extended to cover the three new parameters and still pass unmodified at their defaults.

### Changed

- Parameter count grew from 7 to 10 (Knee, Duck, Listen added); all existing v0.1 parameter IDs, ranges, and defaults are unchanged.
- `GateEngine::process()` gained an optional second `sidechainBlock` parameter (default `nullptr`, fully backward compatible with the v0.1 single-block call).
- `docs/architecture.md` and `README.md` updated to describe the full v0.1.0 signal path (knee/duck/listen/sidechain), and the v0.1.0 GUI now also exposes Knee, Duck, and Listen.
