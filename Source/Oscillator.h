/*
  ==============================================================================

    Oscillator.h
    Created: 29 Aug 2024 5:40:51pm
    Author:  Sharavanan Balasundaravel

  ==============================================================================
*/

#pragma once

const float TWO_PI = juce::MathConstants<float>::twoPi;
const float PI = juce::MathConstants<float>::pi;
const float PI_OVER_4 = juce::MathConstants<float>::pi / 4;

class Oscillator 
{
public:
    float amplitude;
    float sampleRate;
    float freq;
    float phase;
    
    void setFrequency(float freq) 
    {
        this->freq = freq;
        updateIncrement();
    }
    
    void setSampleRate(float sampleRate) 
    {
        this->sampleRate = sampleRate;
        this->nyquist = sampleRate / 2.0f;
    }
    
    void reset() 
    {
        inc = 0.0f;
        phase = 0.0f;
        amplitude = 0.0f;
    }
    
    // Generated using claude sonnet 3.5 (Poly BLEP)
    float nextPolyBLEPSample()
    {
        float t = phase;
        float value = 2.0f * t - 1.0f;

        float dt = inc;
        if (t < dt) 
        {
            t /= dt;
            value -= (t + t - t * t - 1.0f);
        }
        else if (t > 1.0f - dt)
        {
            t = (t - 1.0f) / dt;
            value -= (t * t + t + t + 1.0f);
        }

        phase += inc;
        if (phase >= 1.0f) phase -= 1.0f;

        return amplitude * value;
    }
    
    float nextNaiveSample()
    {
        float value = 2.0f * phase - 1.0f;

        phase += inc;
        if (phase >= 1.0f) phase -= 1.0f;

        return amplitude * value;
    }
    
    float nextFourierSample()
    {
        float value = 0.0f;
        float h = freq;
        float i = 1.0f;
        float m = 0.63661977236f;
        
        while (h < nyquist) 
        {
            value += m * std::sin(TWO_PI * phase * i) / i;
            h += freq;
            i += 1.0f;
            m = -m;
        }
        
        phase += inc;
        if (phase >= 1.0f) phase -= 1.0f;
        
        return amplitude * value;
    }
    
private:
    float inc;
    float nyquist;

    void updateIncrement()
    {
        inc = freq / sampleRate;
    }
};
