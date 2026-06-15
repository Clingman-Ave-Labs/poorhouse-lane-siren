// reverb_schroeder.cpp — Schroeder reverb (1962)
//
// Classic topology: 4 parallel comb filters with LP-filtered feedback
// summed into 2 series allpass diffusers.  Produces the characteristic
// metallic, colored tail of early digital reverb units.

#include "reverb_schroeder.h"
#include "dsp_utils.h"
#include <cmath>
#include <algorithm>

// Comb filter delay lengths (samples at 48 kHz)
// These mutually prime lengths avoid reinforcing common modes
static constexpr int COMB_LENGTHS[] = {1557, 1617, 1491, 1422};

// Allpass diffuser delay lengths (samples at 48 kHz)
static constexpr int AP_LENGTHS[] = {225, 556};

// LP cutoff for comb feedback damping (Hz)
static constexpr float COMB_LP_HZ = 5000.0f;

// ─── Comb filter ───────────────────────────────────────────────────

float SchroederReverb::CombFilter::process(float input)
{
    float out = buf[pos];

    // One-pole LP in feedback path — natural HF damping
    lpState += lpCoeff * (out * feedback - lpState);

    buf[pos] = dsp::sanitize(input + dsp::fast_tanh(lpState));
    pos = (pos + 1) % static_cast<int>(buf.size());

    return out;
}

void SchroederReverb::CombFilter::reset()
{
    std::fill(buf.begin(), buf.end(), 0.0f);
    lpState = 0.0f;
}

// ─── Allpass diffuser ──────────────────────────────────────────────

float SchroederReverb::Allpass::process(float input)
{
    float delayed = buf[pos];
    float w = input - coeff * delayed;

    buf[pos] = dsp::sanitize(w);
    pos = (pos + 1) % static_cast<int>(buf.size());

    return delayed + coeff * w;
}

void SchroederReverb::Allpass::reset()
{
    std::fill(buf.begin(), buf.end(), 0.0f);
}

// ─── SchroederReverb ───────────────────────────────────────────────

void SchroederReverb::init(float sampleRate)
{
    sampleRate_ = sampleRate;
    float scale = sampleRate / 48000.0f;

    // LP coefficient for comb feedback damping
    float lp = 1.0f - std::exp(-2.0f * dsp::PI_F * COMB_LP_HZ
                                / sampleRate);

    for (int i = 0; i < NUM_COMBS; i++) {
        int len = std::max(1, static_cast<int>(COMB_LENGTHS[i] * scale));
        combs_[i].buf.assign(len, 0.0f);
        combs_[i].pos     = 0;
        combs_[i].lpCoeff = lp;
    }

    for (int i = 0; i < NUM_ALLPASS; i++) {
        int len = std::max(1, static_cast<int>(AP_LENGTHS[i] * scale));
        allpasses_[i].buf.assign(len, 0.0f);
        allpasses_[i].pos   = 0;
        allpasses_[i].coeff = 0.5f;
    }

    dcIn_  = 0.0f;
    dcOut_ = 0.0f;

    setSize(size_);
}

void SchroederReverb::setSize(float size)
{
    size_ = std::clamp(size, 0.0f, 1.0f);

    // Feedback: 0.70 (short, tight) to 0.95 (long, washy)
    float fb = 0.70f + size_ * 0.25f;
    for (auto& c : combs_) c.feedback = fb;

    // LP damping: higher size → lower cutoff → darker tail
    // Scale LP coefficient so larger size damps HF more
    float lpHz = 5000.0f - size_ * 2500.0f;   // 5 kHz → 2.5 kHz
    float lp   = 1.0f - std::exp(-2.0f * dsp::PI_F * lpHz
                                  / sampleRate_);
    for (auto& c : combs_) c.lpCoeff = lp;
}

void SchroederReverb::setMix(float mix)
{
    mix_ = std::clamp(mix, 0.0f, 1.0f);
}

void SchroederReverb::setSuperDrip(bool /*on*/)
{
    // No-op — Schroeder reverb has no spring drip analogue
}

void SchroederReverb::reset()
{
    for (auto& c : combs_)     c.reset();
    for (auto& a : allpasses_) a.reset();
    dcIn_ = dcOut_ = 0.0f;
}

float SchroederReverb::process(float input)
{
    // Sum of 4 parallel comb filters
    float combSum = 0.0f;
    for (auto& c : combs_)
        combSum += c.process(input);
    combSum *= 0.25f;   // normalise

    // 2 series allpass diffusers
    float diffused = combSum;
    for (auto& a : allpasses_)
        diffused = a.process(diffused);

    // DC blocker (~10 Hz high-pass)
    float tmp = diffused;
    dcOut_ = 0.9995f * (dcOut_ + diffused - dcIn_);
    dcIn_  = tmp;
    float wet = dcOut_;

    // NaN / Inf safety — reset state if anything blows up
    if (!std::isfinite(wet)) {
        reset();
        return input;
    }

    return input * (1.0f - mix_) + wet * mix_;
}
