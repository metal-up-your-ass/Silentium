#include "GateEngine.h"

#include <algorithm>
#include <cmath>

GateEngine::GateEngine() = default;

float GateEngine::clampBelowNyquist (float frequencyHz, double rate) noexcept
{
    const auto nyquist = static_cast<float> (rate) * 0.5f;
    return juce::jlimit (10.0f, nyquist * 0.9f, frequencyHz);
}

int GateEngine::computeLookaheadSamples() const noexcept
{
    const auto clampedMs = juce::jlimit (0.0f, maxLookaheadMs, lastLookaheadMs);
    return juce::roundToInt (clampedMs * 0.001 * sampleRate);
}

void GateEngine::prepare (const juce::dsp::ProcessSpec& spec)
{
    sampleRate = spec.sampleRate;
    numChannels = spec.numChannels;

    scHighPass.prepare (spec);

    juce::dsp::ProcessSpec monoSpec = spec;
    monoSpec.numChannels = 1;
    envelopeFollower.prepare (monoSpec);
    envelopeFollower.setLevelCalculationType (juce::dsp::BallisticsFilterLevelCalculationType::peak);
    envelopeFollower.setAttackTime (detectorAttackMs);
    envelopeFollower.setReleaseTime (detectorReleaseMs);

    // Lookahead is a structural parameter (see getLatencySamples()'s
    // comment): the delay line's maximum capacity comfortably covers the
    // whole parameter range regardless of what is currently requested, but
    // the *applied* delay and reported latency are only re-derived here, in
    // prepare(), from the current lastLookaheadMs.
    const auto maxLookaheadSamples = static_cast<int> (std::ceil (maxLookaheadMs * 0.001 * sampleRate)) + 1;
    lookaheadDelay.setMaximumDelayInSamples (maxLookaheadSamples);
    lookaheadDelay.prepare (spec);

    detectionBuffer.setSize (static_cast<int> (spec.numChannels), static_cast<int> (spec.maximumBlockSize), false, false, true);
    monoEnvelopeBuffer.setSize (1, static_cast<int> (spec.maximumBlockSize), false, false, true);

    rangeSmoothed.reset (sampleRate, smoothingTimeSeconds);
    rangeSmoothed.setCurrentAndTargetValue (lastRangeDb);
    scHighpassSmoothed.reset (sampleRate, smoothingTimeSeconds);
    scHighpassSmoothed.setCurrentAndTargetValue (lastScHighpassHz);

    latencySamples = computeLookaheadSamples();
    lookaheadDelay.setDelay (static_cast<float> (latencySamples));

    reset();

    // Prime the SC HPF coefficients immediately so the very first process()
    // call runs with correct, non-default coefficients rather than an
    // identity/uninitialised state. Assigning from the plain std::array
    // returned by ArrayCoefficients::makeHighPass (rather than
    // Coefficients::makeHighPass, which heap-allocates a new reference-
    // counted Coefficients object every call) also grows scHighPass.state's
    // internal juce::Array to its final capacity here, in prepare(), so the
    // identical assignment in process() below never allocates either - see
    // the comment there.
    *scHighPass.state = juce::dsp::IIR::ArrayCoefficients<float>::makeHighPass (
        sampleRate, clampBelowNyquist (lastScHighpassHz, sampleRate), filterQ);
}

void GateEngine::reset()
{
    scHighPass.reset();
    envelopeFollower.reset();
    lookaheadDelay.reset();

    gateOpen = false;
    holdCounterSamples = 0;
    // Start fully closed at the current Range floor, so a signal that never
    // crosses Threshold stays silent (apart from the attack ramp) rather
    // than momentarily passing through at 0 dB right after reset()/prepare().
    currentGainDb = lastRangeDb;
}

void GateEngine::setThresholdDb (float newThresholdDb)
{
    lastThresholdDb = newThresholdDb;
}

void GateEngine::setAttackMs (float newAttackMs)
{
    lastAttackMs = newAttackMs;
}

void GateEngine::setHoldMs (float newHoldMs)
{
    lastHoldMs = newHoldMs;
}

void GateEngine::setReleaseMs (float newReleaseMs)
{
    lastReleaseMs = newReleaseMs;
}

void GateEngine::setRangeDb (float newRangeDb)
{
    lastRangeDb = newRangeDb;
    rangeSmoothed.setTargetValue (newRangeDb);
}

void GateEngine::setLookaheadMs (float newLookaheadMs)
{
    // See getLatencySamples(): the new value only takes effect on the next
    // prepare() call, not immediately.
    lastLookaheadMs = newLookaheadMs;
}

