# Contributing to Silentium

Thanks for your interest in contributing. This document covers the workflow, testing requirements, and code style expected for changes to this project.

## Branch workflow

- **No direct commits to `main`.** All changes land through a pull request.
- Create a feature branch off `main`, named descriptively (e.g. `feature/oversampling`, `fix/gate-hysteresis`, `docs/building-windows`).
- Keep pull requests scoped to a single concern where practical — easier to review, easier to bisect.
- Rebase onto the latest `main` before requesting review if your branch has drifted.
- A PR needs a green CI run before merge (see [CI gates](#ci-gates) below).

## Commit messages

This project uses [Conventional Commits](https://www.conventionalcommits.org/en/v1.0.0/):

```
<type>[optional scope]: <description>

[optional body]

[optional footer(s)]
```

Common types: `feat`, `fix`, `docs`, `test`, `refactor`, `perf`, `build`, `ci`, `chore`.

Examples:

```
feat(dsp): add oversampled waveshaper stage
fix(gate): correct hysteresis threshold comparison
test(dsp): add null test for unity-gain bypass
```

## DSP test requirements

Silentium is real-time audio software — DSP correctness bugs are expensive to catch late, so tests are not optional for DSP code.

**New DSP code without tests does not merge.** At minimum, new or changed DSP components need:

- **Reference / null tests** — a bypass or unity-parameter configuration must null against a known-good reference to a tight tolerance (e.g. below -90 dBFS residual), or reproduce the expected analytic response within tolerance.
- **NaN / Inf sweeps** — feed edge-case input (silence, full-scale, denormals, extreme parameter values, rapid parameter automation) through the signal chain and assert no `NaN`/`Inf` escapes.
- **State round-trip tests** — serialize plugin/parameter state, deserialize it, and verify the reconstructed state matches (parameter values, preset data, and any versioned migration logic).

Tests live under `tests/` and use Catch2 v3. Run them locally with:

```sh
ctest --test-dir build --output-on-failure
```

## Real-time audio rules

Code that runs on the audio thread (`processBlock` and anything it calls) must be real-time safe. In particular:

- **No heap allocation** — no `new`, no `malloc`, no growing `std::vector`/`std::string` on the audio thread. Pre-allocate in `prepareToPlay`.
- **No locks** — no `std::mutex`, no blocking synchronization primitives. Use lock-free structures (e.g. atomics, `juce::AbstractFifo`) for cross-thread communication.
- **No file or network I/O** — no reading/writing files, no sockets, on the audio thread. Load IRs, presets, and other resources off the audio thread and hand off safely.
- **No logging** — no `DBG`, `std::cout`, `printf`, or similar on the audio thread; these can block or allocate.
- **No unbounded loops** — avoid anything whose iteration count depends on unbounded external state.

If a change touches `processBlock` or code reachable from it, call this out explicitly in the PR description.

## CI gates

Pull requests must pass:

- The **build matrix** (macOS + Windows, Release configuration).
- **`ctest`** — the full DSP/unit test suite.
- **`pluginval`** at **strictness level 10** against the built VST3.

See [`.github/workflows/ci.yml`](.github/workflows/ci.yml) for the exact steps.

## Code style

- **C++20.** Use its features where they improve clarity (concepts, ranges, `constexpr` extensions) — don't force them where they don't.
- **Follow JUCE idioms.** Use `juce::dsp` module building blocks where equivalents exist rather than hand-rolling DSP primitives. Follow JUCE's `AudioProcessorValueTreeState` conventions for parameter management.
- Prefer `const`/`constexpr` and value semantics; avoid raw owning pointers — use JUCE/STL smart pointer types where ownership is needed.
- Keep `processBlock` and other audio-thread code readable; extract non-trivial DSP logic into dedicated, unit-testable classes under `src/dsp`.
- Match the formatting of surrounding code in a file rather than introducing a new style locally.
