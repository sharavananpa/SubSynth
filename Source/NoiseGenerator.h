/*
  ==============================================================================

    NoiseGenerator.h
    Created: 28 Aug 2024 6:55:06pm
    Author:  Sharavanan Balasundaravel

  ==============================================================================
*/

#pragma once

class NoiseGenerator
{
public:
    void reset()
    {
        random.setSeed(2304);
    }
    
    float nextValue()
    {
        return random.nextFloat() * 2 - 1;
    }
    
private:
    juce::Random random;
};
