# Silentium Deep-Dive Research Notes

**Note on provenance:** the task briefed this as RESUME MODE against a pre-existing
`silentium-research-notes.md` and a structural template at
`miserere-design-brief-v2.md`. Neither file exists anywhere on this machine
(`find` over `/Users/yves/Development/Audio`, `~/.claude`, `/private/tmp` came up
empty for both before this session started writing them). This file is therefore
a fresh pass, not an extension of prior work. Structure follows the six-section
shape the task description itself specifies (why-v1-falls-short → topology →
module specs with sourced defaults → test guarantees → honesty section →
versioning), since that shape was given explicitly in the brief even without the
template file to confirm against.

## 1. What v1 actually does (read from source, not assumed)

Source: `/Users/yves/Development/Audio/silentium/src/dsp/GateEngine.{h,cpp}`,
`README.md`, `CLAUDE.md`, v0.1.0, M1 done.

Signal flow: `SC HPF (sidechain-only, 20-500 Hz Butterworth) -> stereo-linked
max(|.|) -> juce::dsp::BallisticsFilter peak envelope (fixed internal ballistics,
not exposed) -> dB -> hysteresis comparator (Threshold / Threshold-3dB, fixed
internal hysteresis) + hold countdown -> knee blend (smoothstep) -> optional duck
invert -> dB-domain attack/release ramp -> gain applied to lookahead-delayed main
signal (or Listen substitutes the SC-filtered detector output directly)`.

