// filter.cpp — Simplified Moog ladder (4-pole LP with tanh saturation)

#include "filter.h"
#include "dsp_utils.h"
#include <cmath>
#include <algorithm>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

void MoogFilter::setSampleRate(float sr)
{
    sr_ = sr;
    // Recompute cached coefficient with new rate
    setCutoff(cutoff_);
}

void MoogFilter::setCutoff(float hz)
{
    cutoff_ = hz;
    float fc = std::min(cutoff_, sr_ * 0.45f);
    f_ = 2.0f * std::sin(static_cast<float>(M_PI) * fc / sr_);
}

void MoogFilter::setResonance(float r)
{
    reso_ = r;
    k_ = 4.0f * r;     // 0–0.95 → 0–3.8
}

void MoogFilter::reset()
{
    for (auto& s : stage_) s = 0.0f;
}

float MoogFilter::process(float input)
{
    // Resonance feedback
    input -= k_ * stage_[3];

    // Four cascaded one-pole stages with tanh saturation
    stage_[0] += f_ * (dsp::fast_tanh(input)    - dsp::fast_tanh(stage_[0]));
    stage_[1] += f_ * (dsp::fast_tanh(stage_[0]) - dsp::fast_tanh(stage_[1]));
    stage_[2] += f_ * (dsp::fast_tanh(stage_[1]) - dsp::fast_tanh(stage_[2]));
    stage_[3] += f_ * (dsp::fast_tanh(stage_[2]) - dsp::fast_tanh(stage_[3]));

    // NaN/Inf protection — graceful reset
    for (auto& s : stage_) {
        if (!std::isfinite(s)) { reset(); return 0.0f; }
    }

    return stage_[3];
}
