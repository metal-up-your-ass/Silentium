#include "dsp/GateEngine.h"
#include "TestHelpers.h"

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <cmath>

namespace
{
    constexpr double testSampleRate = 48000.0;
    constexpr int testBlockSize = 4096;
    constexpr double testFrequencyHz = 1000.0;

    juce::dsp::ProcessSpec makeTestSpec (int numChannels, int blockSize = testBlockSize)
    {
        juce::dsp::ProcessSpec spec;
        spec.sampleRate = testSampleRate;
        spec.maximumBlockSize = static_cast<juce::uint32> (blockSize);
        spec.numChannels = static_cast<juce::uint32> (numChannels);
        return spec;
    }
}

TEST_CASE ("Gate-open passthrough: Threshold at minimum + Range 0 dB is unity apart from lookahead delay", "[dsp][gate][null]")
{
    GateEngine engine;

    // Range = 0 dB means the gain computer's target is 0 dB whether the
    // gate is open or closed - the gate can therefore never attenuate, and
    // the whole signal path reduces to a pure delay. This is the correct
    // "always open" reference passthrough case (Threshold is set to its
    // minimum too, so the gate is also genuinely open throughout, not just
    // coincidentally unattenuated).
    engine.setThresholdDb (-80.0f);
    engine.setRangeDb (0.0f);
    engine.setAttackMs (1.0f);
    engine.setHoldMs (20.0f);
    engine.setReleaseMs (80.0f);
    engine.setLookaheadMs (5.0f);
    engine.setScHighpassHz (20.0f);

    const auto spec = makeTestSpec (2);
    engine.prepare (spec);

    const auto latency = engine.getLatencySamples();
    REQUIRE (latency > 0);
    REQUIRE (latency < testBlockSize / 2);

    juce::AudioBuffer<float> reference (2, testBlockSize);
    TestHelpers::fillWithSine (reference, testSampleRate, testFrequencyHz, 0.5f);

    juce::AudioBuffer<float> processed;
    processed.makeCopyOf (reference);

    juce::dsp::AudioBlock<float> block (processed);
    engine.process (block);

    const auto overlapLength = testBlockSize - latency;
    REQUIRE (overlapLength > testBlockSize / 2);

    constexpr float tolerance = 1.0e-5f;

    for (int channel = 0; channel < reference.getNumChannels(); ++channel)
    {
        const auto* refData = reference.getReadPointer (channel);
        const auto* outData = processed.getReadPointer (channel);

        float maxResidual = 0.0f;

        for (int i = 0; i < overlapLength; ++i)
            maxResidual = std::max (maxResidual, std::abs (outData[latency + i] - refData[i]));

        CHECK (maxResidual < tolerance);
    }

    CHECK (engine.isGateOpen());
}

TEST_CASE ("Below-threshold signal is attenuated toward the Range floor", "[dsp][gate]")
{
    GateEngine engine;

    engine.setThresholdDb (-10.0f);
    engine.setRangeDb (-60.0f);
    engine.setAttackMs (1.0f);
    engine.setHoldMs (0.0f);
    engine.setReleaseMs (30.0f);
    // Isolate the gain-computer amplitude check from the lookahead delay.
    engine.setLookaheadMs (0.0f);
    engine.setScHighpassHz (20.0f);

    const auto spec = makeTestSpec (2);
    engine.prepare (spec);

    // -40 dBFS peak: comfortably below both the open threshold (-10 dB) and
    // the close threshold (-13 dB, Threshold - hysteresis), so the gate
    // never opens and the gain computer sits at the Range floor throughout.
    constexpr float inputAmplitude = 0.01f; // -40 dBFS
    juce::AudioBuffer<float> buffer (2, testBlockSize);
    TestHelpers::fillWithSine (buffer, testSampleRate, testFrequencyHz, inputAmplitude);

    juce::dsp::AudioBlock<float> block (buffer);
    engine.process (block);

    CHECK_FALSE (engine.isGateOpen());

    const auto expectedFloorGain = juce::Decibels::decibelsToGain (-60.0f);
    const auto expectedPeak = inputAmplitude * expectedFloorGain;

    // Measure only the back half of the buffer, safely past the release
    // ramp's settling time (30 ms release is a tiny fraction of the buffer
    // length at 48 kHz).
    const auto measureStart = testBlockSize / 2;

    for (int channel = 0; channel < buffer.getNumChannels(); ++channel)
    {
        float peak = 0.0f;
        const auto* data = buffer.getReadPointer (channel);

        for (int i = measureStart; i < testBlockSize; ++i)
            peak = std::max (peak, std::abs (data[i]));

        CHECK (peak == Catch::Approx (expectedPeak).margin (expectedPeak * 0.05f + 1.0e-6f));
    }
}

