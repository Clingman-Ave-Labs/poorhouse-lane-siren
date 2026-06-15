#pragma once

#include <vector>

// ─── Chorus ──────────────────────────────────────────────────────────
//
// Classic chorus effect: a short modulated delay line mixed with the
// dry signal.  Two voices with slightly offset LFO phases create a
// wider stereo-like image even in mono.
//
// Architecture:
//   Input → 2 × modulated delay taps (LFO-swept, ~5–30 ms) → mix → output
//
// The subtle pitch modulation from the moving delay taps creates the
// characteristic thickening and shimmering sound essential for dub.

class Chorus {
public:
    void  init(float sampleRate);
    void  setRate(float hz);        // LFO rate (0.1–5 Hz)
    void  setDepth(float depth);    // modulation depth (0–1)
    void  setMix(float mix);        // 0–1 wet/dry
    float process(float input);
    void  reset();

private:
    static constexpr int NUM_VOICES = 2;
    static constexpr float BASE_DELAY_MS = 7.0f;    // center delay
    static constexpr float MOD_DEPTH_MS  = 5.0f;    // max excursion

    std::vector<float> buf_;
    int   writePos_   = 0;
    int   bufSize_    = 1;
    float sr_         = 48000.0f;

    float rate_       = 0.8f;     // LFO Hz
    float depth_      = 0.6f;     // 0–1
    float mix_        = 0.5f;     // wet/dry

    float lfoPhase_[NUM_VOICES] = {};
    float lfoInc_     = 0.0f;
};
