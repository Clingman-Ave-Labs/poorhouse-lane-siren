#pragma once

#include <vector>

// ─── Hall reverb ────────────────────────────────────────────────────
//
// 8-channel Feedback Delay Network (FDN) with Hadamard mixing matrix.
// Models a large reflective space — smooth, diffuse tail without the
// metallic chirp of a spring.
//
// Each channel has a prime-length delay line with LP-filtered feedback.
// Hadamard matrix couples all channels, distributing energy evenly.
// DC blocker on output prevents low-frequency buildup.

class HallReverb {
public:
    void  init(float sampleRate);
    void  setSize(float size);      // 0 – 1.0
    void  setMix(float mix);        // 0 – 1.0
    void  setSuperDrip(bool on);    // no-op for hall
    float process(float input);
    void  reset();

private:
    static constexpr int NUM_CHANNELS = 8;

    // Per-channel delay line with one-pole LP in the feedback path
    struct DelayLine {
        std::vector<float> buf;
        int   pos     = 0;
        float lpState = 0.0f;
        float lpCoeff = 0.0f;
        void  reset();
    };

    DelayLine lines_[NUM_CHANNELS];

    float feedback_   = 0.0f;
    float size_       = 0.65f;
    float mix_        = 0.0f;
    float sampleRate_ = 48000.0f;

    // DC blocker state
    float dcIn_  = 0.0f;
    float dcOut_ = 0.0f;

    // Input distribution gains (one per channel)
    static constexpr float INPUT_GAINS[NUM_CHANNELS] = {
         0.35f,  0.30f,  0.33f,  0.28f,
         0.32f,  0.27f,  0.31f,  0.29f
    };

    // Recalculate feedback and LP coefficients from size_
    void updateParams();

    // In-place Hadamard transform on 8 elements (butterfly stages)
    static void hadamard8(float* x);
};
