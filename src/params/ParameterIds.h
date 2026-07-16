#pragma once

// Central definition of all AudioProcessorValueTreeState parameter IDs for
// Silentium. See docs/architecture.md for the corresponding signal-flow
// diagram.
//
// FROZEN AS OF THE v0.1 PARAMETER LAYOUT:
// Parameter IDs below must NEVER change once shipped - saved sessions and
// presets persist the APVTS state keyed by these string IDs, and renaming or
// removing one would silently break every user's saved state. Ranges,
// defaults, and skew MAY still be refined during voicing/tuning milestones;
// only the IDs themselves are frozen.
namespace ParamIDs
{
    // Open threshold, in dBFS, measured on the (sidechain-filtered) envelope.
    // The gate's close threshold sits a fixed amount below this (see
    // GateEngine::hysteresisDb) so a signal hovering right at Threshold does
    // not chatter the gate open/closed.
    inline constexpr auto threshold = "threshold";

    // Time for the gain computer to ramp from Range (closed) to 0 dB (open)
    // once the envelope crosses the open threshold.
    inline constexpr auto attack = "attack";

    // Minimum time the gate stays open once opened, regardless of the
    // envelope dropping below the close threshold - retriggered every sample
    // the envelope stays above the close threshold. Prevents the gate from
    // slamming shut between consecutive transients of a palm-muted phrase.
    inline constexpr auto hold = "hold";

    // Time for the gain computer to ramp from 0 dB back down to Range once
    // the hold time has elapsed with the envelope below the close threshold.
    inline constexpr auto release = "release";

    // Floor attenuation applied when the gate is closed, in dB. 0 dB means
    // the gate never attenuates at all (an always-open passthrough).
    inline constexpr auto range = "range";

    // Lookahead applied to the main (delayed) signal path so the gate can
    // start opening slightly before a transient actually arrives. Reported
    // to the host as this plugin's total latency (see
    // SilentiumAudioProcessor::prepareToPlay).
    inline constexpr auto lookahead = "lookahead";

    // Sidechain high-pass filter cutoff applied only to the detection path
    // (never to the main signal), so that low-frequency hum/rumble doesn't
    // falsely hold the gate open.
    inline constexpr auto scHighpass = "scHighpass";

    // v0.2.0: sidechain low-pass filter cutoff, applied only to the
    // detection path in series after the SC HPF, so the detection band can
    // be narrowed toward the guitar pick-attack transient region (roughly
    // 2-5 kHz - see docs/design-brief.md) instead of only having its bottom
    // end rejected. Defaults fully open (16 kHz) so a v0.1.0 session that
    // never touches it reproduces v0.1.0 behaviour exactly (tolerant
    // import - see docs/design-brief.md's Versioning section).
    inline constexpr auto scLowpass = "scLowpass";

    // Soft-knee width, in dB, centred on Threshold. 0 dB reproduces the
    // original v0.1 hard-knee behaviour exactly (the gain computer's target
    // snaps between Range and 0 dB at the open/close thresholds); a wider
    // knee blends the target gain smoothly across the band, still bounded by
    // the same hysteresis/hold state machine (see GateEngine::process()).
    inline constexpr auto knee = "knee";

    // Duck: inverts the gain computer so the detector crossing Threshold
    // attenuates the output toward Range instead of opening it - the same
    // detection path (SC HPF, hysteresis, hold, lookahead) driving a ducker
    // instead of a gate.
    inline constexpr auto duck = "duck";

    // Listen: routes the sidechain-filtered detection signal (post SC HPF,
    // pre envelope-follower) directly to the output, bypassing the gain
    // computer entirely, so the gate's actual trigger signal can be
    // auditioned while dialling in SC HPF/Threshold.
    inline constexpr auto listen = "listen";
}