TEST_CASE ("Hysteresis: the gate's dead band prevents chatter", "[dsp][gate][hysteresis]")
{
    constexpr float thresholdDb = -20.0f;
    constexpr float loudDb = -10.0f;    // well above Threshold: opens the gate
    constexpr float betweenDb = -21.5f; // below Threshold, above Threshold - hysteresis (-23 dB): the dead band

    const auto loudAmplitude = juce::Decibels::decibelsToGain (loudDb);
    const auto betweenAmplitude = juce::Decibels::decibelsToGain (betweenDb);

    auto configureEngine = [] (GateEngine& engine)
    {
        engine.setThresholdDb (thresholdDb);
        engine.setRangeDb (-60.0f);
        engine.setAttackMs (1.0f);
        engine.setHoldMs (0.0f);
        engine.setReleaseMs (30.0f);
        engine.setLookaheadMs (0.0f);
        engine.setScHighpassHz (20.0f);
        engine.prepare (makeTestSpec (2, 512));
    };

    auto processChunks = [] (GateEngine& engine, float amplitude, int numChunks, int& openTransitions, int& closeTransitions, bool& previousState)
    {
        for (int c = 0; c < numChunks; ++c)
        {
            juce::AudioBuffer<float> buffer (2, 512);
            TestHelpers::fillWithSine (buffer, testSampleRate, testFrequencyHz, amplitude);

            juce::dsp::AudioBlock<float> block (buffer);
            engine.process (block);

            const auto currentState = engine.isGateOpen();
            if (currentState && ! previousState)
                ++openTransitions;
            if (! currentState && previousState)
                ++closeTransitions;
            previousState = currentState;
        }
    };

    SECTION ("gate opened while loud stays open once it drops into the dead band, with zero chatter")
    {
        GateEngine engine;
        configureEngine (engine);

        int openTransitions = 0;
        int closeTransitions = 0;
        bool previousState = engine.isGateOpen();

        // Phase 1: loud signal opens the gate (~213 ms, well past the 1 ms attack).
        processChunks (engine, loudAmplitude, 20, openTransitions, closeTransitions, previousState);
        REQUIRE (engine.isGateOpen());

        // Phase 2: drop into the dead band. A single-threshold comparator
        // would close here (betweenDb < thresholdDb); hysteresis must keep
        // the gate open with zero chatter because betweenDb is still above
        // the close threshold (thresholdDb - hysteresis).
        processChunks (engine, betweenAmplitude, 20, openTransitions, closeTransitions, previousState);
        CHECK (engine.isGateOpen());

        CHECK (openTransitions == 1);
        CHECK (closeTransitions == 0);
    }

    SECTION ("the dead-band level alone, from a closed start, never opens the gate")
    {
        GateEngine engine;
        configureEngine (engine);

        REQUIRE_FALSE (engine.isGateOpen());

        int openTransitions = 0;
        int closeTransitions = 0;
        bool previousState = engine.isGateOpen();

        processChunks (engine, betweenAmplitude, 20, openTransitions, closeTransitions, previousState);

        CHECK_FALSE (engine.isGateOpen());
        CHECK (openTransitions == 0);
    }
}

TEST_CASE ("Engine reset() clears filter/envelope/delay-line state without crashing", "[dsp][gate]")
{
    GateEngine engine;
    engine.setThresholdDb (-30.0f);
    engine.setRangeDb (-60.0f);
    engine.setLookaheadMs (5.0f);

    const auto spec = makeTestSpec (2);
    engine.prepare (spec);

    juce::AudioBuffer<float> buffer (2, testBlockSize);
    TestHelpers::fillWithSine (buffer, testSampleRate, testFrequencyHz, 0.9f);

    juce::dsp::AudioBlock<float> block (buffer);
    engine.process (block);

    CHECK_NOTHROW (engine.reset());
    CHECK (TestHelpers::allSamplesFinite (buffer));

    TestHelpers::fillWithSine (buffer, testSampleRate, testFrequencyHz, 0.9f);
    CHECK_NOTHROW (engine.process (block));
    CHECK (TestHelpers::allSamplesFinite (buffer));
}

TEST_CASE ("Zero-sample block is a safe no-op", "[dsp][gate]")
{
    GateEngine engine;
    engine.setLookaheadMs (5.0f);

    const auto spec = makeTestSpec (2);
    engine.prepare (spec);

    juce::AudioBuffer<float> buffer (2, 0);
    juce::dsp::AudioBlock<float> block (buffer);

    CHECK_NOTHROW (engine.process (block));
}
