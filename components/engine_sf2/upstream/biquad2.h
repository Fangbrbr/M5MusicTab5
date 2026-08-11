/*
 * ----------------------------------------------------------------------------
 * ESP32-S3 optimized biquad filter
 * splitted into 3 classes
 *
* Author: Evgeny Aslovskiy AKA Copych
 * License: MIT
 * Repository: https://github.com/copych/ESP32-S3_SF2_Sampler_Synthesizer
 * 
 * File: biquad2.h
 * Purpose: contains 3 classes:
 *          Biquad filter coefficient calculator, 
 *          Biquad filter with internal coefficients,
 *          Biquad filter with shared coefficients
 * ----------------------------------------------------------------------------
 */

#pragma once
#include <Arduino.h>
#include "config.h"

class BiquadCalc {
public:
    enum Mode : uint8_t {
        LowPass = 0,
        HighPass,
        BandPass,
        Notch
    };

    struct Coeffs {
        float b0, b1, b2, a1, a2;
    };

private:
    static constexpr DRAM_ATTR float pi = 3.14159265358979323846f;
    static constexpr DRAM_ATTR size_t FreqSteps = 64;
    static constexpr DRAM_ATTR size_t QSteps = 16;
    static constexpr DRAM_ATTR float Fs = (float)SAMPLE_RATE;
    static constexpr DRAM_ATTR float FreqMin = 20.0f;
    static constexpr DRAM_ATTR float FreqMax = 20000.0f;
    static constexpr DRAM_ATTR float QMin = 0.5f;
    static constexpr DRAM_ATTR float QMax = FILTER_MAX_Q;
    static constexpr DRAM_ATTR float logFreqMin = 2.9957323f;   // logf(20)
    static constexpr DRAM_ATTR float logFreqMax = 9.9034876f;   // logf(20000)
    static constexpr DRAM_ATTR float invLogFreqRange = 1.0f / (logFreqMax - logFreqMin);

    struct CoeffsLUTEntry {
        float b0, b1, b2, a1, a2;
    };

    static inline float lerp(float a, float b, float t) {
        return a + t * (b - a);
    }

    static inline CoeffsLUTEntry lerpCoeffs(const CoeffsLUTEntry& c1, const CoeffsLUTEntry& c2, float t) {
        return {
            lerp(c1.b0, c2.b0, t),
            lerp(c1.b1, c2.b1, t),
            lerp(c1.b2, c2.b2, t),
            lerp(c1.a1, c2.a1, t),
            lerp(c1.a2, c2.a2, t)
        };
    }

    static inline float qAtIndex(size_t i) {
        float t = float(i) / float(QSteps - 1);
        return QMin + t * (QMax - QMin);
    }

    static inline float freqAtIndex(size_t i) {
        float t = float(i) / float(FreqSteps - 1);
        return expf(logFreqMin + t * (logFreqMax - logFreqMin));
    }

    static CoeffsLUTEntry calcBaseCoeffs(float f0, float Q) {
        float w0 = 2.0f * pi * f0 / Fs;
        float cos_w0 = cosf(w0);
        float sin_w0 = sinf(w0);
        float alpha = sin_w0 / (2.0f * Q);

        float a0 = 1.0f + alpha;
        float a1 = -2.0f * cos_w0;
        float a2 = 1.0f - alpha;

        float common = (1.0f - cos_w0) * 0.5f;
        float b0 = common;
        float b1 = 2.0f * common;
        float b2 = common;

        return {
            b0 / a0,
            b1 / a0,
            b2 / a0,
            a1 / a0,
            a2 / a0
        };
    }

    static void generateLUT() {
        for (size_t q = 0; q < QSteps; ++q) {
            for (size_t f = 0; f < FreqSteps; ++f) {
                lut[q * FreqSteps + f] = calcBaseCoeffs(freqAtIndex(f), qAtIndex(q));
            }
        }
        lutInitialized = true;
    }

    static inline CoeffsLUTEntry interpolateLUT(float freq, float Q) {
        freq = constrain(freq, FreqMin, FreqMax);
        Q = constrain(Q, QMin, QMax);

        float freqPos = (logf(freq) - logFreqMin) * invLogFreqRange * (FreqSteps - 1);
        float qPos = (Q - QMin) / (QMax - QMin) * (QSteps - 1);

        size_t fi0 = static_cast<size_t>(freqPos);
        size_t fi1 = min(fi0 + 1, FreqSteps - 1);
        size_t qi0 = static_cast<size_t>(qPos);
        size_t qi1 = min(qi0 + 1, QSteps - 1);

        float tf = freqPos - fi0;
        float tq = qPos - qi0;

        const CoeffsLUTEntry& c00 = lut[qi0 * FreqSteps + fi0];
        const CoeffsLUTEntry& c01 = lut[qi0 * FreqSteps + fi1];
        const CoeffsLUTEntry& c10 = lut[qi1 * FreqSteps + fi0];
        const CoeffsLUTEntry& c11 = lut[qi1 * FreqSteps + fi1];

        CoeffsLUTEntry cf0 = lerpCoeffs(c00, c01, tf);
        CoeffsLUTEntry cf1 = lerpCoeffs(c10, c11, tf);
        return lerpCoeffs(cf0, cf1, tq);
    }

public:
    static void ensureLUT() {
        if (!lutInitialized) generateLUT();
    }

