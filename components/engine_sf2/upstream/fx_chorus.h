/*
* FxChorus - Chorus audio effect
* based on a basic understanding of the chorus effect
*
* May 2025
* Author: Evgeny Aslovskiy AKA Copych
* License: MIT
*/

#pragma once
#include "config.h"

class FxChorus {
public:
    FxChorus() {
        sampleRate = SAMPLE_RATE;
        writeIndex = 0;
        lfoPhase = 0.0f;
        updateCounter = 0;
        currentLfoValue = 0.0f;

        for (int i = 0; i < MAX_DELAY; ++i) {
            bufferL[i] = 0.0f;
            bufferR[i] = 0.0f;
        }

        setLfoFreq(0.5f);
        setDepth(0.002f);
        setBaseDelay(0.03f);

    }
    
    inline void  __attribute__((hot,always_inline)) IRAM_ATTR processBlock(float* left, float* right) {
        for (int n = 0; n < DMA_BUFFER_LEN; ++n) {
            if (++updateCounter > LFO_UPDATE_INTERVAL) {
                updateCounter = 0;
                currentLfoValue = fast_sin(TWOPI * lfoPhase);
                lfoPhase += lfoFreq * LFO_UPDATE_INTERVAL / sampleRate;
                if (lfoPhase >= 1.0f) lfoPhase -= 1.0f;
            }

            float delayOffsetL = (baseDelay + depth * currentLfoValue) * sampleRate;
            float delayOffsetR = (baseDelay + depth * -currentLfoValue) * sampleRate;

            float readIndexL = writeIndex - delayOffsetL;
            float readIndexR = writeIndex - delayOffsetR;

            float delayedL = getInterpolatedSample(bufferL, readIndexL);
            float delayedR = getInterpolatedSample(bufferR, readIndexR);

            bufferL[writeIndex] = left[n];
            bufferR[writeIndex] = right[n];

            left[n]  = delayedL;
            right[n] = delayedR;

            writeIndex = (writeIndex + 1) % MAX_DELAY;
        }
    }

    // Parameter setters
    void setLfoFreq(float freq) { lfoFreq = freq; }
    void setDepth(float d) { depth = d; }
    void setBaseDelay(float delay) { baseDelay = delay; }

    inline float getLfoFreq() { return lfoFreq; }
    inline float getDepth() { return depth; }
    inline float getBaseDelay() { return baseDelay; }

private:
    static const int MAX_DELAY = 4096;
    static const int LFO_UPDATE_INTERVAL = 16;

    float bufferL[MAX_DELAY] = {0};
    float bufferR[MAX_DELAY] = {0};
    int writeIndex = 0;
    float lfoPhase = 0.0f;
    float currentLfoValue = 0.0f;
    int updateCounter = 0;

    float sampleRate;
    float lfoFreq = 5.0f;
    float depth = 0.2f;
    float baseDelay = 0.1f;

    float getInterpolatedSample(float* buffer, float index) {
        while (index < 0) index += MAX_DELAY;
        int indexInt = static_cast<int>(index);
        float frac = index - indexInt;
        int indexNext = (indexInt + 1) % MAX_DELAY;
        return (1.0f - frac) * buffer[indexInt] + frac * buffer[indexNext];
    }
};
