#pragma once

// ─── Phaser ──────────────────────────────────────────────────────────
//
// Classic analog phaser: cascade of allpass filters with LFO-modulated
// center frequencies.  The number of stages (2–6) determines the number
// of notches in the frequency response; more stages = deeper, more
// complex sweeping.
//
// Architecture:
//   Input → 4 × 1st-order allpass (LFO-swept) → mix with dry → output
//
// The LFO sweeps the allpass cutoff frequency logarithmically between
// a low and high bound, creating the characteristic swooshing sound.
// Feedback from output back to input intensifies the effect.

class Phaser {
public:
    void  init(float sampleRate);
    void  setRate(float hz);        // LFO rate (0.05–5 Hz)
    void  setDepth(float depth);    // sweep depth (0–1)
    void  setFeedback(float fb);    // -0.95 to 0.95
    void  setMix(float mix);        // 0–1
    float process(float input);
    void  reset();

private:
    static constexpr int NUM_STAGES = 4;

    // 1st-order allpass stage
    struct AllpassStage {
        float z1 = 0.0f;
        float process(float in, float coeff);
    };

    AllpassStage stages_[NUM_STAGES];

    float sr_       = 48000.0f;
    float rate_     = 0.4f;     // LFO Hz
    float depth_    = 0.7f;     // 0–1
    float feedback_ = 0.3f;     // -0.95 to 0.95
    float mix_      = 0.5f;     // wet/dry

    float lfoPhase_ = 0.0f;
    float lfoInc_   = 0.0f;
    float fbState_  = 0.0f;     // feedback sample

    // Sweep range (Hz) — logarithmic
    static constexpr float MIN_FREQ = 200.0f;
    static constexpr float MAX_FREQ = 8000.0f;
};
