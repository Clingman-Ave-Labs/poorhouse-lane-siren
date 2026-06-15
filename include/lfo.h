#pragma once

// ─── LFO waveform shapes ──────────────────────────────────────────
enum class LfoWave {
    Sine, Triangle, Square, RampUp, RampDown,   // traditional
    SampleHold, ExpRise, ExpFall,               // experimental
    COUNT
};

// ─── Low-frequency oscillator ──────────────────────────────────────
//
// 8 waveform shapes with phase retrigger.
// Traditional: Sine, Triangle, Square, RampUp, RampDown
// Experimental: Sample & Hold, Exponential Rise, Exponential Fall

class LFO {
public:
    void setSampleRate(float sr);
    void setRate(float hz);
    void setWaveform(LfoWave w);

    LfoWave waveform() const { return wave_; }

    // Reset phase to 0 (call on trigger press for consistent attacks)
    void resetPhase();

    // Returns next sample in [–1, +1]
    float tick();

private:
    float    phase_    = 0.0f;
    float    phaseInc_ = 0.0f;
    float    rate_     = 1.0f;
    float    sr_       = 48000.0f;
    LfoWave  wave_     = LfoWave::Sine;

    // Sample & Hold state
    float    shHeld_   = 0.0f;  // current held random value
    bool     shHalf_   = false; // which half of the cycle we're in

    void updateInc();
};
