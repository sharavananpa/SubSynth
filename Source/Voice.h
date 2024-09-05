/*
  ==============================================================================

    Voice.h
    Created: 28 Aug 2024 7:06:05pm
    Author:  Sharavanan Balasundaravel

  ==============================================================================
*/

#pragma once

#include "Oscillator.h"
#include "Envelope.h"
#include "Filter.h"

class Voice
{
public:
    int note;
    int velocity;
    float frequency;
    Oscillator oscillatorA;
    Oscillator oscillatorB;
    Envelope envelope;
    float panLeft, panRight;
    float sawA = 0;
    float sawB = 0;
    Filter filter;
    float cutoff;
    float filterMod;
    float filterQ;
    float pitchBend;
    Envelope filterEnv;
    float filterEnvDepth;
    
    void reset()
    {
        note = 0;
        
        oscillatorA.reset();
        oscillatorB.reset();
        envelope.reset();
        
        filter.reset();
        filterEnv.reset();
        
        panLeft = 0.707f;
        panRight = 0.707f;
    }
    
    float render(float noise)
    {
        if (frequency < 40.0f)
        {
            sawA = oscillatorA.nextNaiveSample();
            sawB = oscillatorB.nextNaiveSample();
        }
        else if (frequency < 1000.f)
        {
            sawA = oscillatorA.nextPolyBLEPSample();
            sawB = oscillatorB.nextPolyBLEPSample();
        }
        else
        {
            sawA = oscillatorA.nextFourierSample();
            sawB = oscillatorB.nextFourierSample();
        }
        return filter.render(sawA + sawB + (noise * (velocity / 127.0f))) * envelope.nextValue();
    }
    
    void updatePanning()
    {
        float panning = std::clamp((note - 60.0f) / 96.0f, -0.3f, 0.3f);
        panLeft = std::sin(PI_OVER_4 * (1.0f - panning));
        panRight = std::sin(PI_OVER_4 * (1.0f + panning));
    }
    
    void updateLFO()
    {
        float fenv = filterEnv.nextValue();
        float modulatedCutoff = cutoff * std::exp(filterMod + filterEnvDepth * fenv) / pitchBend;
        modulatedCutoff = std::clamp(modulatedCutoff, 20.0f, 20000.0f);
        filter.updateCoefficients(modulatedCutoff, filterQ);
    }
};
