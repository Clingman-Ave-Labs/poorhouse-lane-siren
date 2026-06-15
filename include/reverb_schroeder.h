#pragma once

#include <vector>

// ─── Schroeder reverb (Schroeder 1962) ─────────────────────────────
//
// Classic digital reverb topology:
//   Input → 4 parallel comb filters (each with LP-filtered feedback)
//   → sum → 2 series allpass filters → DC blocker → wet output.
//
// The parallel combs create the dense, metallic tail characteristic
// of early digital reverbs.  LP filtering in each comb's feedback
// path provides natural high-frequency damping.  The series allpasses
// add diffusion to thicken the echo density.
//
// This produces the "vintage digital reverb" sound — colored, with
// audible comb filter coloration.  Simple but characteristic.

class SchroederReverb {
public:
    void  init(float sampleRate);
    void  setSize(float size);      // 0 – 1.0
    void  setMix(float mix);        // 0 – 1.0
    void  setSuperDrip(bool on);    // no-op for schroeder
    float process(float input);
    void  reset();

private:
    // Comb filter with one-pole LP in feedback path
    struct CombFilter {
        std::vector<float> buf;
        int   pos      = 0;
        float feedback  = 0.0f;
        float lpState   = 0.0f;
        float lpCoeff   = 0.0f;
        float process(float input);
        void  reset();
    };

    // Allpass diffuser (fixed delay)
    struct Allpass {
        std::vector<float> buf;
        int   pos   = 0;
        float coeff = 0.5f;
        float process(float input);
        void  reset();
    };

    static constexpr int NUM_COMBS    = 4;
    static constexpr int NUM_ALLPASS  = 2;

    CombFilter combs_[NUM_COMBS];
    Allpass    allpasses_[NUM_ALLPASS];

    float sampleRate_ = 48000.0f;
    float size_       = 0.5f;
    float mix_        = 0.0f;

    // DC blocker state
    float dcIn_  = 0.0f;
    float dcOut_ = 0.0f;
};
