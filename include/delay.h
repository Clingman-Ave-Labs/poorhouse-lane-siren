#pragma once

#include <vector>

// ─── Tape delay ─────────────────────────────────────────────────────
//
// Analog-style tape delay with:
//   - Fractional read head with linear interpolation
//   - Spring-damped read head (overshoots on time change → boomerang)
//   - Tape wobble (~0.5 Hz) and flutter (~3.5 Hz) modulation
//   - Feedback path: HP 80 Hz → LP 5 kHz → driven tanh blend (30%)
//   - NaN/Inf-safe: resets on detection in feedback path

class TapeDelay {
public:
    void  init(float sampleRate, float maxTimeSec);
    void  setTime(float sec);
    void  setFeedback(float fb);         // 0 – 0.95
    void  setMix(float mix);             // 0 – 1.0
    void  setRepitchRate(float rate);     // 0 – 1.0 (higher = more overshoot)
    void  setWobbleAmount(float amt);    // 0 – 1.0 (tape transport drift)
    void  setFlutterAmount(float amt);   // 0 – 1.0 (tape tension variation)
    float process(float input);
    void  reset();

private:
    std::vector<float> buf_;
    int   writePos_     = 0;
    int   maxSamples_   = 1;
    float sr_           = 48000.0f;

    // Spring-damped read position (samples behind write head)
    float readPos_      = 1.0f;
    float readVel_      = 0.0f;          // read head velocity (samples/sample)
    float targetDelay_  = 1.0f;
    float spring_       = 0.0f;          // stiffness (ω₀²)
    float damp_         = 0.0f;          // friction  (2ζω₀)

    float feedback_     = 0.0f;
    float mix_          = 0.0f;

    // Tape modulation oscillators
    float wobblePhase_  = 0.0f;          // ~0.5 Hz drift
    float flutterPhase_ = 0.0f;          // ~3.5 Hz variation
    float wobbleInc_    = 0.0f;
    float flutterInc_   = 0.0f;
    float wobbleDepth_  = 0.0f;          // amplitude in samples (max)
    float flutterDepth_ = 0.0f;
    float wobbleAmt_    = 1.0f;          // 0–1 user control
    float flutterAmt_   = 1.0f;         // 0–1 user control

    // Feedback path 1-pole filters
    float hpState_      = 0.0f;          // HP at 80 Hz
    float hpPrevIn_     = 0.0f;
    float hpCoeff_      = 0.0f;
    float lpState_      = 0.0f;          // LP at 5 kHz
    float lpCoeff_      = 0.0f;
};
