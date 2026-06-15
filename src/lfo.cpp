// lfo.cpp — Low-frequency oscillator with 8 waveform shapes
//
// Traditional: Sine, Triangle, Square, Ramp Up, Ramp Down
// Experimental: Sample & Hold, Exponential Rise, Exponential Fall
//
// No antialiasing needed — LFO operates at sub-audio rates (0.1–20 Hz).

#include "lfo.h"
#include <cmath>
#include <random>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

void LFO::setSampleRate(float sr) { sr_ = sr; updateInc(); }
void LFO::setRate(float hz)       { rate_ = hz; updateInc(); }
void LFO::setWaveform(LfoWave w)  { wave_ = w; }

void LFO::resetPhase()
{
    phase_  = 0.0f;
    shHalf_ = false;
    shHeld_ = 0.0f;
}

void LFO::updateInc()
{
    phaseInc_ = rate_ / sr_;
}

float LFO::tick()
{
    float out = 0.0f;

    switch (wave_) {
    case LfoWave::Sine:
        out = std::sin(2.0f * static_cast<float>(M_PI) * phase_);
        break;
    case LfoWave::Triangle:
        out = 4.0f * std::fabs(phase_ - 0.5f) - 1.0f;
        break;
    case LfoWave::Square:
        out = (phase_ < 0.5f) ? 1.0f : -1.0f;
        break;
    case LfoWave::RampUp:
        out = 2.0f * phase_ - 1.0f;
        break;
    case LfoWave::RampDown:
        out = 1.0f - 2.0f * phase_;
        break;

    // ── Experimental waveforms ──────────────────────────────────

    case LfoWave::SampleHold: {
        // Random step: holds a value, jumps to a new one each half-cycle.
        // Two steps per cycle gives rhythmic variety at any LFO rate.
        bool half = (phase_ >= 0.5f);
        if (half != shHalf_) {
            shHalf_ = half;
            // High-quality pseudo-random in [-1, +1]
            static std::mt19937 gen(std::random_device{}());
            static std::uniform_real_distribution<float> dist(-1.0f, 1.0f);
            shHeld_ = dist(gen);
        }
        out = shHeld_;
        break;
    }
    case LfoWave::ExpRise:
        // Exponential rise: slow start, accelerating climb.
        // Maps phase [0,1] → [-1, +1] with exponential curve.
        out = (std::exp2(4.0f * phase_) - 1.0f) / 15.0f * 2.0f - 1.0f;
        break;

    case LfoWave::ExpFall:
        // Exponential fall: fast drop, slow tail (mirror of ExpRise).
        out = (std::exp2(4.0f * (1.0f - phase_)) - 1.0f) / 15.0f * 2.0f - 1.0f;
        break;

    default:
        break;
    }

    // Advance phase
    phase_ += phaseInc_;
    if (phase_ >= 1.0f)
        phase_ -= 1.0f;

    return out;
}
