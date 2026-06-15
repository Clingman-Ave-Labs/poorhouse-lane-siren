#pragma once

#include <vector>

// ─── Flanger ─────────────────────────────────────────────────────────
//
// Classic flanger: a very short modulated delay (0.5–10 ms) mixed with
// the dry signal creates comb filtering.  The LFO sweeps the comb
// notches through the spectrum, producing the characteristic jet/
// metallic swoosh.
//
// Feedback intensifies the comb filter resonances, creating the
// aggressive flanging sound used in dub for dramatic transitions.
//
// Architecture:
//   Input + feedback → short delay (LFO-swept) → mix with dry → output
//                         ↑__________________ feedback ←─────────┘

class Flanger {
public:
    void  init(float sampleRate);
    void  setRate(float hz);        // LFO rate (0.05–5 Hz)
    void  setDepth(float depth);    // modulation depth (0–1)
    void  setFeedback(float fb);    // -0.95 to 0.95
    void  setMix(float mix);        // 0–1 wet/dry
    float process(float input);
    void  reset();

private:
    static constexpr float BASE_DELAY_MS = 2.0f;    // center delay
    static constexpr float MOD_DEPTH_MS  = 4.0f;    // max excursion

    std::vector<float> buf_;
    int   writePos_   = 0;
    int   bufSize_    = 1;
    float sr_         = 48000.0f;

    float rate_       = 0.3f;     // LFO Hz (slow for dub)
    float depth_      = 0.7f;     // 0–1
    float feedback_   = 0.5f;     // -0.95 to 0.95
    float mix_        = 0.5f;     // wet/dry

    float lfoPhase_   = 0.0f;
    float lfoInc_     = 0.0f;
    float fbState_    = 0.0f;     // feedback sample
};
