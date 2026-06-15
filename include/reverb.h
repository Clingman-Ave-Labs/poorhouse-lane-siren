#pragma once

#include <vector>

// ─── Spring reverb ──────────────────────────────────────────────────
//
// Models a dual-spring tank (like an Accutronics Type 4):
//   Input → dispersion allpass cascade → 2 parallel spring delay
//   lines with LP-filtered feedback → DC blocker → wet output.
//
// The allpass dispersion creates the characteristic metallic chirp.
// LP filtering in the feedback path simulates natural high-frequency
// damping of a physical spring.

class Reverb {
public:
    void  init(float sampleRate);
    void  setSize(float size);      // 0 – 1.0
    void  setMix(float mix);        // 0 – 1.0
    void  setSuperDrip(bool on);    // heavy dub spring reverb mode
    float process(float input);
    void  reset();

private:
    // Dispersion allpass (short delays → spring chirp)
    struct Allpass {
        std::vector<float> buf;
        int   pos   = 0;
        float coeff = 0.5f;
        float process(float input);
        void  reset();
    };

    // Spring delay line with LP-filtered feedback
    struct SpringLine {
        std::vector<float> buf;
        int   pos      = 0;
        float feedback  = 0.0f;
        float lpState   = 0.0f;
        float lpCoeff   = 0.0f;
        float process(float input);
        void  reset();
    };

    static constexpr int NUM_DISPERSION = 6;
    static constexpr int NUM_SPRINGS    = 2;

    Allpass    dispersion_[NUM_DISPERSION];
    SpringLine springs_[NUM_SPRINGS];
    float      size_       = 0.65f;
    float      mix_        = 0.0f;
    bool       drip_       = false;
    float      sampleRate_ = 48000.0f;
    float      inputGain_  = 0.35f;

    // DC blocker state
    float dcIn_  = 0.0f;
    float dcOut_ = 0.0f;
};
