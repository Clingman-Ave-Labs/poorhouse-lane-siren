#pragma once

// ─── Moog ladder filter (4-pole low-pass) ───────────────────────────
//
// Simplified Stilson/Smith model with tanh saturation for the
// characteristic Moog sound.  NaN/Inf-safe: resets state on detection.

class MoogFilter {
public:
    void  setSampleRate(float sr);
    void  setCutoff(float hz);      // 20 – 20 000 Hz
    void  setResonance(float r);    // 0 – 0.95
    float process(float input);
    void  reset();

private:
    float stage_[4] = {};
    float cutoff_   = 20000.0f;
    float reso_     = 0.0f;
    float sr_       = 48000.0f;
    float f_        = 1.0f;         // cached frequency coeff
    float k_        = 0.0f;         // cached resonance coeff
};
