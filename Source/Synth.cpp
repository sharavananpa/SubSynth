/*
  ==============================================================================

    Synth.cpp
    Created: 28 Aug 2024 7:15:18pm
    Author:  Sharavanan Balasundaravel

  ==============================================================================
*/

#include "Synth.h"

Synth::Synth()
{
    this->sampleRate = 44100.0f;
}

void Synth::allocateResources(double sampleRate, int samplesPerBlock)
{
    this->sampleRate = static_cast<float>(sampleRate);
    
    juce::dsp::ProcessSpec spec;
    spec.sampleRate = sampleRate;
    spec.maximumBlockSize = samplesPerBlock;
    spec.numChannels = 1;
    
    for (int i = 0; i < numVoices; ++i)
    {
        voices[i].filter.setMode(juce::dsp::LadderFilterMode::LPF12);
        voices[i].filter.prepare(spec);
    }
}

void Synth::deallocateResources() { }

void Synth::reset()
{
    for (int i = 0; i < this->numVoices; ++i)
    {
        voices[i].reset();
    }
    
    noiseGenerator.reset();
    pitchBend = 1.0f;
    sustainPedalPressed = false;
    
    outputLevelSmoother.reset(sampleRate, 0.05f);
    oscMixSmoother.reset(sampleRate, 0.0001f);
    
    lfo = 0.0f;
    lfoStep = 0;
    
    modWheel = 0.0f;
    
    resonanceCtl = 1.0f;
    filterCtl = 0.0f;
    
    aftertouch = 0.0f;
    
    filterSmoother = 0.0f;
}

void Synth::render(juce::AudioBuffer<float>& buffer, int bufferOffset, int sampleCount, int numChannels)
{
    float* leftOutputBuffer = buffer.getWritePointer(0) + bufferOffset;
    float* rightOutputBuffer = buffer.getWritePointer(1) + bufferOffset;
    
    
    for (int i = 0; i < this->numVoices; ++i) 
    {
        Voice& voice = voices[i];
        if (voice.envelope.isActive())
        {
            voice.oscillatorA.setFrequency(voice.frequency * pitchBend);
            voice.oscillatorB.setFrequency(voice.oscillatorA.freq * oscBTune);
            
            voice.oscillatorA.amplitude = ((0.004f * float((voice.velocity + 64) * (voice.velocity + 64)) - 8.0f) / 127.0f) * 0.5f;
            voice.oscillatorB.amplitude = voice.oscillatorA.amplitude * oscMixSmoother.getNextValue();
            voice.filterQ = filterQ + resonanceCtl;
            voice.pitchBend = pitchBend;
            voice.filterEnvDepth = filterEnvDepth;
        }
    }
    
    for (int sample = 0; sample < sampleCount; ++sample)
    {
        updateLFO();
        float outputL = 0.0f;
        float outputR = 0.0f;
        
        for (int i = 0; i < this->numVoices; ++i) 
        {
            Voice& voice = voices[i];
            if (voice.envelope.isActive())
            {
                float noise = noiseGenerator.nextValue() * noiseMix;
                float output = voice.render(noise);
                outputL += output * voice.panLeft;
                outputR += output * voice.panRight;
            }
        }
        
        float outputLevel = outputLevelSmoother.getNextValue();
        outputL *= outputLevel;
        outputR *= outputLevel;
        
        if (numChannels > 1) {
            leftOutputBuffer[sample] = outputL;
            rightOutputBuffer[sample] = outputR;
        } else {
            leftOutputBuffer[sample] = (outputL + outputR) * 0.5f;
        }
    }
    for (int i = 0; i < this->numVoices; ++i)
    {
        Voice& voice = voices[i];
        if (!voice.envelope.isActive()) {
            voice.envelope.reset();
            voice.filter.reset();
        }
    }
}

void Synth::midiMessage(uint8_t data0, uint8_t data1, uint8_t data2)
{
    switch (data0 & 0xF0) 
    {
        case 0x80:
            noteOff(data1 & 0x7F);
            break;
            
        case 0x90: {
            uint8_t note = data1 & 0x7F;
            uint8_t velocity = data2 & 0x7F;
            if (velocity > 0)
            {
                noteOn(note, velocity);
            }
            else
            {
                noteOff(note);
            }
            break;
        }
            
        case 0xE0:
            pitchBend = std::exp(0.000014102f * float(data1 + 128 * data2 - 8192));
            break;
        
        case 0xB0:
            controlChange(data1, data2);
            break;
            
        case 0xD0:
            aftertouch = 0.0001f * float(data1 * data1);
            break;
    }
}

