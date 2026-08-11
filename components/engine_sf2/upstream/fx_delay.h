/*
* FxDelay - Delay audio effect
* Based on Michael Licence's delay code
*
* Author: Evgeny Aslovskiy AKA Copych
* License: MIT
*/

#pragma once

#include "Arduino.h"
#include "esp_heap_caps.h"
#include "config.h"
#include "misc.h"

#ifdef BOARD_HAS_PSRAM 
  #define MALLOC_CAP        (MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT)
  #define MAX_DELAY         (SAMPLE_RATE)
#else
  #define MALLOC_CAP        (MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT)
  #define MAX_DELAY         (SAMPLE_RATE / 4)
#endif

enum class DelayTimeDiv : uint8_t {
    Whole = 0,
    Half,
    Quarter,
    Eighth,
    Sixteenth,
    Triplet8th,
    Dotted8th,
    Custom = 255
};

enum class DelayMode : uint8_t {
    Normal = 0,
    PingPong
};

class FxDelay {
public:
    FxDelay() {}

    inline void init() {
        delayLine_l = (float*)heap_caps_calloc(1, sizeof(float) * MAX_DELAY, MALLOC_CAP);
        delayLine_r = (float*)heap_caps_calloc(1, sizeof(float) * MAX_DELAY, MALLOC_CAP);

        if (!delayLine_l || !delayLine_r) {
            ESP_LOGI("Delay","DELAY: Memory allocation failed");
        } else {
            ESP_LOGI("Delay","DELAY: Memory allocated");
        }

        reset();
    }

    inline void reset() {
        for (int i = 0; i < MAX_DELAY; ++i) {
            delayLine_l[i] = 0.0f;
            delayLine_r[i] = 0.0f;
        }
        delayIn = 0;
        delayFeedback = 0.1f;
        delayLen = MAX_DELAY / 4;
        mode = DelayMode::Normal;
    }

    inline void setFeedback(float value) {
        delayFeedback = fclamp(value, 0.0f, 0.95f);
    }

    inline void setDelayTime(DelayTimeDiv div, float bpm) {
        float secondsPerBeat = 60.0f / bpm;
        float seconds = secondsPerBeat;

        switch (div) {
            case DelayTimeDiv::Whole:      seconds *= 4.0f; break;
            case DelayTimeDiv::Half:       seconds *= 2.0f; break;
            case DelayTimeDiv::Quarter:    seconds *= 1.0f; break;
            case DelayTimeDiv::Eighth:     seconds *= 0.5f; break;
            case DelayTimeDiv::Sixteenth:  seconds *= 0.25f; break;
            case DelayTimeDiv::Triplet8th: seconds *= 1.0f / 3.0f; break;
            case DelayTimeDiv::Dotted8th:  seconds *= 0.75f; break;
            case DelayTimeDiv::Custom:     return;
        }

        delayLen = fclamp((uint32_t)(seconds * SAMPLE_RATE), 1, MAX_DELAY - 1);
    }

    inline void setCustomLength(float seconds) {
        delayLen = fclamp((uint32_t)(seconds * SAMPLE_RATE), 1, MAX_DELAY - 1);
    }

    inline void setMode(DelayMode m) {
        mode = m;
    }

    inline float getFeedback() { return delayFeedback; }
    inline float getDelayTime() { return delayLen * (1.0f / SAMPLE_RATE); }
    inline float getDelayTimeBPM() { return 60.0f / (delayLen * (1.0f / SAMPLE_RATE)); }
    inline uint32_t getDelayLength() { return delayLen; }
    inline DelayMode getMode() { return mode; }
    inline DelayTimeDiv getDelayTimeDiv() { return DelayTimeDiv::Custom; }


    inline void  __attribute__((hot,always_inline)) IRAM_ATTR ProcessBlock(float* buffer_l, float* buffer_r) {
        for (int i = 0; i < DMA_BUFFER_LEN; ++i) {
            uint32_t outIndex = (delayIn + MAX_DELAY - delayLen) % MAX_DELAY;

            float outL = delayLine_l[outIndex];
            float outR = delayLine_r[outIndex];

            if (mode == DelayMode::PingPong) {
                delayLine_l[delayIn] = buffer_l[i] + outR * delayFeedback;  
                delayLine_r[delayIn] = buffer_r[i] + outL * delayFeedback;
            } else {
                delayLine_l[delayIn] = buffer_l[i] + outL * delayFeedback;
                delayLine_r[delayIn] = buffer_r[i] + outR * delayFeedback;
            }

            buffer_l[i] = outL;
            buffer_r[i] = outR;

            delayIn = (delayIn + 1) % MAX_DELAY;
        }
    }

private:
    float* delayLine_l = nullptr;
    float* delayLine_r = nullptr;
    float delayFeedback = 0.2f;

    uint32_t delayLen = MAX_DELAY / 4;
    uint32_t delayIn = 0;
    DelayMode mode = DelayMode::Normal;
};
