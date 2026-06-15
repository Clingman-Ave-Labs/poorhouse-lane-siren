#pragma once

#include <vector>

// ─── Plate reverb (Dattorro) ────────────────────────────────────────
//
// Models a steel plate reverberator using the Dattorro topology:
//   Input → 4 input allpass diffusers (series) → tank section
//   with 2 cross-coupled delay paths, each containing a modulated
//   allpass, delay line, and LP-filtered feedback → DC blocker.
//
// The modulated allpasses add subtle chorus that prevents metallic
// ringing.  LP filtering in the feedback path simulates the natural
// high-frequency absorption of a vibrating plate.

class PlateReverb {
public:
    void  init(float sampleRate);
    void  setSize(float size);      // 0 – 1.0
    void  setMix(float mix);        // 0 – 1.0
    void  setSuperDrip(bool on);    // no-op (plate has no spring drip)
    float process(float input);
    void  reset();

private:
    // Simple allpass diffuser (fixed delay)
    struct Allpass {
        std::vector<float> buf;
        int   pos   = 0;
        float coeff = 0.5f;
        float process(float input);
        void  reset();
    };

    // Modulated allpass (LFO-modulated delay for tank chorus)
    struct ModAllpass {
        std::vector<float> buf;
        int   size    = 0;          // buffer length
        int   wPos    = 0;          // write position
        float coeff   = 0.5f;
        float phase   = 0.0f;      // LFO phase [0, 2pi)
        float lfoRate = 0.0f;      // LFO increment per sample
        float lfoAmt  = 0.0f;      // modulation depth in samples
        int   baseDel = 0;         // nominal delay length
        float process(float input);
        void  reset();
    };

    // Delay line (simple circular buffer)
    struct Delay {
        std::vector<float> buf;
        int pos = 0;
        float read() const;
        void  write(float v);
        void  advance();
        void  reset();
    };

    // One half of the tank (two halves cross-feed each other)
    struct TankHalf {
        ModAllpass modAp;
        Delay     delay;
        float     lpState  = 0.0f;  // one-pole LP state
        float     lpCoeff  = 0.0f;  // damping coefficient
        float     feedback = 0.0f;
    };

    static constexpr int NUM_DIFFUSERS = 4;

    Allpass   diffusers_[NUM_DIFFUSERS];
    TankHalf  tank_[2];

    float sampleRate_ = 48000.0f;
    float size_       = 0.5f;
    float mix_        = 0.0f;

    // DC blocker state
    float dcIn_  = 0.0f;
    float dcOut_ = 0.0f;
};