Full parameter set + ranges + defaults (from README "Features (v0.1.0 scope)"
and `GateEngine.h`'s `last*` member defaults, which agree):

| Parameter | Range | Default | Notes |
|---|---|---|---|
| Threshold | -80 to 0 dB | -40 dB | open threshold on SC-filtered envelope |
| Hysteresis (close threshold) | fixed | Threshold - 3 dB | not user-exposed |
| Attack | 0.1 - 50 ms | 1 ms | ramp Range floor -> unity |
| Hold | 0 - 500 ms | 20 ms | retriggered continuously while envelope > close threshold |
| Release | 5 - 500 ms | 80 ms | ramp unity -> Range floor |
| Range | -80 to 0 dB | -60 dB | floor attenuation; 0 dB = always open |
| Lookahead | 0 - 20 ms | 5 ms | reported as host latency; structural (re-derived only on `prepare()`) |
| SC HPF | 20 - 500 Hz | 80 Hz | sidechain-only, never touches main signal |
| Knee | 0 - 24 dB | 0 dB | smoothstep blend band centred on Threshold; 0 dB = hard knee |
| Duck | off/on | off | inverts openness (attenuate above Threshold) |
| Listen | off/on | off | routes SC-filtered detector signal to output |
| External sidechain | off/on (bus) | off | detection path keyed from a second input bus instead of self |

Fixed, not user-exposed, internal constants (`GateEngine.h` lines 128-156):
- `filterQ = sqrt(2)/2` (Butterworth Q) for the SC HPF.
- `hysteresisDb = 3.0f` fixed close-threshold offset.
- `detectorAttackMs = 0.3f`, `detectorReleaseMs = 15.0f` — internal
  `BallisticsFilter` ballistics, explicitly distinct from the user-facing
  Attack/Release (which shape the *gain ramp*, not the envelope follower).
- `minusInfinityDb = -100.0f` floor for dB conversions.
- Gain computer: single dB-domain linear ramp,
  `rate = (0 - rangeDb) / timeSamples` — i.e. a constant-slope "time to cross
  the full Range span" convention, applied identically regardless of how far
  above/below threshold the signal actually is. No program-dependence: a
  1 dB overshoot and a 40 dB overshoot both ramp at the same dB/sample rate.

Architecture already covers, well-reasoned in code comments:
- Real-time safety (no allocation in `process()`, buffers sized once in
  `prepare()`, coefficient assignment via `ArrayCoefficients` to avoid the
  heap-allocating `Coefficients::makeHighPass`).
- Stereo-linked detection (never shifts stereo image).
- External sidechain "splat" behaviour when the sidechain has fewer channels
  than the detection path.
- Lookahead treated as a structural (like-oversampling) parameter, re-derived
  only in `prepare()`, avoiding non-realtime-safe host latency calls from
  `process()`.
- Range = 0 dB / Knee = 0 / Duck = off collapses to a pure delay, used as the
  null-test reference (see `tests/EngineTests.cpp` per `CLAUDE.md`).

## 2. Reference class

Four sources triangulated, covering hardware pedal (ISP Decimator), hardware
rack (dbx 166 series), and software (FabFilter Pro-G), plus workflow lore from
metal-mixing practitioners (Nail The Mix, Fortin Amps on 4-cable-method
placement).

### ISP Technologies Decimator / Decimator II / Decimator X
- Threshold-setting ritual is explicit user guidance, not a fixed default:
  "the threshold control should be adjusted by listening to the noise floor
  while no instrument signal is present and turned clockwise until the noise
  floor becomes inaudible, and may need readjustment with the guitar volume
  all the way on." — [ManualsLib, Decimator II manual](https://www.manualslib.com/manual/877972/Isp-Technologies-Decimator-Ii.html)
- Decimator X's core differentiator is **program-dependent release**, branded
  "Time Vector Integration" / "Integration Release Window": "eliminates
  modulation of sustained notes, allowing long sustained notes to provide
  amazingly smooth and transparent release response" and "tracks the envelope
  of both super fast staccato notes as well as long sustained notes." —
  [ISP Decimator X G String manual, ManualsLib](https://www.manualslib.com/manual/3534337/Isp-Decimator-X-G-String-Noise-Reduction.html)
  This is the single most load-bearing finding for the gap analysis below: the
  category-defining hardware gate does NOT use a fixed-slope release ramp —
  it adapts release behaviour to whether the input is staccato or sustained.

### FabFilter Pro-G (software reference, extremely well-documented)
- Lookahead: "can open up to 10 ms before the audio level actually exceeds the
  threshold... when look-ahead is disabled and oversampling is off, Pro-G
  works without any latency. When enabled, the latency will be 10 ms" —
  [fabfilter.com/help/pro-g/using/timecontrols](https://www.fabfilter.com/help/pro-g/using/timecontrols)
  Notably smaller max lookahead (10 ms) than Silentium's 20 ms ceiling.
- Range: "specify the maximum expansion range... e.g. 20 dB, Pro-G will
  reduce the signal level with a maximum of 20 dB." Same "floor" concept as
  Silentium's Range, confirming the naming/semantics choice is sound.
- Hold: "minimum time the gate/expander will remain fully opened... adjustable
  hold time is up to 250 ms" — same semantic as Silentium's Hold but half the
  ceiling (Silentium: 500 ms).
- Attack/Release are explicitly **program-dependent**: "Pro-G is capable of
  very fast attack times and they are program dependent"; Release "is very
  program dependent" too.
- **Style presets are curated hysteresis + curve shapes, not raw knobs**:
  "Classic, Clean, Vocal, Guitar, Upward, Ducking... include built-in,
  carefully tuned hysteresis to prevent flutter." This validates Silentium's
  fixed-3dB hysteresis design decision (a fixed, non-user-exposed hysteresis
  amount is exactly what the category-reference plugin also does — it's not
  under-scoped, it's converged-on-independently correct) but exposes that
  Pro-G additionally varies curve *shape* per use case (a "Guitar" style
  exists distinct from "Vocal"/"Clean"), which Silentium's single generic
  knee shape doesn't differentiate.
- Knee: "custom soft knee setting... react more gradually" — same concept as
  Silentium's Knee, no numeric range published.
- Source: [fabfilter.com/help/pro-g/using/timecontrols](https://www.fabfilter.com/help/pro-g/using/timecontrols)

### dbx 166 series (166XS/166XL) — rack hardware reference
- "AutoDynamic attack and release circuitry that automatically adapts to
  program material for effortless setup" and "program-adaptive
  expander/gates with new gate timing algorithms ensure the smoothest release
  characteristics." — [dbxpro.com/en-US/products/166xs](https://dbxpro.com/en-US/products/166xs)
  Second independent confirmation (after ISP) that program-dependent release
  is the category norm, not an edge-case feature.
- dbx exposes the gate as a continuous-ratio **expander** (1:1 to ∞:1) rather
  than Silentium's binary "gate open / gate closed with hard floor" model —
  a genuinely different topology (partial gain reduction proportional to
  how far under threshold the signal is, vs Silentium's fixed Range floor
  regardless of how far under threshold). Noted as a scope decision for the
  honesty section, not something v2 should silently absorb.
- Source: [dbxpro.com/en-US/products/166xs](https://dbxpro.com/en-US/products/166xs)

### Boss NS-2 (pedal reference, different topology — send/return loop, not audio-rate sidechain filter)
- Confirms the "detect on a cleaner copy of the signal, act on the noisy
  chain" principle generalizes beyond DI-keying: the NS-2's own send/return
  loop "essentially functions as a sidechain, allowing the best results to be
  achieved by inserting noisy pedals in the NS-2's send/return loop... the
  circuitry continually detects the input signal while suppressing noise from
  the pedals." — [BOSS Articles, "The Many Uses of the BOSS NS-2 Noise Suppressor"](https://articles.boss.info/the-many-uses-of-the-boss-ns-2-noise-suppressor/)
- Decay (= Release) guidance: "slower DECAY settings will give a more natural
  overall guitar sound and avoid the end of long notes being 'chopped off' as
  they die out" — directly supports program-dependent or at least
  release-tuned-to-material guidance in the manual/presets, not a specific
  number.

### Workflow lore — Nail The Mix ("Noise Gate For Metal Guitars")
Concrete numeric practitioner guidance, most directly actionable for v2
defaults/presets:
- Attack: **"0.1ms to 1ms, sometimes even 0ms if your gate allows lookahead"**
  — matches Silentium's 0.1 ms floor and 1 ms default almost exactly; 0 ms
  is not currently reachable (floor is 0.1 ms), a minor gap.
- Hold: **"10-50ms"** for preventing chattering — Silentium's 20 ms default
  sits inside this band; the 500 ms range ceiling is far outside typical use
  (not wrong, just headroom for edge cases, e.g. ambient/clean).
- Range: for maximum tightness, **"-∞ dB (or its maximum setting)"**; for a
  more natural result, **"-12dB to -20dB"** partial reduction is called out as
  sounding better than full mute. Silentium's -60 dB default sits between
  these two practitioner modes (closer to "tight/surgical" than "natural") —
  worth codifying as two distinct presets rather than one default that
  compromises between them.
- Threshold: "above the noise floor (hiss, hum) but below the quietest part
  of the signal you want to keep" — descriptive, not numeric; supports
  Silentium keeping Threshold as a by-ear control (already the case).
- Key filter: **"2-5kHz"** cited as the guitar pick-attack frequency band to
  focus the gate's detection on. This is the clearest concrete gap: Silentium
  's SC path is HPF-only (20-500 Hz), aimed at rejecting hum/rumble, but has
  no mechanism to *emphasize* the pick-attack transient band the way the
  reference workflow recommends — a band-pass or a second (low-pass) SC
  control would let a mix engineer narrow the key signal onto 2-5 kHz instead
  of leaving everything above 80 Hz undifferentiated.
- Gold-standard workflow: **DI-keyed sidechain** — "record a clean DI signal
  alongside your amped tone, put the gate on your amped track, but set its
  Key Input/Sidechain to receive the DI signal... the DI doesn't have the
  compression and sustain of the high-gain amp tone, making the gate's job
  much easier and more precise." Silentium's external sidechain input bus
  (M1, already shipped) is architecturally exactly this — the finding here is
  confirmation, not a gap, but it means the manual/presets should explicitly
  document and lean into this workflow rather than treating external
  sidechain as a niche toggle.
- Source: [nailthemix.com/noise-gate-for-metal-guitars](https://www.nailthemix.com/noise-gate-for-metal-guitars)

### Workflow lore — Fortin Amps (4-cable-method / effects-loop placement)
- Explains *why* gate placement and the DI-key workflow matter physically:
  front-of-amp gating "does nothing to stop the 'hiss' generated by your
  amp's own high-gain preamp," while naive effects-loop gating on the
  amp's own (already distorted, compressed) signal makes "the gate 'struggle'
  to know when you've actually stopped playing... chatter... or cut-off
  sustain." The DI-sidechain resolves this because the gate "opens the
  millisecond you touch a string and slams shut the millisecond you stop,
  regardless of how much gain or feedback your amp is generating." —
  [fortinamps.com, "The Sidechain Secret"](https://fortinamps.com/blogs/news/the-sidechain-secret-why-the-4-cable-method-is-essential-for-high-gain-and-noise-gates)
  This is strong justification for why Silentium's external sidechain bus is
  not a nice-to-have but the category-correct way to gate high-gain guitar,
  and should be reflected in preset design (at least one preset framed
  explicitly as "use with a DI key").

## 3. Gap analysis — concretely what v1 gets wrong/generic/missing

1. **No program-dependence anywhere in the gain computer.** Both independent
   hardware references (ISP Decimator X's "Time Vector Integration" and dbx's
   "AutoDynamic") make adaptive attack/release their headline differentiator;
   FabFilter Pro-G's help text independently states both Attack and Release
   "are program dependent." Silentium's ramp is a constant dB/sample slope
   regardless of overshoot magnitude or how long the signal was open before
   closing — this is the single biggest "engineered core, not authentically
   voiced" gap. v1 is architecturally correct but sonically generic here.
2. **SC path detects hum/rumble but doesn't emphasize the pick-attack band.**
   Reference workflow calls out 2-5 kHz specifically as the frequency band a
   gate's key signal should focus on for high-gain guitar; Silentium only
   high-passes at 20-500 Hz and never shapes the top end of the detection
   path. A second SC filter stage (low-pass or a band control) is the
   concrete DSP gap.
3. **Range default (-60 dB) sits in an undocumented middle ground.**
   Practitioner guidance bifurcates into "tight/surgical" (-∞ / max
   attenuation) and "natural" (-12 to -20 dB) use cases; a single default
   compromises between them instead of being backed by either camp. This is
   better solved by presets (M2, already on the roadmap) than by changing the
   single default, but the brief should make the two camps explicit.
4. **No curve/style differentiation.** FabFilter differentiates "Guitar" from
   "Vocal"/"Clean"/"Classic" styles — different hysteresis and curve tuning
   per source material. Silentium has one hard-coded hysteresis value (3 dB)
   and one knee shape (smoothstep) for all material. The 3 dB hysteresis
   value itself is independently validated (Pro-G's styles are described as
   using "carefully tuned hysteresis," and fixed, non-adjustable hysteresis
   is the norm, not a gap) — but a single global curve for all use cases is
   generic where the reference class differentiates by instrument/context.
5. **Attack floor (0.1 ms) can't reach 0 ms with lookahead engaged.**
   Practitioner guidance explicitly allows "0ms if your gate allows
   lookahead" — Silentium has lookahead but its Attack range bottoms out at
   0.1 ms. Minor, easy v2 fix (lower the floor, not the default).
6. **External sidechain is under-messaged relative to how central it is to
   the category.** The DI-keyed workflow is described by two independent
   sources as the "gold standard" / architecturally necessary way high-gain
   gates are actually used; Silentium ships the feature already (M1) but
   nothing in the parameter set, manual, or (future) presets currently
   foregrounds it. Not a DSP gap — a documentation/preset-design gap.
7. **No continuous-ratio expansion mode.** dbx's rack reference exposes a
   1:1-to-∞:1 ratio; Silentium is architecturally a hard-floor gate (binary
   gate-open state that ramps to a fixed Range floor) plus a separately-scoped
   Duck inversion. This is a legitimate scope boundary, not obviously a
   defect — flagged for the honesty/versioning section as an explicit
   non-goal for v0.2.0 rather than silently absorbed.

## 4. Sources consulted (8 fetched/searched, all cited inline above)

1. ManualsLib — ISP Decimator II manual (threshold-setting ritual)
2. ManualsLib — ISP Decimator X G String manual (Time Vector Integration / program-dependent release)
3. fabfilter.com/help/pro-g/using/timecontrols (Lookahead, Range, Hold, Attack/Release program-dependence, Knee, Styles/hysteresis)
4. dbxpro.com/en-US/products/166xs (AutoDynamic, program-adaptive gate timing, continuous ratio)
5. articles.boss.info — "The Many Uses of the BOSS NS-2 Noise Suppressor" (send/return-as-sidechain, Decay/release-to-material guidance)
6. nailthemix.com/noise-gate-for-metal-guitars (concrete Attack/Hold/Range numbers, 2-5kHz key filter, DI-keyed gold-standard workflow)
7. fortinamps.com — "The Sidechain Secret" (why front-of-amp vs effects-loop vs DI-keyed gating differ physically)
8. Local repo: `README.md`, `CLAUDE.md`, `src/dsp/GateEngine.{h,cpp}` (ground truth for v1 behaviour)
