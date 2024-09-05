/*
  ==============================================================================

    This file contains the basic framework code for a JUCE plugin processor.

  ==============================================================================
*/

#include "PluginProcessor.h"
#include "PluginEditor.h"

template<typename T>
inline static void castParameter(juce::AudioProcessorValueTreeState& apvts, const juce::ParameterID& id, T& destination)
{
    destination = dynamic_cast<T>(apvts.getParameter(id.getParamID()));
    jassert(destination);
}

//==============================================================================
SubSynthAudioProcessor::SubSynthAudioProcessor()
#ifndef JucePlugin_PreferredChannelConfigurations
     : AudioProcessor (BusesProperties()
                     #if ! JucePlugin_IsMidiEffect
                      #if ! JucePlugin_IsSynth
                       .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
                      #endif
                       .withOutput ("Output", juce::AudioChannelSet::stereo(), true)
                     #endif
                       )
#endif
{
    castParameter(apvts, ParameterID::oscMix, oscMixParam);
    castParameter(apvts, ParameterID::oscTune, oscTuneParam);
    castParameter(apvts, ParameterID::oscFine, oscFineParam);
    castParameter(apvts, ParameterID::filterFreq, filterFreqParam);
    castParameter(apvts, ParameterID::filterReso, filterResoParam);
    castParameter(apvts, ParameterID::filterEnv, filterEnvParam);
    castParameter(apvts, ParameterID::filterLFO, filterLFOParam);
    castParameter(apvts, ParameterID::filterVelocity, filterVelocityParam);
    castParameter(apvts, ParameterID::filterAttack, filterAttackParam);
    castParameter(apvts, ParameterID::filterDecay, filterDecayParam);
    castParameter(apvts, ParameterID::filterSustain, filterSustainParam);
    castParameter(apvts, ParameterID::filterRelease, filterReleaseParam);
    castParameter(apvts, ParameterID::envAttack, envAttackParam);
    castParameter(apvts, ParameterID::envDecay, envDecayParam);
    castParameter(apvts, ParameterID::envSustain, envSustainParam);
    castParameter(apvts, ParameterID::envRelease, envReleaseParam);
    castParameter(apvts, ParameterID::lfoRate, lfoRateParam);
    castParameter(apvts, ParameterID::vibrato, vibratoParam);
    castParameter(apvts, ParameterID::noise, noiseParam);
    castParameter(apvts, ParameterID::octave, octaveParam);
    castParameter(apvts, ParameterID::tuning, tuningParam);
    castParameter(apvts, ParameterID::outputLevel, outputLevelParam);

    apvts.state.addListener(this);
}

SubSynthAudioProcessor::~SubSynthAudioProcessor()
{
    apvts.state.removeListener(this);
}

//==============================================================================
const juce::String SubSynthAudioProcessor::getName() const
{
    return JucePlugin_Name;
}

bool SubSynthAudioProcessor::acceptsMidi() const
{
   #if JucePlugin_WantsMidiInput
    return true;
   #else
    return false;
   #endif
}

bool SubSynthAudioProcessor::producesMidi() const
{
   #if JucePlugin_ProducesMidiOutput
    return true;
   #else
    return false;
   #endif
}

bool SubSynthAudioProcessor::isMidiEffect() const
{
   #if JucePlugin_IsMidiEffect
    return true;
   #else
    return false;
   #endif
}

double SubSynthAudioProcessor::getTailLengthSeconds() const
{
    return 0.0;
}

int SubSynthAudioProcessor::getNumPrograms()
{
    return 1;   // NB: some hosts don't cope very well if you tell them there are 0 programs,
                // so this should be at least 1, even if you're not really implementing programs.
}

int SubSynthAudioProcessor::getCurrentProgram()
{
    return 0;
}

void SubSynthAudioProcessor::setCurrentProgram (int /*index*/)
{
}

const juce::String SubSynthAudioProcessor::getProgramName (int /*index*/)
{
    return {};
}

