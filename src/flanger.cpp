#include "flanger.h"
#include "dsp_utils.h"
#include <cmath>
#include <algorithm>

void Flanger::init(float sampleRate)
{
    sr_ = sampleRate;
    bufSize_ = static_cast<int>((BASE_DELAY_MS + MOD_DEPTH_MS + 2.0f)
                                * sr_ / 1000.0f) + 2;
    buf_.assign(bufSize_, 0.0f);
    lfoInc_ = rate_ / sr_;
    lfoPhase_ = 0.0f;
    writePos_ = 0;
    fbState_ = 0.0f;
}

void Flanger::setRate(float hz)
{
    rate_ = std::clamp(hz, 0.05f, 5.0f);
    lfoInc_ = rate_ / sr_;
}

void Flanger::setDepth(float depth)
{
    depth_ = std::clamp(depth, 0.0f, 1.0f);
}

void Flanger::setFeedback(float fb)
{
    feedback_ = std::clamp(fb, -0.95f, 0.95f);
}

void Flanger::setMix(float mix)
{
    mix_ = std::clamp(mix, 0.0f, 1.0f);
}

float Flanger::process(float input)
{
    // Triangle LFO (0–1) — smoother flanging sweep
    float phase2 = lfoPhase_ * 2.0f;
    float lfo = (phase2 < 1.0f) ? phase2 : (2.0f - phase2);

    // Modulated delay in samples
    float delaySamples = (BASE_DELAY_MS + lfo * MOD_DEPTH_MS * depth_)
                         * sr_ / 1000.0f;
    delaySamples = std::max(delaySamples, 0.5f);

    // Write input + feedback to buffer
    buf_[writePos_] = dsp::sanitize(input + fbState_ * feedback_);

    // Fractional read with linear interpolation
    float readF = static_cast<float>(writePos_) - delaySamples;
    if (readF < 0.0f) readF += static_cast<float>(bufSize_);

    int idx0 = static_cast<int>(readF);
    int idx1 = (idx0 + 1) % bufSize_;
    float frac = readF - static_cast<float>(idx0);
    idx0 = idx0 % bufSize_;

    float wet = buf_[idx0] * (1.0f - frac) + buf_[idx1] * frac;
    wet = dsp::sanitize(wet);

    fbState_ = wet;

    // Advance LFO
    lfoPhase_ += lfoInc_;
    if (lfoPhase_ >= 1.0f) lfoPhase_ -= 1.0f;

    // Advance write position
    writePos_ = (writePos_ + 1) % bufSize_;

    return input * (1.0f - mix_) + wet * mix_;
}

void Flanger::reset()
{
    std::fill(buf_.begin(), buf_.end(), 0.0f);
    writePos_ = 0;
    lfoPhase_ = 0.0f;
    fbState_ = 0.0f;
}
