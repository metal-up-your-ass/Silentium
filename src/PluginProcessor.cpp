#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "params/ParameterIds.h"
#include "params/ParameterLayout.h"

//==============================================================================
SilentiumAudioProcessor::SilentiumAudioProcessor()
    : AudioProcessor (BusesProperties()
                          .withInput ("Input", juce::AudioChannelSet::stereo(), true)
                          .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "PARAMETERS", createParameterLayout())
{
    thresholdDb = apvts.getRawParameterValue (ParamIDs::threshold);
    attackMs = apvts.getRawParameterValue (ParamIDs::attack);
    holdMs = apvts.getRawParameterValue (ParamIDs::hold);
    releaseMs = apvts.getRawParameterValue (ParamIDs::release);
    rangeDb = apvts.getRawParameterValue (ParamIDs::range);
    lookaheadMs = apvts.getRawParameterValue (ParamIDs::lookahead);
    scHighpassHz = apvts.getRawParameterValue (ParamIDs::scHighpass);

    jassert (thresholdDb != nullptr);
    jassert (attackMs != nullptr);
    jassert (holdMs != nullptr);
    jassert (releaseMs != nullptr);
    jassert (rangeDb != nullptr);
    jassert (lookaheadMs != nullptr);
    jassert (scHighpassHz != nullptr);
}

SilentiumAudioProcessor::~SilentiumAudioProcessor() = default;

//==============================================================================
juce::AudioProcessorValueTreeState::ParameterLayout SilentiumAudioProcessor::createParameterLayout()
{
    return slnt::createParameterLayout();
}

//==============================================================================
const juce::String SilentiumAudioProcessor::getName() const
{
    return JucePlugin_Name;
}

bool SilentiumAudioProcessor::acceptsMidi() const
{
    return false;
}

bool SilentiumAudioProcessor::producesMidi() const
{
    return false;
}

bool SilentiumAudioProcessor::isMidiEffect() const
{
    return false;
}

double SilentiumAudioProcessor::getTailLengthSeconds() const
{
    return 0.0;
}

int SilentiumAudioProcessor::getNumPrograms()
{
    return 1;
}

int SilentiumAudioProcessor::getCurrentProgram()
{
    return 0;
}

void SilentiumAudioProcessor::setCurrentProgram (int)
{
}

const juce::String SilentiumAudioProcessor::getProgramName (int)
{
    return {};
}

void SilentiumAudioProcessor::changeProgramName (int, const juce::String&)
{
}

//==============================================================================
void SilentiumAudioProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    juce::dsp::ProcessSpec spec;
    spec.sampleRate = sampleRate;
    spec.maximumBlockSize = static_cast<juce::uint32> (samplesPerBlock);
    spec.numChannels = static_cast<juce::uint32> (getTotalNumOutputChannels());

    // Seed the engine's parameters from the current APVTS state before
    // prepare() derives the lookahead delay/latency and primes the sidechain
    // filter coefficients, so the very first block after prepareToPlay()
    // already reflects the host/session's actual parameter values rather
    // than the engine's built-in defaults.
    engine.setThresholdDb (thresholdDb->load (std::memory_order_relaxed));
    engine.setAttackMs (attackMs->load (std::memory_order_relaxed));
    engine.setHoldMs (holdMs->load (std::memory_order_relaxed));
    engine.setReleaseMs (releaseMs->load (std::memory_order_relaxed));
    engine.setRangeDb (rangeDb->load (std::memory_order_relaxed));
    engine.setLookaheadMs (lookaheadMs->load (std::memory_order_relaxed));
    engine.setScHighpassHz (scHighpassHz->load (std::memory_order_relaxed));

    engine.prepare (spec);

    // Lookahead is the only source of the plugin's reported latency; the
    // main signal path is delayed internally by GateEngine (see
    // docs/architecture.md). Changing Lookahead live only takes effect on
    // the next prepareToPlay() (see GateEngine::getLatencySamples()).
    setLatencySamples (engine.getLatencySamples());
}

void SilentiumAudioProcessor::releaseResources()
{
}

void SilentiumAudioProcessor::reset()
{
    engine.reset();
}

bool SilentiumAudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto mono = juce::AudioChannelSet::mono();
    const auto stereo = juce::AudioChannelSet::stereo();

    const auto mainOut = layouts.getMainOutputChannelSet();
    const auto mainIn = layouts.getMainInputChannelSet();

    if (mainOut != mono && mainOut != stereo)
        return false;

    if (mainOut != mainIn)
        return false;

    return true;
}

void SilentiumAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;

    const auto totalNumInputChannels = getTotalNumInputChannels();
    const auto totalNumOutputChannels = getTotalNumOutputChannels();

    // Buses are constrained to in == out (mono or stereo), so this is
    // normally a no-op, but it's cheap insurance against stray channels.
    for (auto channel = totalNumInputChannels; channel < totalNumOutputChannels; ++channel)
        buffer.clear (channel, 0, buffer.getNumSamples());

    engine.setThresholdDb (thresholdDb->load (std::memory_order_relaxed));
    engine.setAttackMs (attackMs->load (std::memory_order_relaxed));
    engine.setHoldMs (holdMs->load (std::memory_order_relaxed));
    engine.setReleaseMs (releaseMs->load (std::memory_order_relaxed));
    engine.setRangeDb (rangeDb->load (std::memory_order_relaxed));
    engine.setLookaheadMs (lookaheadMs->load (std::memory_order_relaxed));
    engine.setScHighpassHz (scHighpassHz->load (std::memory_order_relaxed));

    juce::dsp::AudioBlock<float> block (buffer);
    engine.process (block);
}

//==============================================================================
bool SilentiumAudioProcessor::hasEditor() const
{
    return true;
}

juce::AudioProcessorEditor* SilentiumAudioProcessor::createEditor()
{
    return new SilentiumAudioProcessorEditor (*this);
}

//==============================================================================
void SilentiumAudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    const auto state = apvts.copyState();
    const std::unique_ptr<juce::XmlElement> xml (state.createXml());
    copyXmlToBinary (*xml, destData);
}

void SilentiumAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    const std::unique_ptr<juce::XmlElement> xmlState (getXmlFromBinary (data, sizeInBytes));

    if (xmlState != nullptr && xmlState->hasTagName (apvts.state.getType()))
        apvts.replaceState (juce::ValueTree::fromXml (*xmlState));
}

//==============================================================================
// This creates new instances of the plugin.
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new SilentiumAudioProcessor();
}
