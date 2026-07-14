#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_dsp/juce_dsp.h>

#include "dsp/GateEngine.h"

// Silentium: a tight lookahead noise gate with hysteresis, for silencing amp
// hiss/hum between palm-muted chugs. Signal flow lives in GateEngine
// (src/dsp) so it stays unit-testable independent of this AudioProcessor;
// this class is just APVTS + host plumbing around it.
class SilentiumAudioProcessor final : public juce::AudioProcessor
{
public:
    SilentiumAudioProcessor();
    ~SilentiumAudioProcessor() override;

    //==============================================================================
    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;
    void reset() override;

    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;

    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    //==============================================================================
    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override;

    //==============================================================================
    const juce::String getName() const override;

    bool acceptsMidi() const override;
    bool producesMidi() const override;
    bool isMidiEffect() const override;
    double getTailLengthSeconds() const override;

    //==============================================================================
    int getNumPrograms() override;
    int getCurrentProgram() override;
    void setCurrentProgram (int index) override;
    const juce::String getProgramName (int index) override;
    void changeProgramName (int index, const juce::String& newName) override;

    //==============================================================================
    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    static juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();

    juce::AudioProcessorValueTreeState apvts;

private:
    GateEngine engine;

    // Raw atomic pointers into the APVTS-managed parameter values, resolved
    // once at construction time so processBlock() never has to search for
    // them (no allocation/locks on the audio thread).
    std::atomic<float>* thresholdDb = nullptr;
    std::atomic<float>* attackMs = nullptr;
    std::atomic<float>* holdMs = nullptr;
    std::atomic<float>* releaseMs = nullptr;
    std::atomic<float>* rangeDb = nullptr;
    std::atomic<float>* lookaheadMs = nullptr;
    std::atomic<float>* scHighpassHz = nullptr;
    std::atomic<float>* kneeDb = nullptr;
    std::atomic<float>* duckMode = nullptr;
    std::atomic<float>* listenMode = nullptr;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SilentiumAudioProcessor)
};
