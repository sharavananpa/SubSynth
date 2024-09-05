/*
  ==============================================================================

    Filter.h
    Created: 4 Sep 2024 7:32:05pm
    Author:  Sharavanan Balasundaravel

  ==============================================================================
*/

#pragma once

class Filter : public juce::dsp::LadderFilter<float>
{
public:
    void updateCoefficients(float cutoff, float Q)
    {
        setCutoffFrequencyHz(cutoff);
        setResonance(std::clamp(Q / 30.0f, 0.0f, 1.0f));
    }

    float render(float x)
    {
        updateSmoothers();
        return processSample(x, 0);
    }
};
