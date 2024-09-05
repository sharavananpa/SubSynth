/*
  ==============================================================================

    Synth.h
    Created: 28 Aug 2024 7:15:18pm
    Author:  Sharavanan Balasundaravel

  ==============================================================================
*/

#pragma once

#include <JuceHeader.h>
#include "Voice.h"
#include "NoiseGenerator.h"

class Synth
{
public:
    
    Synth();
    
    void allocateResources(double sampleRate, int samplesPerBlock);
    void deallocateResources();
    
    void reset();
    void render(juce::AudioBuffer<float>& buffer, int bufferOffser, int sampleCount, int numChannels);
    void midiMessage(uint8_t data0, uint8_t data1, uint8_t data2);
    
    float noiseMix;
    float envAttack, envDecay, envSustain, envRelease;
    float oscBTune;
    float masterTune;
    
    static constexpr int numVoices = 16;

    juce::LinearSmoothedValue<float> outputLevelSmoother;
    juce::LinearSmoothedValue<float> oscMixSmoother;
    
    float velocitySensitivity;
    bool ignoreVelocity;
    
    const int LFO_MAX = 32;
    float lfoInc;
    float vibrato;
    float pwmDepth;
    
    float filterKeyTracking;
    float filterQ;
    float filterLFODepth;
    
    float filterAttack, filterDecay, filterSustain, filterRelease;
    float filterEnvDepth;
    
private:
    void noteOn(int note, int velocity);
    void noteOff(int note);
    
    void controlChange(uint8_t data1, uint8_t data2);
    
    float sampleRate;
    
    NoiseGenerator noiseGenerator;
    
    float pitchBend;
    
    std::array<Voice, numVoices> voices;
    
    bool sustainPedalPressed;
    
    void updateLFO();
    int lfoStep;
    float lfo;
    
    float modWheel;
    
    float resonanceCtl;
    float filterCtl;
    
    float aftertouch;
    
    float filterSmoother;
};