void GateEngine::setScHighpassHz (float newFrequencyHz)
{
    lastScHighpassHz = newFrequencyHz;
    scHighpassSmoothed.setTargetValue (newFrequencyHz);
}

void GateEngine::setKneeDb (float newKneeDb)
{
    lastKneeDb = newKneeDb;
}

void GateEngine::setDuckingMode (bool shouldDuck)
{
    duckingMode = shouldDuck;
}

void GateEngine::setListenMode (bool shouldListen)
{
    listenMode = shouldListen;
}

void GateEngine::process (juce::dsp::AudioBlock<float>& block, const juce::dsp::AudioBlock<float>* sidechainBlock)
{
    const auto numSamples = block.getNumSamples();

    if (numSamples == 0)
        return;

    // --- Detection path: SC HPF (sidechain-only, never touches the main
    // signal) applied to a scratch copy of either the main input or an
    // external sidechain input, if one was supplied and usable. ---
    juce::dsp::AudioBlock<float> detectionBlock (detectionBuffer);
    auto detectionSub = detectionBlock.getSubBlock (0, numSamples);

    const auto sidechainUsable = sidechainBlock != nullptr
                                  && sidechainBlock->getNumChannels() > 0
                                  && sidechainBlock->getNumSamples() == numSamples;

    if (sidechainUsable)
    {
        // Splat: if the sidechain has fewer channels than the detection path
        // (e.g. a mono sidechain feeding a stereo instance), reuse the last
        // available sidechain channel for the remaining detection channels
        // rather than leaving them holding stale data from a previous block.
        const auto sidechainChannels = sidechainBlock->getNumChannels();

        for (size_t channel = 0; channel < detectionSub.getNumChannels(); ++channel)
        {
            const auto sourceChannel = std::min (channel, sidechainChannels - 1);
            detectionSub.getSingleChannelBlock (channel).copyFrom (sidechainBlock->getSingleChannelBlock (sourceChannel));
        }
    }
    else
    {
        detectionSub.copyFrom (block);
    }

    const auto scHz = clampBelowNyquist (scHighpassSmoothed.skip (static_cast<int> (numSamples)), sampleRate);

    // Recomputed once per block from the smoothed cutoff. Deliberately uses
    // ArrayCoefficients::makeHighPass (a plain std::array<float, 6> returned
    // by value) assigned in place into the existing, ref-counted
    // scHighPass.state object, *not* Coefficients::makeHighPass, whose
    // implementation is `return *new Coefficients (...)`: a heap allocation
    // (plus the matching deallocation of the temporary Coefficients::Ptr)
    // on every single process() call, which is not real-time-safe.
    // Coefficients::operator=(const std::array&) reuses the juce::Array
    // storage already reserved by the identical assignment in prepare()
    // above (ensureStorageAllocated only grows, never shrinks/reallocates
    // when the requested size is already met), so this line does not
    // allocate either.
    *scHighPass.state = juce::dsp::IIR::ArrayCoefficients<float>::makeHighPass (sampleRate, scHz, filterQ);

    juce::dsp::ProcessContextReplacing<float> detectionContext (detectionSub);
    scHighPass.process (detectionContext);

    // Stereo-linked combine: per-sample max(|channel|) across all channels,
    // so a signal panned to one side alone can still open the gate, and the
    // gate never shifts the stereo image (the same gain is applied to every
    // channel below).
    auto* monoData = monoEnvelopeBuffer.getWritePointer (0);

    for (size_t sample = 0; sample < numSamples; ++sample)
    {
        float maxAbs = 0.0f;

        for (size_t channel = 0; channel < detectionSub.getNumChannels(); ++channel)
            maxAbs = std::max (maxAbs, std::abs (detectionSub.getChannelPointer (channel)[sample]));

        monoData[sample] = maxAbs;
    }

    juce::dsp::AudioBlock<float> monoBlock (monoEnvelopeBuffer);
    auto monoSub = monoBlock.getSubBlock (0, numSamples);
    juce::dsp::ProcessContextReplacing<float> monoContext (monoSub);
    envelopeFollower.process (monoContext);
    // monoEnvelopeBuffer now holds the per-sample linear peak envelope.

    // --- Gain computer: hysteresis comparator + hold timer + attack/release
    // ramp, all in the dB domain, evaluated once per block for the
    // block-rate quantities (Range, thresholds, ramp rates) and once per
    // sample for the state machine and gain itself. ---
    const auto rangeDbNow = rangeSmoothed.skip (static_cast<int> (numSamples));
    const auto closeThresholdDb = lastThresholdDb - hysteresisDb;

    const auto attackTimeSamples = std::max (1.0f, static_cast<float> (lastAttackMs * 0.001 * sampleRate));
    const auto releaseTimeSamples = std::max (1.0f, static_cast<float> (lastReleaseMs * 0.001 * sampleRate));
    const auto holdTimeSamples = std::max (0, juce::roundToInt (lastHoldMs * 0.001 * sampleRate));

    // Ramp rates express "time to cross the full Range span", the standard
    // attack/release convention also used for compressor ballistics.
    const auto attackRatePerSample = (0.0f - rangeDbNow) / attackTimeSamples;
    const auto releaseRatePerSample = (0.0f - rangeDbNow) / releaseTimeSamples;

    const auto numChannelsToProcess = block.getNumChannels();

    // Knee: 0 dB reproduces the original hard-knee target exactly (openness
    // snaps between 0 and 1 with gateOpen); a wider knee blends openness
    // smoothly across a band centred on Threshold. Hold still overrides the
    // blend to guarantee a fully open target for its whole duration in both
    // cases - otherwise a dip into the knee band during Hold could sag the
    // gain, defeating Hold's purpose.
    const auto kneeDbNow = juce::jlimit (0.0f, maxKneeDb, lastKneeDb);
    const auto hardKnee = kneeDbNow < 0.0001f;
    const auto kneeLowerDb = lastThresholdDb - kneeDbNow * 0.5f;

    for (size_t sample = 0; sample < numSamples; ++sample)
    {
        const auto envelopeLinear = monoData[sample];
        const auto envelopeDb = juce::Decibels::gainToDecibels (envelopeLinear, minusInfinityDb);

        if (! gateOpen && envelopeDb >= lastThresholdDb)
        {
            gateOpen = true;
            holdCounterSamples = holdTimeSamples;
        }
        else if (gateOpen)
        {
            if (envelopeDb >= closeThresholdDb)
                holdCounterSamples = holdTimeSamples;
            else if (holdCounterSamples > 0)
                --holdCounterSamples;
            else
                gateOpen = false;
        }

        float openness; // 0 == fully closed target, 1 == fully open target

        if (hardKnee)
        {
            openness = gateOpen ? 1.0f : 0.0f;
        }
        else
        {
            const auto normalised = juce::jlimit (0.0f, 1.0f, (envelopeDb - kneeLowerDb) / kneeDbNow);
            openness = normalised * normalised * (3.0f - 2.0f * normalised); // smoothstep
        }

        // Hold guarantees a fully open target for its whole duration,
        // regardless of the knee curve's instantaneous value.
        if (gateOpen && holdCounterSamples > 0)
            openness = 1.0f;

        // Duck inverts the target: attenuate above Threshold instead of
        // opening above it, reusing the exact same detection/hysteresis/
        // hold/knee machinery above.
        if (duckingMode)
            openness = 1.0f - openness;

        const auto targetGainDb = juce::jmap (openness, rangeDbNow, 0.0f);

        if (targetGainDb > currentGainDb)
            currentGainDb = std::min (targetGainDb, currentGainDb + attackRatePerSample);
        else if (targetGainDb < currentGainDb)
            currentGainDb = std::max (targetGainDb, currentGainDb - releaseRatePerSample);

        const auto gainLinear = juce::Decibels::decibelsToGain (currentGainDb, minusInfinityDb);
        const auto detectionChannelCount = detectionSub.getNumChannels();

        for (size_t channel = 0; channel < numChannelsToProcess; ++channel)
        {
            auto* channelData = block.getChannelPointer (channel);
            lookaheadDelay.pushSample (static_cast<int> (channel), channelData[sample]);
            const auto delayed = lookaheadDelay.popSample (static_cast<int> (channel));

            if (listenMode)
            {
                // Listen bypasses the gain computer entirely: audition
                // exactly what the detector hears (post SC HPF, pre
                // envelope-follower), not the gated/ducked main signal.
                const auto detectionChannel = detectionChannelCount > 0
                                                   ? std::min (channel, detectionChannelCount - 1)
                                                   : channel;
                channelData[sample] = detectionChannelCount > 0
                                           ? detectionSub.getChannelPointer (detectionChannel)[sample]
                                           : 0.0f;
            }
            else
            {
                channelData[sample] = delayed * gainLinear;
            }
        }
    }
}
