// reverb.cpp — Dual-spring tank reverb
//
// 6 short allpass filters in series create frequency-dependent
// dispersion (the metallic "chirp" / "drip" of a spring).
// Two parallel delay lines with LP-filtered feedback simulate
// the two springs of a classic dub reverb tank.

#include "reverb.h"
#include "dsp_utils.h"
#include <cmath>
#include <algorithm>

// Dispersion allpass delays (samples at 48 kHz) — short primes
// create the characteristic metallic chirp of a spring
static constexpr int DISP_LENGTHS[] = {13, 19, 29, 37, 43, 59};

// Main spring delay lines (samples at 48 kHz)
// Two springs with different lengths for a fuller sound
static constexpr int SPRING_LENGTHS[] = {1559, 2111};

static constexpr float INPUT_GAIN   = 0.35f;    // pre-scale into springs
static constexpr float SPRING_LP_HZ = 4000.0f;  // feedback damping cutoff

// ─── Dispersion allpass ─────────────────────────────────────────────

float Reverb::Allpass::process(float input)
{
    float delayed = buf[pos];
    float w = input - coeff * delayed;

    buf[pos] = dsp::sanitize(w);
    pos = (pos + 1) % static_cast<int>(buf.size());

    return delayed + coeff * w;
}

void Reverb::Allpass::reset()
{
    std::fill(buf.begin(), buf.end(), 0.0f);
}

// ─── Spring delay line ──────────────────────────────────────────────

float Reverb::SpringLine::process(float input)
{
    float out = buf[pos];

    // LP filter in feedback path (spring naturally damps highs)
    lpState += lpCoeff * (out * feedback - lpState);

    buf[pos] = dsp::sanitize(input + dsp::fast_tanh(lpState));
    pos = (pos + 1) % static_cast<int>(buf.size());

    return out;
}

void Reverb::SpringLine::reset()
{
    std::fill(buf.begin(), buf.end(), 0.0f);
    lpState = 0.0f;
}

// ─── Reverb ─────────────────────────────────────────────────────────

void Reverb::init(float sampleRate)
{
    sampleRate_ = sampleRate;
    float scale = sampleRate / 48000.0f;

    for (int i = 0; i < NUM_DISPERSION; i++) {
        int len = std::max(1, static_cast<int>(DISP_LENGTHS[i] * scale));
        dispersion_[i].buf.assign(len, 0.0f);
        dispersion_[i].pos = 0;
    }

    float lp = 1.0f - std::exp(-2.0f * dsp::PI_F * SPRING_LP_HZ
                                / sampleRate);

    for (int i = 0; i < NUM_SPRINGS; i++) {
        int len = std::max(1, static_cast<int>(SPRING_LENGTHS[i] * scale));
        springs_[i].buf.assign(len, 0.0f);
        springs_[i].pos     = 0;
        springs_[i].lpCoeff = lp;
    }

    dcIn_  = 0.0f;
    dcOut_ = 0.0f;

    setSize(size_);
}

void Reverb::setSize(float size)
{
    size_ = std::clamp(size, 0.0f, 1.0f);

    if (drip_) {
        // Super drip: extreme chirp and long lush decay
        for (auto& s : springs_)    s.feedback = 0.94f;
        for (auto& d : dispersion_) d.coeff    = 0.92f;
    } else {
        // Feedback: 0.3 (tight) to 0.85 (long spring)
        float fb = 0.3f + size_ * 0.55f;
        for (auto& s : springs_) s.feedback = fb;

        // Dispersion: more chirp at higher sizes (0.4 – 0.7)
        float disp = 0.4f + size_ * 0.3f;
        for (auto& d : dispersion_) d.coeff = disp;
    }
}

void Reverb::setMix(float mix) { mix_ = std::clamp(mix, 0.0f, 1.0f); }

// ─── Super drip mode ────────────────────────────────────────────────
//
// Emulates the heavy, splashy spring reverb of Jamaican dub music.
// High dispersion for metallic drip, high feedback for long decay,
// darker LP filtering, and hotter input gain for spring saturation.

static constexpr float DRIP_LP_HZ    = 1800.0f;  // darker than normal 4 kHz
static constexpr float DRIP_IN_GAIN  = 0.65f;    // drive springs harder
static constexpr float NORMAL_IN_GAIN = 0.35f;

void Reverb::setSuperDrip(bool on)
{
    if (drip_ == on) return;
    drip_ = on;

    float lpHz = on ? DRIP_LP_HZ : SPRING_LP_HZ;
    float lp   = 1.0f - std::exp(-2.0f * dsp::PI_F * lpHz / sampleRate_);
    for (auto& s : springs_) s.lpCoeff = lp;

    inputGain_ = on ? DRIP_IN_GAIN : NORMAL_IN_GAIN;

    setSize(size_);   // reapply feedback / dispersion with new drip state
}

void Reverb::reset()
{
    for (auto& d : dispersion_) d.reset();
    for (auto& s : springs_)   s.reset();
    dcIn_ = dcOut_ = 0.0f;
}

float Reverb::process(float input)
{
    // Dispersion chain — creates the spring chirp
    float dispersed = input * inputGain_;
    for (auto& d : dispersion_)
        dispersed = d.process(dispersed);

    // Two parallel spring lines
    float wet = 0.0f;
    for (auto& s : springs_)
        wet += s.process(dispersed);
    wet *= 0.5f;

    // DC blocker (~10 Hz high-pass)
    float tmp = wet;
    dcOut_ = 0.9995f * (dcOut_ + wet - dcIn_);
    dcIn_  = tmp;
    wet    = dcOut_;

    if (!std::isfinite(wet)) {
        reset();
        return input;
    }

    return input * (1.0f - mix_) + wet * mix_;
}
