#pragma once

// ─── Waveform shapes ────────────────────────────────────────────────
enum class Waveform { Sine, Square, Saw, Triangle, COUNT };

// ─── Band-limited oscillator ────────────────────────────────────────
//
// Phase-accumulator oscillator with PolyBLEP antialiasing on Square
// and Saw.  Triangle uses a naive formula (its harmonics already roll
// off at –12 dB/oct so aliasing is negligible).
//
class Oscillator {
public:
    void setSampleRate(float sr);
    void setFrequency(float hz);
    void setWaveform(Waveform w);

    Waveform waveform()  const { return wave_; }
    float    frequency() const { return freq_; }

    // Returns the next sample in [–1, +1]
    float tick();

private:
    float    phase_    = 0.0f;
    float    phaseInc_ = 0.0f;
    float    freq_     = 440.0f;
    float    sr_       = 48000.0f;
    Waveform wave_     = Waveform::Sine;

    void updateIncrement();
    static float polyblep(float t, float dt);
};
