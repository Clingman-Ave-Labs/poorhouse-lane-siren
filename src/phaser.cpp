#include "phaser.h"
#include "dsp_utils.h"
#include <cmath>
#include <algorithm>

// ─── AllpassStage ────────────────────────────────────────────────────

float Phaser::AllpassStage::process(float in, float coeff)
{
    float out = -coeff * in + z1;
    z1 = dsp::sanitize(in + coeff * out);
    return out;
}

// ─── Phaser ──────────────────────────────────────────────────────────

void Phaser::init(float sampleRate)
{
    sr_ = sampleRate;
    lfoInc_ = rate_ / sr_;
    reset();
}

void Phaser::setRate(float hz)
{
    rate_ = std::clamp(hz, 0.05f, 5.0f);
    lfoInc_ = rate_ / sr_;
}

void Phaser::setDepth(float depth)
{
    depth_ = std::clamp(depth, 0.0f, 1.0f);
}

void Phaser::setFeedback(float fb)
{
    feedback_ = std::clamp(fb, -0.95f, 0.95f);
}

void Phaser::setMix(float mix)
{
    mix_ = std::clamp(mix, 0.0f, 1.0f);
}

float Phaser::process(float input)
{
    // Triangle LFO (0–1 range) — smoother sweep than sine for phaser
    float phase2 = lfoPhase_ * 2.0f;
    float lfo = (phase2 < 1.0f) ? phase2 : (2.0f - phase2);

    // Scale by depth
    lfo *= depth_;

    // Logarithmic frequency sweep between MIN_FREQ and MAX_FREQ
    float logMin = std::log2(MIN_FREQ);
    float logMax = std::log2(MAX_FREQ);
    float sweepFreq = std::exp2(logMin + lfo * (logMax - logMin));

    // Convert frequency to allpass coefficient
    // a = (tan(pi*f/sr) - 1) / (tan(pi*f/sr) + 1)
    float w = std::tan(static_cast<float>(M_PI) * sweepFreq / sr_);
    float coeff = (w - 1.0f) / (w + 1.0f);

    // Apply feedback
    float in = dsp::sanitize(input + fbState_ * feedback_);

    // Cascade through allpass stages
    float out = in;
    for (int i = 0; i < NUM_STAGES; i++)
        out = stages_[i].process(out, coeff);

    fbState_ = dsp::sanitize(out);

    // Advance LFO
    lfoPhase_ += lfoInc_;
    if (lfoPhase_ >= 1.0f) lfoPhase_ -= 1.0f;

    // Wet/dry mix
    return input * (1.0f - mix_) + out * mix_;
}

void Phaser::reset()
{
    lfoPhase_ = 0.0f;
    fbState_ = 0.0f;
    for (auto& s : stages_) s.z1 = 0.0f;
}