    static Coeffs calcCoeffs(float freq, float Q, Mode mode) {
        ensureLUT();

        CoeffsLUTEntry c = interpolateLUT(freq, Q);
        uint32_t signB1, signB2;

        switch (mode) {
            case LowPass:
            case HighPass:
                signB1 = 1;
                signB2 = 1;
                break;
            case BandPass:
                signB1 = -1;
                signB2 = -1;
                break;
            case Notch:
                signB1 = -1;
                signB2 = 1;
                break;
            default:
                signB1 = 1;
                signB2 = 1;
                break;
        }

        return {
            c.b0,
            c.b1 * (float)signB1,
            c.b2 * (float)signB2,
            c.a1,
            c.a2
        };
    }

private:
    static CoeffsLUTEntry lut[FreqSteps * QSteps];
    static bool lutInitialized;
};

// Static definitions
inline BiquadCalc::CoeffsLUTEntry BiquadCalc::lut[BiquadCalc::FreqSteps * BiquadCalc::QSteps];
inline bool BiquadCalc::lutInitialized = false;

////////////////////////////////////////////////////////////////////////////////////////////////

class BiquadFilterSharedCoeffs {
public:
    using Coeffs = BiquadCalc::Coeffs;

    BiquadFilterSharedCoeffs() = default;

    inline void setCoeffs(const Coeffs* coeffsPtr) {
        coeffs = coeffsPtr;
    }

    inline void resetState() {
        x1 = x2 = y1 = y2 = 0.0f;
        w1 = w2 = z1 = z2 = 0.0f;
    }

    inline float __attribute__((always_inline)) IRAM_ATTR process(float in) {
        // Use coeffs from external pointer, no internal copy
        const Coeffs& c = *coeffs;
        float out = fmaf(c.b0, in, fmaf(c.b1, x1, c.b2 * x2)) - fmaf(c.a1, y1, c.a2 * y2);

        x2 = x1; x1 = in;
        y2 = y1; y1 = out;
        return out;
    }

    inline void __attribute__((always_inline)) IRAM_ATTR processLR(float* inOutL, float* inOutR) {
        const Coeffs& c = *coeffs;

        float outL = fmaf(c.b0, *inOutL, fmaf(c.b1, x1, c.b2 * x2)) - fmaf(c.a1, y1, c.a2 * y2);
        x2 = x1; x1 = *inOutL;
        y2 = y1; *inOutL = y1 = outL;

        float outR = fmaf(c.b0, *inOutR, fmaf(c.b1, w1, c.b2 * w2)) - fmaf(c.a1, z1, c.a2 * z2);
        w2 = w1; w1 = *inOutR;
        z2 = z1; *inOutR = z1 = outR;
    }

private:
    const Coeffs* coeffs = nullptr;

    float x1 = 0, x2 = 0, y1 = 0, y2 = 0;
    float w1 = 0, w2 = 0, z1 = 0, z2 = 0;
};

////////////////////////////////////////////////////////////////////////////////////////////////

class BiquadFilterInternalCoeffs {
public:
    using Coeffs = BiquadCalc::Coeffs;

    BiquadFilterInternalCoeffs() {
        resetState();
        setMode(BiquadCalc::LowPass);
        setFreqAndQ(20000.0f, 0.707f);
    }

    inline void setMode(BiquadCalc::Mode m) {
        mode = m;
        updateCoeffs();
    }

    inline void setFreq(float f) {
        if (f != freq) {
            freq = f;
            updateCoeffs();
        }
    }

    inline void setQ(float q) {
        if (q != Q) {
            Q = q;
            updateCoeffs();
        }
    }

    inline void setFreqAndQ(float f, float q) {
        if (f != freq || q != Q) {
            freq = f;
            Q = q;
            updateCoeffs();
        }
    }

    inline void resetState() {
        x1 = x2 = y1 = y2 = 0.0f;
        w1 = w2 = z1 = z2 = 0.0f;
    }

    inline float __attribute__((always_inline)) IRAM_ATTR process(float in) {
        float out = fmaf(coeffs.b0, in, fmaf(coeffs.b1, x1, coeffs.b2 * x2)) - fmaf(coeffs.a1, y1, coeffs.a2 * y2);
        x2 = x1; x1 = in;
        y2 = y1; y1 = out;
        return out;
    }

    inline void __attribute__((always_inline)) IRAM_ATTR processLR(float* inOutL, float* inOutR) {
        float outL = fmaf(coeffs.b0, *inOutL, fmaf(coeffs.b1, x1, coeffs.b2 * x2)) - fmaf(coeffs.a1, y1, coeffs.a2 * y2);
        x2 = x1; x1 = *inOutL;
        y2 = y1; *inOutL = y1 = outL;

        float outR = fmaf(coeffs.b0, *inOutR, fmaf(coeffs.b1, w1, coeffs.b2 * w2)) - fmaf(coeffs.a1, z1, coeffs.a2 * z2);
        w2 = w1; w1 = *inOutR;
        z2 = z1; *inOutR = z1 = outR;
    }

private:
    void updateCoeffs() {
        coeffs = BiquadCalc::calcCoeffs(freq, Q, mode);
    }

    BiquadCalc::Mode mode;
    float freq = 20000.0f;
    float Q = 0.707f;
    Coeffs coeffs;

    float x1 = 0, x2 = 0, y1 = 0, y2 = 0;
    float w1 = 0, w2 = 0, z1 = 0, z2 = 0;
};
