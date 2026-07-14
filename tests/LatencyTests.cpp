#include "PluginProcessor.h"
#include "params/ParameterIds.h"
#include "dsp/GateEngine.h"

#include <catch2/catch_test_macros.hpp>

namespace
{
    void setParam (SilentiumAudioProcessor& processor, const char* id, float realValue)
    {
        auto* param = processor.apvts.getParameter (id);
        REQUIRE (param != nullptr);
        param->setValueNotifyingHost (param->convertTo0to1 (realValue));
    }
}

TEST_CASE ("getLatencySamples() reports the lookahead delay after prepareToPlay", "[latency]")
{
    SilentiumAudioProcessor processor;

    // Before prepareToPlay, no engine has been prepared yet - JUCE's default
    // AudioProcessor latency is 0.
    CHECK (processor.getLatencySamples() == 0);

    processor.prepareToPlay (48000.0, 512);

    // Cross-check against a standalone engine prepared identically with the
    // Lookahead parameter's default (5 ms): the processor must report
    // exactly what GateEngine computes, not an approximation of it.
    GateEngine referenceEngine;
    referenceEngine.setLookaheadMs (5.0f);

    juce::dsp::ProcessSpec spec;
    spec.sampleRate = 48000.0;
    spec.maximumBlockSize = 512;
    spec.numChannels = 2;
    referenceEngine.prepare (spec);

    CHECK (processor.getLatencySamples() == referenceEngine.getLatencySamples());
    CHECK (processor.getLatencySamples() == 240); // 5 ms @ 48 kHz, exact integer
}

TEST_CASE ("Latency tracks a non-default Lookahead value on the next prepareToPlay", "[latency]")
{
    SilentiumAudioProcessor processor;
    processor.prepareToPlay (48000.0, 512);

    setParam (processor, ParamIDs::lookahead, 10.0f);

    // Lookahead is treated as a structural parameter (see
    // GateEngine::getLatencySamples()): the new value only takes effect on
    // the next prepareToPlay(), not immediately.
    processor.prepareToPlay (48000.0, 512);

    CHECK (processor.getLatencySamples() == 480); // 10 ms @ 48 kHz
}

TEST_CASE ("Latency is stable across repeated prepareToPlay calls at the same sample rate", "[latency]")
{
    SilentiumAudioProcessor processor;

    processor.prepareToPlay (44100.0, 256);
    const auto firstLatency = processor.getLatencySamples();

    processor.prepareToPlay (44100.0, 256);
    const auto secondLatency = processor.getLatencySamples();

    CHECK (firstLatency == secondLatency);
}

TEST_CASE ("Latency scales with sample rate for a fixed Lookahead", "[latency]")
{
    SilentiumAudioProcessor processor;

    processor.prepareToPlay (44100.0, 512);
    const auto latencyAt44k = processor.getLatencySamples();

    processor.prepareToPlay (96000.0, 512);
    const auto latencyAt96k = processor.getLatencySamples();

    CHECK (latencyAt44k > 0);
    CHECK (latencyAt96k > latencyAt44k); // same lookahead in ms, more samples at a higher rate
}
