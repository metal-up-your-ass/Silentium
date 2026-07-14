#pragma once

#include <juce_dsp/juce_dsp.h>

// The complete Silentium signal path, independent of juce::AudioProcessor so
// it can be exercised directly by unit tests without instantiating a full
// plugin (see tests/EngineTests.cpp). Owns all DSP state; every buffer/
// filter/delay-line is allocated in prepare() and never reallocated on the
// audio thread.
//
// Signal flow (see docs/architecture.md for the full diagram):
//
//   Detection path (never reaches the output):
//     input -> SC HPF (sidechain-only) -> stereo-linked max|.| -> peak
//     envelope follower -> dBFS -> hysteresis comparator + hold timer
//     -> attack/release gain ramp (dB domain, floor = Range)
//
//   Main path:
//     input -> lookahead delay -> * gain (from the detection path, applied
//     identically to every channel: a stereo-linked gate never shifts the
//     stereo image) -> output
//
// The gate uses separate open/close thresholds (Threshold, and Threshold
// minus a fixed internal hysteresis amount) so a signal hovering right at
// Threshold cannot chatter the gate open and closed on every sample - this is
// what tests/EngineTests.cpp's hysteresis test verifies. Range == 0 dB means
// the floor is 0 dB below unity, i.e. the gate never attenuates at all; this
// is used as an "always open" reference passthrough in the null test.
class GateEngine
{
public:
    GateEngine();

    // Allocates all DSP state. Must be called (and completed) before the
    // first process() call, and again whenever sample rate/block size/
    // channel count change.
    void prepare (const juce::dsp::ProcessSpec& spec);

    // Clears all filter/envelope-follower/delay-line/gate state without
    // deallocating. Safe to call from the audio thread (e.g. on playback
    // stop/loop).
    void reset();

    // Processes `block` in place. `block` must have at most the maximum
    // sample/channel counts declared to prepare(); a zero-sample block is a
    // safe no-op. No allocation occurs here.
    void process (juce::dsp::AudioBlock<float>& block);

    // Parameter setters, in real units (dB, ms, Hz). Safe to call every block
    // from the audio thread - no allocation/locks. Range and SC HPF are
    // smoothed internally (see rangeSmoothed/scHighpassSmoothed); Threshold/
    // Attack/Hold/Release only affect the discrete gate state machine's
    // decision boundaries/ramp rates, so they are applied directly without
    // additional smoothing (a block-rate step in these does not itself
    // multiply the audio signal, unlike Range).
    void setThresholdDb (float newThresholdDb);
    void setAttackMs (float newAttackMs);
    void setHoldMs (float newHoldMs);
    void setReleaseMs (float newReleaseMs);
    void setRangeDb (float newRangeDb);
    void setLookaheadMs (float newLookaheadMs);
    void setScHighpassHz (float newFrequencyHz);

    // Lookahead latency in samples, valid after prepare() has run. Lookahead
    // is treated as a structural parameter (like an oversampling factor):
    // its value at the time prepare() runs determines both the applied delay
    // and the reported latency for the life of that prepared session. Live
    // changes via setLookaheadMs() take effect on the next prepare() call
    // (typically triggered by the host re-calling prepareToPlay), not
    // immediately - this keeps the reported host latency always consistent
    // with the actual delay applied, without calling into
    // AudioProcessor::setLatencySamples()/updateHostDisplay() (which are not
    // safe to call from the audio thread) from inside process().
    int getLatencySamples() const noexcept { return latencySamples; }

    // True while the gate is open (including its attack/hold phase), false
    // while closed/releasing towards Range. Cheap, side-effect-free query;
    // exposed for diagnostics/tests (see tests/EngineTests.cpp's hysteresis
    // test) and a future GUI gate-open indicator.
    bool isGateOpen() const noexcept { return gateOpen; }

private:
    // Butterworth (maximally-flat) Q for the sidechain HPF.
    static constexpr float filterQ = juce::MathConstants<float>::sqrt2 / 2.0f;

    // Fixed hysteresis: the close threshold sits this many dB below the
    // open (Threshold) value, so the two thresholds never coincide - the
    // single most common cause of gate chatter on a signal hovering near one
    // threshold.
    static constexpr float hysteresisDb = 3.0f;

    // Envelope-follower ballistics (fixed, not user-exposed): fast attack so
    // transients are caught almost immediately, moderate release so the
    // envelope doesn't itself chatter on a bumpy, sustained signal. Distinct
    // from the user-facing Attack/Release, which shape the gain ramp, not
    // the envelope.
    static constexpr float detectorAttackMs = 0.3f;
    static constexpr float detectorReleaseMs = 15.0f;

    // Floor used for both Decibels::gainToDecibels/decibelsToGain calls -
    // anything quieter is treated as silence for gate-logic purposes.
    static constexpr float minusInfinityDb = -100.0f;

    // Generous upper bound on Lookahead so the delay line's fixed capacity
    // (set once in prepare()) comfortably covers the whole parameter range
    // (0-20 ms, see ParameterLayout.cpp) at any realistic sample rate.
    static constexpr float maxLookaheadMs = 25.0f;

    static float clampBelowNyquist (float frequencyHz, double sampleRate) noexcept;
    int computeLookaheadSamples() const noexcept;

    double sampleRate = 44100.0;
    juce::uint32 numChannels = 2;

    // Sidechain-only high-pass; never applied to the main signal path.
    juce::dsp::ProcessorDuplicator<juce::dsp::IIR::Filter<float>, juce::dsp::IIR::Coefficients<float>> scHighPass;

    // Mono (1-channel) peak envelope follower fed by the stereo-linked
    // max|.| of the sidechain-filtered detection signal.
    juce::dsp::BallisticsFilter<float> envelopeFollower;

    // Lookahead delay on the main signal, one channel-independent tap per
    // channel but driven by the same (stereo-linked) gain envelope.
    juce::dsp::DelayLine<float, juce::dsp::DelayLineInterpolationTypes::None> lookaheadDelay { 0 };

    // Scratch buffers for the detection path, sized in prepare() to the
    // maximum block size declared there; never resized on the audio thread.
    juce::AudioBuffer<float> detectionBuffer;
    juce::AudioBuffer<float> monoEnvelopeBuffer;

    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> rangeSmoothed;
    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Multiplicative> scHighpassSmoothed;
    static constexpr double smoothingTimeSeconds = 0.05;

    // Last commanded values (ParameterLayout defaults until a setter is
    // called), re-applied on every prepare() so re-prepare (sample-rate
    // change, etc.) never resets a live parameter back to a default.
    float lastThresholdDb = -40.0f;
    float lastAttackMs = 1.0f;
    float lastHoldMs = 20.0f;
    float lastReleaseMs = 80.0f;
    float lastRangeDb = -60.0f;
    float lastLookaheadMs = 5.0f;
    float lastScHighpassHz = 80.0f;

    // Gate state machine, advanced one sample at a time in process().
    bool gateOpen = false;
    int holdCounterSamples = 0;
    float currentGainDb = -60.0f;

    int latencySamples = 0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (GateEngine)
};
