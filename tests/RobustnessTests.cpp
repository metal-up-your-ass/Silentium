#include "PluginProcessor.h"
#include "params/ParameterIds.h"
#include "TestHelpers.h"

#include <catch2/catch_test_macros.hpp>

#include <limits>
#include <random>

namespace
{
    void setParam (SilentiumAudioProcessor& processor, const char* id, float realValue)
    {
        auto* param = processor.apvts.getParameter (id);
        REQUIRE (param != nullptr);
        param->setValueNotifyingHost (param->convertTo0to1 (realValue));
    }
}

TEST_CASE ("Silence produces silence (and no NaN/Inf)", "[robustness]")
{
    SilentiumAudioProcessor processor;
    processor.prepareToPlay (48000.0, 512);

    setParam (processor, ParamIDs::threshold, -40.0f);
    setParam (processor, ParamIDs::range, -60.0f);

    juce::AudioBuffer<float> buffer (2, 512);
    buffer.clear();

    juce::MidiBuffer midi;

    for (int i = 0; i < 8; ++i)
        CHECK_NOTHROW (processor.processBlock (buffer, midi));

    CHECK (TestHelpers::allSamplesFinite (buffer));
}

TEST_CASE ("Full-scale input at extreme gate settings produces no NaN/Inf", "[robustness]")
{
    SilentiumAudioProcessor processor;
    processor.prepareToPlay (48000.0, 512);

    setParam (processor, ParamIDs::threshold, -80.0f);
    setParam (processor, ParamIDs::attack, 0.1f);
    setParam (processor, ParamIDs::hold, 500.0f);
    setParam (processor, ParamIDs::release, 5.0f);
    setParam (processor, ParamIDs::range, -80.0f);
    setParam (processor, ParamIDs::lookahead, 20.0f);
    setParam (processor, ParamIDs::scHighpass, 500.0f);

    juce::AudioBuffer<float> buffer (2, 512);
    TestHelpers::fillWithSine (buffer, 48000.0, 1000.0, 1.0f);

    juce::MidiBuffer midi;

    for (int i = 0; i < 8; ++i)
        CHECK_NOTHROW (processor.processBlock (buffer, midi));

    CHECK (TestHelpers::allSamplesFinite (buffer));
    CHECK (TestHelpers::peakAbsolute (buffer) < 100.0f); // sane bound, not just "finite"
}

TEST_CASE ("Denormal-range input produces no NaN/Inf output", "[robustness]")
{
    SilentiumAudioProcessor processor;
    processor.prepareToPlay (48000.0, 512);

    setParam (processor, ParamIDs::threshold, -80.0f);

    constexpr int numSamples = 512;
    juce::AudioBuffer<float> buffer (2, numSamples);

    const auto denormalValue = std::numeric_limits<float>::denorm_min() * 4.0f;

    for (int channel = 0; channel < buffer.getNumChannels(); ++channel)
    {
        auto* data = buffer.getWritePointer (channel);

        for (int sample = 0; sample < numSamples; ++sample)
            data[sample] = (sample % 2 == 0) ? denormalValue : -denormalValue;
    }

    juce::MidiBuffer midi;

    for (int i = 0; i < 8; ++i)
        CHECK_NOTHROW (processor.processBlock (buffer, midi));

    CHECK (TestHelpers::allSamplesFinite (buffer));
}

TEST_CASE ("NaN/Inf input samples do not crash processBlock, and reset() lets clean audio recover", "[robustness][nan]")
{
    SilentiumAudioProcessor processor;
    processor.prepareToPlay (48000.0, 512);

    setParam (processor, ParamIDs::threshold, -40.0f);
    setParam (processor, ParamIDs::lookahead, 5.0f);

    constexpr int numSamples = 512;
    juce::AudioBuffer<float> buffer (2, numSamples);

    const auto nan = std::numeric_limits<float>::quiet_NaN();
    const auto inf = std::numeric_limits<float>::infinity();

    for (int channel = 0; channel < buffer.getNumChannels(); ++channel)
    {
        auto* data = buffer.getWritePointer (channel);

        for (int sample = 0; sample < numSamples; ++sample)
        {
            if (sample % 3 == 0)
                data[sample] = nan;
            else if (sample % 3 == 1)
                data[sample] = inf;
            else
                data[sample] = 0.1f;
        }
    }

    juce::MidiBuffer midi;

    // Feeding pathological input is not something the gate is expected to
    // sanitise on its own (that is the host's job) - the contract under
    // test is that it must not crash, hang, or throw.
    CHECK_NOTHROW (processor.processBlock (buffer, midi));

    // reset() clears every filter/envelope/delay-line's internal state
    // (including the lookahead delay line's buffered samples), so any
    // NaN/Inf poisoned into the lookahead buffer above must not leak into
    // subsequent clean audio.
    CHECK_NOTHROW (processor.reset());

    TestHelpers::fillWithSine (buffer, 48000.0, 1000.0, 0.5f);

    // Process enough blocks to fully flush the lookahead delay line's
    // capacity with clean samples.
    for (int i = 0; i < 4; ++i)
        CHECK_NOTHROW (processor.processBlock (buffer, midi));

    CHECK (TestHelpers::allSamplesFinite (buffer));
}

