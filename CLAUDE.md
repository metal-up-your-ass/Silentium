# Silentium — tight lookahead noise gate (guitar)

Per-repo working memory for Claude Code sessions on this plugin. Part of the **Metal up your ass** symphonic-metal plugin suite (`github.com/metal-up-your-ass`).

## What this is
Silentium is the "tight lookahead noise gate (guitar)" member of the suite. AU / VST3 / Standalone, JUCE 8.

## Status (v0.1 — bootstrap complete)
Core DSP working, **19 Catch2 tests green**, CI (macOS + Windows, pluginval strictness 10 + auval) green. GUI is a functional v0.1 slider editor (custom LookAndFeel is roadmap M3). No signing yet (roadmap M4). Open work is tracked in this repo's GitHub **milestones/issues**.

## DSP
GateEngine (src/dsp/GateEngine.{h,cpp}) implements a lookahead hysteresis noise gate: a scratch copy of the input is high-passed by a sidechain-only IIR HPF (juce::dsp::IIR via ProcessorDuplicator, 20-500 Hz, Butterworth Q), stereo-linked via per-sample max(|channel|), then peak-tracked by juce::dsp::BallisticsFilter (fixed 0.3ms/15ms internal ballistics, distinct from the user-facing Attack/Release). A comparator uses two thresholds - Threshold (open) and Threshold-3dB (close, fixed internal hysteresis) - plus a per-sample-retriggered Hold countdown to drive a dB-domain attack/release gain ramp between 0 dB (open) and Range (closed floor). The main signal is delayed by an exact-integer juce::dsp::DelayLine<float, None> sized from Lookahead (a structural parameter re-derived only on prepare()) and multiplied by that gain; Lookahead is reported as the plugin's total latency via setLatencySamples(). Range=0dB collapses the whole engine to a pure delay, which is exploited as the null-test reference.

## Build & test
```sh
export CPM_SOURCE_CACHE="$HOME/.cache/CPM"      # shared JUCE 8.0.14 + Catch2 cache
cmake -B build -G Ninja -DCMAKE_BUILD_TYPE=Debug
cmake --build build --target Tests Silentium_Standalone --parallel 4
ctest --test-dir build --output-on-failure
```
Release/universal + pluginval + auval run in CI, not locally.

## Conventions & guardrails
- JUCE 8.0.14 via CPM · C++20 · AGPLv3 · Pamplejuce `SharedCode` pattern · manufacturer `Yvsv`, plugin code `Slnt`, `com.yvesvogl.silentium`.
- **Real-time safety:** no alloc/lock/file-IO/logging on the audio thread; allocate in `prepareToPlay`; `reset()` clears all state; `ScopedNoDenormals`; smoothed params; report latency via `setLatencySamples` where the chain adds any.
- **DryWetMixer gotcha (JUCE 8.0.14):** prime `setWetMixProportion(mix)` before `reset()` in `prepare()` (else it ramps from 100% wet). See sibling `overture`.
- **`main` is protected** — no direct commits; feature branch + PR, green CI required (Conventional Commits). New DSP needs tests (null/reference, NaN/Inf sweep, state round-trip, latency).

## Roadmap
GitHub milestones (M1 DSP & tests · M2 presets/state · M3 GUI & a11y · M4 release/signing/v1.0.0) + issues. Read with `gh issue list --repo metal-up-your-ass/silentium`.

## Suite context
Style references: sibling `metal-up-your-ass/overture` and `metal-up-your-ass/twist-your-guts`. The suite: overture, tenebrae, nave, silentium, requiem, seraph, aureate, firmament, triptych, apotheosis, twist-your-guts.
