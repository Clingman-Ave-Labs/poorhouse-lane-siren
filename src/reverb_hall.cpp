// reverb_hall.cpp — 8-channel FDN hall reverb
//
// Feedback Delay Network with Hadamard mixing matrix.
// 8 prime-length delay lines produce a dense, smooth reverb tail.
// LP filtering in the feedback path simulates air absorption in
// a large hall — higher frequencies decay faster than lows.

#include "reverb_hall.h"
#include "dsp_utils.h"
#include <cmath>
#include <algorithm>

// Delay line lengths (samples at 48 kHz) — primes chosen to avoid
// common factors, which would create periodic flutter or colouration
static constexpr int DELAY_LENGTHS[8] = {
    1087, 1283, 1481, 1699, 1907, 2131, 2311, 2557
};

// Normalisation factor for the 8×8 Hadamard matrix: 1/sqrt(8)
static constexpr float HADAMARD_NORM = 1.0f / 2.8284271247f;  // 1/sqrt(8)

// ─── DelayLine ──────────────────────────────────────────────────────

void HallReverb::DelayLine::reset()
{
    std::fill(buf.begin(), buf.end(), 0.0f);
    lpState = 0.0f;
}

// ─── In-place Hadamard transform (butterfly implementation) ─────────
//
// Three stages of length-2 butterfly operations implement H8.
// This is equivalent to multiplying by the 8×8 Hadamard matrix but
// requires only 24 additions instead of 56 multiply-adds.
//
// Stage 1: pairs (0,1)(2,3)(4,5)(6,7)
// Stage 2: pairs (0,2)(1,3)(4,6)(5,7)
// Stage 3: pairs (0,4)(1,5)(2,6)(3,7)
// Then normalise by 1/sqrt(8).

void HallReverb::hadamard8(float* x)
{
    float a, b;

    // Stage 1 — adjacent pairs
    a = x[0]; b = x[1]; x[0] = a + b; x[1] = a - b;
    a = x[2]; b = x[3]; x[2] = a + b; x[3] = a - b;
    a = x[4]; b = x[5]; x[4] = a + b; x[5] = a - b;
    a = x[6]; b = x[7]; x[6] = a + b; x[7] = a - b;

    // Stage 2 — stride-2 pairs
    a = x[0]; b = x[2]; x[0] = a + b; x[2] = a - b;
    a = x[1]; b = x[3]; x[1] = a + b; x[3] = a - b;
    a = x[4]; b = x[6]; x[4] = a + b; x[6] = a - b;
    a = x[5]; b = x[7]; x[5] = a + b; x[7] = a - b;

    // Stage 3 — stride-4 pairs
    a = x[0]; b = x[4]; x[0] = a + b; x[4] = a - b;
    a = x[1]; b = x[5]; x[1] = a + b; x[5] = a - b;
    a = x[2]; b = x[6]; x[2] = a + b; x[6] = a - b;
    a = x[3]; b = x[7]; x[3] = a + b; x[7] = a - b;

    // Normalise
    for (int i = 0; i < 8; i++)
        x[i] *= HADAMARD_NORM;
}

// ─── HallReverb ─────────────────────────────────────────────────────

void HallReverb::init(float sampleRate)
{
    sampleRate_ = sampleRate;
    float scale = sampleRate / 48000.0f;

    for (int i = 0; i < NUM_CHANNELS; i++) {
        int len = std::max(1, static_cast<int>(DELAY_LENGTHS[i] * scale));
        lines_[i].buf.assign(len, 0.0f);
        lines_[i].pos     = 0;
        lines_[i].lpState = 0.0f;
    }

    dcIn_  = 0.0f;
    dcOut_ = 0.0f;

    setSize(size_);
}

void HallReverb::updateParams()
{
    // Feedback: 0.3 (small room) to 0.85 (vast hall)
    feedback_ = 0.3f + size_ * 0.55f;

    // LP damping cutoff: 2000 Hz (dark/large) to 8000 Hz (bright/small)
    float lpHz = 2000.0f + (1.0f - size_) * 6000.0f;
    float lp   = 1.0f - std::exp(-2.0f * dsp::PI_F * lpHz / sampleRate_);

    for (int i = 0; i < NUM_CHANNELS; i++)
        lines_[i].lpCoeff = lp;
}

void HallReverb::setSize(float size)
{
    size_ = std::clamp(size, 0.0f, 1.0f);
    updateParams();
}

void HallReverb::setMix(float mix) { mix_ = std::clamp(mix, 0.0f, 1.0f); }

void HallReverb::setSuperDrip(bool /*on*/) { /* no-op for hall reverb */ }

void HallReverb::reset()
{
    for (auto& l : lines_) l.reset();
    dcIn_ = dcOut_ = 0.0f;
}

float HallReverb::process(float input)
{
    // ── Read current outputs from all delay lines ───────────────
    float delayed[NUM_CHANNELS];
    for (int i = 0; i < NUM_CHANNELS; i++)
        delayed[i] = lines_[i].buf[lines_[i].pos];

    // ── Apply Hadamard mixing matrix ────────────────────────────
    // Couples all channels so energy spreads evenly across the FDN,
    // producing a dense, colourless tail.
    hadamard8(delayed);

    // ── Feedback with LP filtering + write new samples ──────────
    for (int i = 0; i < NUM_CHANNELS; i++) {
        // One-pole LP in the feedback path (simulates air absorption)
        float fb = delayed[i] * feedback_;
        lines_[i].lpState += lines_[i].lpCoeff * (fb - lines_[i].lpState);

        // Soft-clip to prevent runaway feedback
        float fbSample = dsp::fast_tanh(lines_[i].lpState);

        // Write input + feedback into delay line
        float in = input * INPUT_GAINS[i] + dsp::sanitize(fbSample);
        lines_[i].buf[lines_[i].pos] = dsp::sanitize(in);

        // Advance write position
        lines_[i].pos = (lines_[i].pos + 1)
                        % static_cast<int>(lines_[i].buf.size());
    }

    // ── Sum output with decorrelation ───────────────────────────
    // Even channels positive, odd channels negative — reduces comb
    // artefacts when summed to mono.
    float wet = 0.0f;
    for (int i = 0; i < NUM_CHANNELS; i++)
        wet += (i & 1) ? -delayed[i] : delayed[i];
    wet *= 0.25f;   // scale: 8 channels, ~half cancel → ×0.25

    // ── DC blocker (~10 Hz high-pass) ───────────────────────────
    float tmp = wet;
    dcOut_ = 0.9995f * (dcOut_ + wet - dcIn_);
    dcIn_  = tmp;
    wet    = dcOut_;

    // ── NaN/Inf protection ──────────────────────────────────────
    if (!std::isfinite(wet)) {
        reset();
        return input;
    }

    return input * (1.0f - mix_) + wet * mix_;
}