void SubSynthAudioProcessor::changeProgramName (int /*index*/, const juce::String& /*newName*/)
{
}

//==============================================================================
void SubSynthAudioProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    // Use this method as the place to do any pre-playback
    // initialisation that you need..
    synth.allocateResources(sampleRate, samplesPerBlock);
    parametersChanged.store(true);
    this->reset();
}

void SubSynthAudioProcessor::reset()
{
    synth.reset();
}

void SubSynthAudioProcessor::releaseResources()
{
    // When playback stops, you can use this as an opportunity to free up any
    // spare memory, etc.
    synth.deallocateResources();
}

#ifndef JucePlugin_PreferredChannelConfigurations
bool SubSynthAudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
  #if JucePlugin_IsMidiEffect
    juce::ignoreUnused (layouts);
    return true;
  #else
    // This is the place where you check if the layout is supported.
    // In this template code we only support mono or stereo.
    // Some plugin hosts, such as certain GarageBand versions, will only
    // load plugins that support stereo bus layouts.
    if (layouts.getMainOutputChannelSet() != juce::AudioChannelSet::mono()
     && layouts.getMainOutputChannelSet() != juce::AudioChannelSet::stereo())
        return false;

    // This checks if the input layout matches the output layout
   #if ! JucePlugin_IsSynth
    if (layouts.getMainOutputChannelSet() != layouts.getMainInputChannelSet())
        return false;
   #endif

    return true;
  #endif
}
#endif

void SubSynthAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages)
{
    juce::ScopedNoDenormals noDenormals;
    auto totalNumInputChannels  = getTotalNumInputChannels();
    auto totalNumOutputChannels = getTotalNumOutputChannels();

    for (auto i = totalNumInputChannels; i < totalNumOutputChannels; ++i) 
    {
        buffer.clear(i, 0, buffer.getNumSamples());
    }
    
    bool expected = true;
    if (isNonRealtime() || parametersChanged.compare_exchange_strong(expected, false)) {
        update();
    }
    
    splitBuffer(buffer, midiMessages);
}

void SubSynthAudioProcessor::update()
{
    float sampleRate = float(getSampleRate());
    float inverseSampleRate = 1.0f / sampleRate;

    synth.envAttack = std::exp(-inverseSampleRate * std::exp(5.5f - 0.075f * envAttackParam->get()));
    synth.envDecay = std::exp(-inverseSampleRate * std::exp(5.5f - 0.075f * envDecayParam->get()));

    synth.envSustain = envSustainParam->get() / 100.0f;

    float envRelease = envReleaseParam->get();
    if (envRelease < 1.0f) {
        synth.envRelease = 0.75f;
    } else {
        synth.envRelease = std::exp(-inverseSampleRate * std::exp(5.5f - 0.075f * envRelease));
    }

    float noiseMix = noiseParam->get() / 100.0f;
    noiseMix *= noiseMix;
    synth.noiseMix = noiseMix * 0.1f;
    
    synth.oscMixSmoother.setTargetValue(oscMixParam->get() / 100.0f);
    
    float semi = oscTuneParam->get();
    float cent = oscFineParam->get() * 0.01f;
    synth.oscBTune = std::pow(1.059463094359f, semi + cent);

    float octave = octaveParam->get();
    float tuning = tuningParam->get();
    synth.masterTune = (octave * 12.0f) + (tuning / 100.0f);
    
    synth.outputLevelSmoother.setTargetValue(juce::Decibels::decibelsToGain(outputLevelParam->get()));
    
    float filterVelocity = filterVelocityParam->get(); 
    if (filterVelocity < -90.0f)
    {
        synth.velocitySensitivity = 0.0f;
        synth.ignoreVelocity = true;
    }
    else
    {
        synth.velocitySensitivity = 0.0005f * filterVelocity;
        synth.ignoreVelocity = false;
    }
    
    const float inverseUpdateRate = inverseSampleRate * synth.LFO_MAX;
    float lfoRate = std::exp(7.0f * lfoRateParam->get() - 4.0f);
    synth.lfoInc = lfoRate * inverseUpdateRate * float(TWO_PI);
    
    float vibrato = vibratoParam->get() / 200.0f;
    synth.vibrato = 0.2f * vibrato * vibrato;
    
    synth.pwmDepth = synth.vibrato;
    if (vibrato < 0.0f)
    { 
        synth.vibrato = 0.0f;
    }
    
    synth.filterKeyTracking = 0.08f * filterFreqParam->get() - 1.5f;
    
    float filterReso = filterResoParam->get() / 100.0f;
    synth.filterQ = std::exp(3.0f * filterReso);
    
    
    float filterLFO = filterLFOParam->get() / 100.0f;
    synth.filterLFODepth = 2.5f * filterLFO * filterLFO;
    
    synth.filterAttack = std::exp(-inverseUpdateRate * std::exp(5.5f - 0.075f * filterAttackParam->get()));
    synth.filterDecay = std::exp(-inverseUpdateRate * std::exp(5.5f - 0.075f * filterDecayParam->get()));
    float filterSustain = filterSustainParam->get() / 100.0f;
    synth.filterSustain = filterSustain * filterSustain;
    synth.filterRelease = std::exp(-inverseUpdateRate * std::exp(5.5f - 0.075f * filterReleaseParam->get()));
    
    synth.filterEnvDepth = 0.06f * filterEnvParam->get();
}