void Synth::controlChange(uint8_t data1, uint8_t data2)
{
    switch (data1) {
        case 0x40:
            sustainPedalPressed = (data2 >= 64);

            if (!sustainPedalPressed) {
                noteOff(-1);
            }
            break;
            
        case 0x01:
            modWheel = 0.000005f * float(data2 * data2); 
            break;
        
        case 0x47:
            resonanceCtl = 154.0f / float(154 - data2); 
            break;
            
        // Filter +
        case 0x4A:
            filterCtl = 0.02f * float(data2); break;
        // Filter -
        case 0x4B:
            filterCtl = -0.03f * float(data2); break;

        // All notes off
        default:
            if (data1 >= 0x78) {
                for (int i = 0; i < this->numVoices; ++i) {
                    voices[i].reset();
                }
                sustainPedalPressed = false;
            }
            break;
    }
}

void Synth::noteOn(int note, int velocity)
{
    if (this->ignoreVelocity) velocity = 80;
    int voiceIndex = 0;
    float minAmp = 9999.0f;
    for (int i = 0; i < this->numVoices; ++i)
    {
        if (!voices[i].envelope.isActive() || voices[i].note == note)
        {
            voiceIndex = i;
            break;
        }
        
        if ((voices[i].velocity * voices[i].envelope.level) < minAmp && !voices[i].envelope.isInAttack()) {
            minAmp = voices[i].velocity * voices[i].envelope.level;
            voiceIndex = i;
        }
        
    }
    
    Voice& voice = voices[voiceIndex];
    voice.note = note;
    float frequency = 440.0f * std::exp2(float(note - 69 + masterTune) / 12.0f);
    
    voice.frequency = frequency;
    voice.cutoff = frequency / PI;
    voice.cutoff *= std::exp(velocitySensitivity * float(velocity - 64));
    voice.velocity = velocity;
    voice.updatePanning();
    
    voice.oscillatorA.setSampleRate(this->sampleRate);
    voice.oscillatorB.setSampleRate(this->sampleRate);
    
    voice.oscillatorA.reset();
    voice.oscillatorB.reset();
    
    voice.envelope.attackA = envAttack;
    voice.envelope.decayA = envDecay;
    voice.envelope.sustainLevel = envSustain;
    voice.envelope.releaseA = envRelease;
    voice.envelope.attack();
    
    voice.filterEnv.attackA = filterAttack;
    voice.filterEnv.decayA = filterDecay;
    voice.filterEnv.sustainLevel = filterSustain;
    voice.filterEnv.releaseA = filterRelease;
    voice.filterEnv.attack();
}

void Synth::noteOff(int note)
{
    for (int i = 0; i < this->numVoices; ++i)
    {
        Voice& voice = voices[i];
        if (voice.note == note) 
        {
            if (sustainPedalPressed) 
            {
                voice.note = -1;
            }
            else
            {
                voice.envelope.release();
                voice.filterEnv.release();
                voice.note = 0;
            }
        }
    }
}

void Synth::updateLFO() 
{
    
    if (--lfoStep <= 0)
    {
        lfoStep = LFO_MAX;
        lfo += lfoInc;
        if (lfo > PI) 
        { 
            lfo -= TWO_PI;
        }
        
        const float sine = std::sin(lfo);
        float vibratoMod = 1.0f + sine * (modWheel + vibrato);
        float pwm = 1.0f + sine * (modWheel + pwmDepth);
        
        float filterMod = filterKeyTracking + filterCtl + (filterLFODepth + aftertouch) * sine;
        
        filterSmoother += 0.005f * (filterMod - filterSmoother);
        
        for (int i = 0; i < numVoices; ++i)
        {
            Voice& voice = voices[i];
            if (voice.envelope.isActive())
            {
                voice.oscillatorA.setFrequency(voice.oscillatorA.freq * vibratoMod);
                voice.oscillatorB.setFrequency(voice.oscillatorB.freq * pwm);
                voice.filterMod = filterSmoother;
                voice.updateLFO();
            }
        }
    }
}

