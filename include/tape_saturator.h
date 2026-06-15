#pragma once

#include <cmath>
#include "dsp_utils.h"

// ─── Tape Saturator ─────────────────────────────────────────────────
//
// Models the nonlinear magnetisation curve of analog tape, adding warm
// harmonic content, gentle compression, and high-frequency roll-off.
//
// Architecture:
//   Input × drive gain
//     → asymmetric soft clip (even + odd harmonics, like real tape)
//     → 1-pole LP at ~8 kHz (tape head HF loss)
//     → wet/dry mix
//
// At low drive the effect is subtle warmth; at high drive it becomes
// a thick, compressed, lo-fi saturation — perfect for dub.

class TapeSaturator {
public:
    void init(float sampleRate)
    {
        sr_ = sampleRate;
        updateCoeffs();
        reset();
    }

    void setDrive(float drive)   // 0–1
    {
        drive_ = drive;
    }

    void setMix(float mix)       // 0–1 wet/dry
    {
        mix_ = mix;
    }

    float process(float input)
    {
        float dry = input;

        // Input gain: 1× at drive=0, ~6× at drive=1
        float gain = 1.0f + drive_ * 5.0f;
        float x = input * gain;

        // Asymmetric soft clip — models tape magnetisation curve.
        // Positive half clips softer (like tape bias), negative harder.
        // This creates even harmonics (2nd, 4th) alongside odd ones.
        float sat;
        if (x >= 0.0f) {
            // Soft: tanh with slight bias toward compression
            sat = dsp::fast_tanh(x * 0.9f);
        } else {
            // Harder clip on negative — asymmetry adds 2nd harmonic
            sat = dsp::fast_tanh(x * 1.1f);
        }

        // Compensate output level (louder drive shouldn't blow up volume)
        sat *= 1.0f / (1.0f + drive_ * 0.5f);

        // Tape head HF roll-off — 1-pole LP at ~8 kHz
        lp_state_ = lp_state_ + lp_coeff_ * (sat - lp_state_);
        lp_state_ = dsp::sanitize(lp_state_);

        // Wet/dry mix
        return dry * (1.0f - mix_) + lp_state_ * mix_;
    }

    void reset()
    {
        lp_state_ = 0.0f;
    }

private:
    void updateCoeffs()
    {
        // 1-pole LP coefficient for ~8 kHz cutoff
        constexpr float CUTOFF = 8000.0f;
        float w = 2.0f * dsp::PI_F * CUTOFF / sr_;
        lp_coeff_ = w / (1.0f + w);  // bilinear-ish approximation
    }

    float sr_       = 48000.0f;
    float drive_    = 0.5f;   // 0–1
    float mix_      = 0.5f;   // wet/dry

    float lp_state_ = 0.0f;
    float lp_coeff_ = 0.0f;
};
