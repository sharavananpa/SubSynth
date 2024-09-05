/*
  ==============================================================================

    Envelope.h
    Created: 2 Sep 2024 9:39:30pm
    Author:  Sharavanan Balasundaravel

  ==============================================================================
*/

#pragma once

const float SILENCE = 0.0001f;

class Envelope {
public:
    float level;
    
    float attackA;
    float decayA;
    float sustainLevel;
    float releaseA;
    
    void reset()
    {
        level = 0.0f;
        target = 0.0f;
        a = 0.0f;
    }
    
    float nextValue()
    {
        level = a * (level - target) + target;
        if (level + target > 3.0f) {
            target = sustainLevel;
            a = decayA;
        }
        return level;
    }
    
    inline bool isActive() const
    {
        return level > SILENCE;
    }

    inline bool isInAttack() const
    {
        return target >= 2.0f;
    }

    void attack()
    {
        level += SILENCE + SILENCE;
        target = 2.0f;
        a = attackA;
    }

    void release()
    {
        target = 0.0f;
        a = releaseA;
    }

private:
    float target;
    float a;
};
