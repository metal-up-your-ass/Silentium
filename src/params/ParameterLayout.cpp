#include "ParameterLayout.h"
#include "ParameterIds.h"

namespace
{
    // True logarithmic (base-10) mapping for frequency parameters, so slider/
    // knob travel spends equal space per octave rather than per Hz. Uses
    // juce::mapToLog10/mapFromLog10 rather than NormalisableRange's built-in
    // power-law skew, which only approximates a log curve.
    juce::NormalisableRange<float> makeLogFrequencyRange (float minHz, float maxHz)
    {
        return juce::NormalisableRange<float> (
            minHz,
            maxHz,
            [] (float rangeStart, float rangeEnd, float normalised)
            { return juce::mapToLog10 (normalised, rangeStart, rangeEnd); },
            [] (float rangeStart, float rangeEnd, float value)
            { return juce::mapFromLog10 (value, rangeStart, rangeEnd); });
    }

    // Same idea for time parameters (Attack/Release) that span a wide,
    // perceptually-logarithmic range: equal slider travel per decade, not per
    // millisecond. Not used for Hold/Lookahead, whose range must include 0.
    juce::NormalisableRange<float> makeLogTimeRange (float minMs, float maxMs)
    {
        return juce::NormalisableRange<float> (
            minMs,
            maxMs,
            [] (float rangeStart, float rangeEnd, float normalised)
            { return juce::mapToLog10 (normalised, rangeStart, rangeEnd); },
            [] (float rangeStart, float rangeEnd, float value)
            { return juce::mapFromLog10 (value, rangeStart, rangeEnd); });
    }
}

namespace slnt
{
    juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout()
    {
        juce::AudioProcessorValueTreeState::ParameterLayout layout;

        //======================================================================
        // Threshold: open threshold on the (sidechain-filtered) envelope.
        layout.add (std::make_unique<juce::AudioParameterFloat> (
            juce::ParameterID { ParamIDs::threshold, 1 },
            "Threshold",
            juce::NormalisableRange<float> (-80.0f, 0.0f, 0.01f),
            -40.0f,
            juce::AudioParameterFloatAttributes().withLabel ("dB")));

        //======================================================================
        // Attack: ramp time from closed (Range) to open (0 dB).
        layout.add (std::make_unique<juce::AudioParameterFloat> (
            juce::ParameterID { ParamIDs::attack, 1 },
            "Attack",
            makeLogTimeRange (0.1f, 50.0f),
            1.0f,
            juce::AudioParameterFloatAttributes().withLabel ("ms")));

        //======================================================================
        // Hold: minimum time the gate stays open, retriggered while the
        // envelope stays above the close threshold. Range must include 0, so
        // this uses a plain linear range rather than makeLogTimeRange.
        layout.add (std::make_unique<juce::AudioParameterFloat> (
            juce::ParameterID { ParamIDs::hold, 1 },
            "Hold",
            juce::NormalisableRange<float> (0.0f, 500.0f, 0.1f),
            20.0f,
            juce::AudioParameterFloatAttributes().withLabel ("ms")));

        //======================================================================
        // Release: ramp time from open (0 dB) back down to Range.
        layout.add (std::make_unique<juce::AudioParameterFloat> (
            juce::ParameterID { ParamIDs::release, 1 },
            "Release",
            makeLogTimeRange (5.0f, 500.0f),
            80.0f,
            juce::AudioParameterFloatAttributes().withLabel ("ms")));

        //======================================================================
        // Range: floor attenuation applied when closed. 0 dB == always open
        // (a useful reference/bypass-like setting, see tests/EngineTests.cpp).
        layout.add (std::make_unique<juce::AudioParameterFloat> (
            juce::ParameterID { ParamIDs::range, 1 },
            "Range",
            juce::NormalisableRange<float> (-80.0f, 0.0f, 0.01f),
            -60.0f,
            juce::AudioParameterFloatAttributes().withLabel ("dB")));

        //======================================================================
        // Lookahead: delays the main signal so the gate can start opening
        // just before a transient arrives. Reported as the plugin's total
        // latency. Range must include 0 (lookahead disabled), so this uses a
        // plain linear range rather than makeLogTimeRange.
        layout.add (std::make_unique<juce::AudioParameterFloat> (
            juce::ParameterID { ParamIDs::lookahead, 1 },
            "Lookahead",
            juce::NormalisableRange<float> (0.0f, 20.0f, 0.01f),
            5.0f,
            juce::AudioParameterFloatAttributes().withLabel ("ms")));

        //======================================================================
        // SC HPF: sidechain-only high-pass, keeps hum/rumble from falsely
        // holding the gate open. Never applied to the main signal.
        layout.add (std::make_unique<juce::AudioParameterFloat> (
            juce::ParameterID { ParamIDs::scHighpass, 1 },
            "SC HPF",
            makeLogFrequencyRange (20.0f, 500.0f),
            80.0f,
            juce::AudioParameterFloatAttributes().withLabel ("Hz")));

        return layout;
    }
}
