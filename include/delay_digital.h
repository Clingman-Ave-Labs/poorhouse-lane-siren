#pragma once

#include <vector>

// ─── Clean digital delay ───────────────────────────────────────────
//
// Pristine digital delay with no tape artifacts:
//   - No wobble or flutter modulation
//   - No tape saturation in feedback path
//   - Optional HP/LP filtering in feedback (prevent mud/harshness)
//   - Linear interpolation for fractional delay
//   - Immediate delay time changes (no spring-damped slew)

class DigitalDelay {
public:
    void  init(float sampleRate, float maxTimeSec);
    void  setTime(float sec);
    void  setFeedback(float fb);         // 0 – 0.95
    void  setMix(float mix);             // 0 – 1.0
    float process(float input);
    void  reset();

private:
    std::vector<float> buf_;
    int   writePos_     = 0;
    int   maxSamples_   = 1;
    float sr_           = 48000.0f;

    float delaySamples_ = 1.0f;
    float feedback_     = 0.0f;
    float mix_          = 0.0f;

    // Feedback path 1-pole filters
    float hpState_      = 0.0f;          // HP at 60 Hz
    float hpPrevIn_     = 0.0f;
    float hpCoeff_      = 0.0f;
    float lpState_      = 0.0f;          // LP at 12 kHz
    float lpCoeff_      = 0.0f;
};