TEST_CASE ("Zero-sample buffer does not crash processBlock", "[robustness]")
{
    SilentiumAudioProcessor processor;
    processor.prepareToPlay (48000.0, 512);

    juce::AudioBuffer<float> buffer (2, 0);
    juce::MidiBuffer midi;

    CHECK_NOTHROW (processor.processBlock (buffer, midi));
    CHECK (buffer.getNumSamples() == 0);
}

TEST_CASE ("Extreme parameter values at both range edges produce no NaN/Inf", "[robustness]")
{
    SilentiumAudioProcessor processor;
    processor.prepareToPlay (44100.0, 256);

    juce::AudioBuffer<float> buffer (2, 256);
    juce::MidiBuffer midi;

    for (bool useMinimum : { true, false })
    {
        setParam (processor, ParamIDs::threshold, useMinimum ? -80.0f : 0.0f);
        setParam (processor, ParamIDs::attack, useMinimum ? 0.1f : 50.0f);
        setParam (processor, ParamIDs::hold, useMinimum ? 0.0f : 500.0f);
        setParam (processor, ParamIDs::release, useMinimum ? 5.0f : 500.0f);
        setParam (processor, ParamIDs::range, useMinimum ? -80.0f : 0.0f);
        setParam (processor, ParamIDs::lookahead, useMinimum ? 0.0f : 20.0f);
        setParam (processor, ParamIDs::scHighpass, useMinimum ? 20.0f : 500.0f);

        TestHelpers::fillWithSine (buffer, 44100.0, 440.0, 0.8f);

        CHECK_NOTHROW (processor.processBlock (buffer, midi));
        CHECK (TestHelpers::allSamplesFinite (buffer));
    }
}

TEST_CASE ("Rapid parameter automation across many blocks produces no NaN/Inf", "[robustness]")
{
    SilentiumAudioProcessor processor;
    processor.prepareToPlay (48000.0, 256);

    std::mt19937 rng (1234);
    std::uniform_real_distribution<float> unit (0.0f, 1.0f);

    juce::MidiBuffer midi;

    for (int block = 0; block < 100; ++block)
    {
        setParam (processor, ParamIDs::threshold, -80.0f + unit (rng) * 80.0f);
        setParam (processor, ParamIDs::attack, 0.1f + unit (rng) * 49.9f);
        setParam (processor, ParamIDs::hold, unit (rng) * 500.0f);
        setParam (processor, ParamIDs::release, 5.0f + unit (rng) * 495.0f);
        setParam (processor, ParamIDs::range, -80.0f + unit (rng) * 80.0f);
        setParam (processor, ParamIDs::lookahead, unit (rng) * 20.0f);
        setParam (processor, ParamIDs::scHighpass, 20.0f + unit (rng) * 480.0f);

        juce::AudioBuffer<float> buffer (2, 256);
        TestHelpers::fillWithSine (buffer, 48000.0, 200.0 + unit (rng) * 4000.0, 0.7f);

        CHECK_NOTHROW (processor.processBlock (buffer, midi));
        CHECK (TestHelpers::allSamplesFinite (buffer));
    }
}

TEST_CASE ("reset() followed by processBlock does not crash", "[robustness]")
{
    SilentiumAudioProcessor processor;
    processor.prepareToPlay (48000.0, 512);

    setParam (processor, ParamIDs::threshold, -30.0f);

    juce::AudioBuffer<float> buffer (2, 512);
    TestHelpers::fillWithSine (buffer, 48000.0, 1000.0, 0.6f);
    juce::MidiBuffer midi;

    processor.processBlock (buffer, midi);

    CHECK_NOTHROW (processor.reset());

    TestHelpers::fillWithSine (buffer, 48000.0, 1000.0, 0.6f);
    CHECK_NOTHROW (processor.processBlock (buffer, midi));
    CHECK (TestHelpers::allSamplesFinite (buffer));
}