void SubSynthAudioProcessor::splitBuffer(juce::AudioBuffer<float> &buffer, juce::MidiBuffer &midiMessages)
{
    int bufferOffset = 0;
    
    for (const auto message : midiMessages) 
    {
        int noOfSamplesTillMessage = message.samplePosition - bufferOffset;
        
        if (noOfSamplesTillMessage > 0)
        {
            render(buffer, noOfSamplesTillMessage, bufferOffset);
            bufferOffset += noOfSamplesTillMessage;
        }
        
        if (message.numBytes <= 3)
        {
            uint8_t data1 = message.numBytes >= 2 ? message.data[1] : 0;
            uint8_t data2 = message.numBytes == 3 ? message.data[2] : 0;
            handleMidi(message.data[0], data1, data2);
        }
    }
    
    int noOfFinalSamples = buffer.getNumSamples() - bufferOffset;
    if (noOfFinalSamples > 0)
    {
        render(buffer, noOfFinalSamples, bufferOffset);
    }
    
    midiMessages.clear();
}

void SubSynthAudioProcessor::handleMidi(uint8_t data0, uint8_t data1, uint8_t data2)
{
    synth.midiMessage(data0, data1, data2);
}

void SubSynthAudioProcessor::render(juce::AudioBuffer<float> &buffer, int sampleCount, int bufferOffset)
{
    synth.render(buffer, bufferOffset, sampleCount, getTotalNumOutputChannels());
}

//==============================================================================
bool SubSynthAudioProcessor::hasEditor() const
{
    return true; // (change this to false if you choose to not supply an editor)
}

juce::AudioProcessorEditor* SubSynthAudioProcessor::createEditor()
{
    auto editor = new juce::GenericAudioProcessorEditor(*this);
    editor->setSize(500, 500);
    return editor;
}
//==============================================================================
void SubSynthAudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    copyXmlToBinary(*apvts.copyState().createXml(), destData);
}

void SubSynthAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    std::unique_ptr<juce::XmlElement> xml(getXmlFromBinary(data, sizeInBytes));
    if (xml.get() != nullptr && xml->hasTagName(apvts.state.getType())) {
        apvts.replaceState(juce::ValueTree::fromXml(*xml));
        parametersChanged.store(true);
    }
}

