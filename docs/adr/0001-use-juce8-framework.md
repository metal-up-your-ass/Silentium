# 1. Use JUCE 8 as the plugin framework

* Status: accepted
* Deciders: Yves Vogl
* Date: 2026-07-14

## Context and Problem Statement

Silentium needs a cross-platform audio plugin framework that can target AU and VST3 on macOS and VST3 on Windows, ship a Standalone build, and support a modern C++ toolchain. Which framework should the project build on?

## Decision Drivers

* Must produce **AU** — Logic Pro is a primary target DAW, and Logic only hosts AU (plus VST3 via internal support), so a framework without native AU support is a non-starter.
* Breadth of built-in DSP coverage (filters, oversampling, convolution, dynamics, etc.) so the envelope following, lookahead delay, and hysteresis gating work isn't reinventing well-trodden primitives.
* Ecosystem maturity: validation tooling (`pluginval`), CI scaffolding patterns (e.g. Pamplejuce-style projects), community familiarity, available examples.
* Track record: professional, commercially shipped plugins in the "heavy DSP, character processing" category are built on it, suggesting the framework holds up under real-world DSP and GUI demands.

## Considered Options

* **JUCE 8** (C++20)
* **iPlug2**
* **Rust + `nih-plug`**

## Decision Outcome

Chosen option: **JUCE 8**, because it is the only option that satisfies the hard AU requirement and comes with the broadest `juce::dsp` module coverage and tooling ecosystem of the three.

### Consequences

* Good, because `juce::dsp` provides ready building blocks (IIR/FIR filters, oversampling, convolution, state variable filters) that map directly onto this project's envelope following, lookahead delay, and hysteresis gating needs.
* Good, because `pluginval` and established JUCE CI patterns give a known-good path to a CI gate (see [`.github/workflows/ci.yml`](../../.github/workflows/ci.yml)).
* Good, because JUCE's plugin wrapper layer (`juce_audio_plugin_client`) handles AU/VST3/Standalone format wrapping, avoiding hand-written per-format glue.
* Bad, because JUCE 8's open-source tier is AGPLv3-licensed (see [ADR 0002](0002-agplv3-licensing.md)), which is a stronger copyleft than a permissive alternative would have required.
* Bad, because JUCE is a large dependency (compile times, binary size) compared to leaner alternatives.

## Pros and Cons of the Options

### JUCE 8

* Good, because it has first-class, mature AU support.
* Good, because `juce::dsp` covers most of the DSP primitives this project needs out of the box.
* Good, because `pluginval`, Pamplejuce-derived CI templates, and a large body of example plugin code exist.
* Neutral, because it requires C++20-capable toolchains, which is satisfied by current Xcode/MSVC.
* Bad, because its open-source license is AGPLv3, a strong copyleft (acceptable here since the project intends copyleft open source regardless — see ADR 0002).

### iPlug2

* Good, because it is permissively licensed (no copyleft obligations).
* Bad, because it has a smaller ecosystem and fewer maintained examples/tooling than JUCE.
* Bad, because its DSP module coverage is thinner, pushing more primitives (filters, oversampling) onto the project to implement and test from scratch.
* Neutral, because it does support AU, so it isn't disqualified outright — it loses on ecosystem/DSP-coverage drivers, not the hard AU requirement.

### Rust + `nih-plug`

* Good, because Rust's memory-safety guarantees remove a class of real-time-audio bugs (use-after-free, data races) at compile time.
* Good, because `nih-plug`'s plugin abstraction is modern and ergonomic for VST3/CLAP.
* Bad, because it has **no native AU support** — a dealbreaker given Logic Pro is a primary target and Logic is AU-first.
* Bad, because its DSP/GUI ecosystem is younger and thinner than JUCE's for this category of plugin.
