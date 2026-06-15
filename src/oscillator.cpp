// oscillator.cpp — Band-limited oscillator with PolyBLEP antialiasing
//
// Sine      — std::sin (exact)
// Square    — naive + PolyBLEP at 0 and 0.5
// Saw       — naive + PolyBLEP at 0
// Triangle  — naive (harmonics roll off fast enough)

#include "oscillator.h"
#include <cmath>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

// ─── Helpers ────────────────────────────────────────────────────────

void Oscillator::setSampleRate(float sr) { sr_ = sr; updateIncrement(); }
void Oscillator::setFrequency(float hz)  { freq_ = hz; updateIncrement(); }
void Oscillator::setWaveform(Waveform w) { wave_ = w; }

void Oscillator::updateIncrement()
{
    phaseInc_ = freq_ / sr_;
}

// ─── PolyBLEP ───────────────────────────────────────────────────────
//
// Smooths the discontinuity at phase 0 (and, for square, at 0.5).
// t = current phase [0,1),  dt = phase increment per sample.

float Oscillator::polyblep(float t, float dt)
{
    if (t < dt) {
        t /= dt;
        return t + t - t * t - 1.0f;
    }
    if (t > 1.0f - dt) {
        t = (t - 1.0f) / dt;
        return t * t + t + t + 1.0f;
    }
    return 0.0f;
}

// ─── Tick ───────────────────────────────────────────────────────────

float Oscillator::tick()
{
    float sample = 0.0f;
    const float t  = phase_;
    const float dt = phaseInc_;

    switch (wave_) {

    case Waveform::Sine:
        sample = std::sin(2.0f * static_cast<float>(M_PI) * t);
        break;

    case Waveform::Saw:
        // Naive: ramp –1 → +1 over one period
        sample = 2.0f * t - 1.0f;
        sample -= polyblep(t, dt);
        break;

    case Waveform::Square:
        // Naive: +1 for first half, –1 for second
        sample = (t < 0.5f) ? 1.0f : -1.0f;
        sample += polyblep(t, dt);                           // rising edge at 0
        sample -= polyblep(std::fmod(t + 0.5f, 1.0f), dt);  // falling edge at 0.5
        break;

    case Waveform::Triangle:
        // Naive triangle — no PolyBLEP needed (–12 dB/oct rolloff)
        sample = 4.0f * std::fabs(t - 0.5f) - 1.0f;
        break;

    default:
        break;
    }

    // Advance phase
    phase_ += dt;
    if (phase_ >= 1.0f) phase_ -= 1.0f;

    return sample;
}