juce::AudioProcessorValueTreeState::ParameterLayout SubSynthAudioProcessor::createParameterLayout()
{
    juce::AudioProcessorValueTreeState::ParameterLayout layout;

    layout.add(std::make_unique<juce::AudioParameterFloat>(
        ParameterID::oscTune,
        "Osc Tune",
        juce::NormalisableRange<float>(-24.0f, 24.0f, 1.0f),
        -12.0f,
        juce::AudioParameterFloatAttributes().withLabel("semi")));

    layout.add(std::make_unique<juce::AudioParameterFloat>(
        ParameterID::oscFine,
        "Osc Fine",
        juce::NormalisableRange<float>(-50.0f, 50.0f, 0.1f, 0.3f, true),
        0.0f,
        juce::AudioParameterFloatAttributes().withLabel("cent")));

    auto oscMixStringFromValue = [](float value, int)
    {
        char s[16] = { 0 };
        snprintf(s, 16, "%4.0f:%2.0f", 100.0 - 0.5f * value, 0.5f * value);
        return juce::String(s);
    };

    layout.add(std::make_unique<juce::AudioParameterFloat>(
        ParameterID::oscMix,
        "Osc Mix",
        juce::NormalisableRange<float>(0.0f, 100.0f),
        0.0f,
        juce::AudioParameterFloatAttributes()
                .withLabel("%")
                .withStringFromValueFunction(oscMixStringFromValue)));

    layout.add(std::make_unique<juce::AudioParameterFloat>(
        ParameterID::filterFreq,
        "Filter Freq",
        juce::NormalisableRange<float>(0.0f, 100.0f, 0.1f),
        100.0f,
        juce::AudioParameterFloatAttributes().withLabel("%")));

    layout.add(std::make_unique<juce::AudioParameterFloat>(
        ParameterID::filterReso,
        "Filter Reso",
        juce::NormalisableRange<float>(0.0f, 100.0f, 1.0f),
        15.0f,
        juce::AudioParameterFloatAttributes().withLabel("%")));

    layout.add(std::make_unique<juce::AudioParameterFloat>(
        ParameterID::filterEnv,
        "Filter Env",
        juce::NormalisableRange<float>(-100.0f, 100.0f, 0.1f),
        50.0f,
        juce::AudioParameterFloatAttributes().withLabel("%")));

    layout.add(std::make_unique<juce::AudioParameterFloat>(
        ParameterID::filterLFO,
        "Filter LFO",
        juce::NormalisableRange<float>(0.0f, 100.0f, 1.0f),
        0.0f,
        juce::AudioParameterFloatAttributes().withLabel("%")));

    auto filterVelocityStringFromValue = [](float value, int)
    {
        if (value < -90.0f)
            return juce::String("OFF");
        else
            return juce::String(value);
    };

    layout.add(std::make_unique<juce::AudioParameterFloat>(
        ParameterID::filterVelocity,
        "Velocity",
        juce::NormalisableRange<float>(-100.0f, 100.0f, 1.0f),
        0.0f,
        juce::AudioParameterFloatAttributes()
                .withLabel("%")
                .withStringFromValueFunction(filterVelocityStringFromValue)));

    layout.add(std::make_unique<juce::AudioParameterFloat>(
        ParameterID::filterAttack,
        "Filter Attack",
        juce::NormalisableRange<float>(0.0f, 100.0f, 1.0f),
        0.0f,
        juce::AudioParameterFloatAttributes().withLabel("%")));

    layout.add(std::make_unique<juce::AudioParameterFloat>(
        ParameterID::filterDecay,
        "Filter Decay",
        juce::NormalisableRange<float>(0.0f, 100.0f, 1.0f),
        30.0f,
        juce::AudioParameterFloatAttributes().withLabel("%")));

    layout.add(std::make_unique<juce::AudioParameterFloat>(
        ParameterID::filterSustain,
        "Filter Sustain",
        juce::NormalisableRange<float>(0.0f, 100.0f, 1.0f),
        0.0f,
        juce::AudioParameterFloatAttributes().withLabel("%")));

    layout.add(std::make_unique<juce::AudioParameterFloat>(
        ParameterID::filterRelease,
        "Filter Release",
        juce::NormalisableRange<float>(0.0f, 100.0f, 1.0f),
        25.0f,
        juce::AudioParameterFloatAttributes().withLabel("%")));

    layout.add(std::make_unique<juce::AudioParameterFloat>(
        ParameterID::envAttack,
        "Env Attack",
        juce::NormalisableRange<float>(0.0f, 100.0f, 1.0f),
        0.0f,
        juce::AudioParameterFloatAttributes().withLabel("%")));

    layout.add(std::make_unique<juce::AudioParameterFloat>(
        ParameterID::envDecay,
        "Env Decay",
        juce::NormalisableRange<float>(0.0f, 100.0f, 1.0f),
        50.0f,
        juce::AudioParameterFloatAttributes().withLabel("%")));

    layout.add(std::make_unique<juce::AudioParameterFloat>(
        ParameterID::envSustain,
        "Env Sustain",
        juce::NormalisableRange<float>(0.0f, 100.0f, 1.0f),
        100.0f,
        juce::AudioParameterFloatAttributes().withLabel("%")));

    layout.add(std::make_unique<juce::AudioParameterFloat>(
        ParameterID::envRelease,
        "Env Release",
        juce::NormalisableRange<float>(0.0f, 100.0f, 1.0f),
        30.0f,
        juce::AudioParameterFloatAttributes().withLabel("%")));

    auto lfoRateStringFromValue = [](float value, int)
    {
        float lfoHz = std::exp(7.0f * value - 4.0f);
        return juce::String(lfoHz, 3);
    };

    layout.add(std::make_unique<juce::AudioParameterFloat>(
        ParameterID::lfoRate,
        "LFO Rate",
        juce::NormalisableRange<float>(),
        0.96f,
        juce::AudioParameterFloatAttributes()
                .withLabel("Hz")
                .withStringFromValueFunction(lfoRateStringFromValue)));

    auto vibratoStringFromValue = [](float value, int)
    {
        if (value < 0.0f)
            return "Only Osc2 " + juce::String(-value, 1);
        else
            return juce::String(value, 1);
    };

    layout.add(std::make_unique<juce::AudioParameterFloat>(
        ParameterID::vibrato,
        "Vibrato",
        juce::NormalisableRange<float>(-100.0f, 100.0f, 0.1f),
        0.0f,
        juce::AudioParameterFloatAttributes()
                .withLabel("%")
                .withStringFromValueFunction(vibratoStringFromValue)));

    layout.add(std::make_unique<juce::AudioParameterFloat>(
        ParameterID::noise,
        "Noise",
        juce::NormalisableRange<float>(0.0f, 100.0f, 1.0f),
        0.0f,
        juce::AudioParameterFloatAttributes().withLabel("%")));

    layout.add(std::make_unique<juce::AudioParameterFloat>(
        ParameterID::octave,
        "Octave",
        juce::NormalisableRange<float>(-2.0f, 2.0f, 1.0f),
        0.0f));

    layout.add(std::make_unique<juce::AudioParameterFloat>(
        ParameterID::tuning,
        "Tuning",
        juce::NormalisableRange<float>(-100.0f, 100.0f, 0.1f),
        0.0f,
        juce::AudioParameterFloatAttributes().withLabel("cent")));

    layout.add(std::make_unique<juce::AudioParameterFloat>(
        ParameterID::outputLevel,
        "Output Level",
        juce::NormalisableRange<float>(-24.0f, 6.0f, 0.1f),
        -6.0f,
        juce::AudioParameterFloatAttributes().withLabel("dB")));

    return layout;
}

//==============================================================================
// This creates new instances of the plugin..
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new SubSynthAudioProcessor();
}
