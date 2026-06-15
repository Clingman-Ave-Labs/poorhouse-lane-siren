// delay_digital.cpp — Clean digital delay
//
// Pristine repeats with no tape artifacts.  Immediate delay time
// changes (no spring-damped slew), no wobble/flutter modulation,
// no tape saturation.  Optional HP/LP filtering in the feedback
// path prevents mud buildup and harsh resonance.

#include "delay_digital.h"
#include "dsp_utils.h"
#include <algorithm>
#include <cmath>

static constexpr float TWO_PI = 6.28318530718f;

void DigitalDelay::init(float sampleRate, float maxTimeSec)
{
    sr_ = sampleRate;
    maxSamples_ = static_cast<int>(sampleRate * maxTimeSec) + 1;
    buf_.assign(maxSamples_, 0.0f);
    writePos_     = 0;
    delaySamples_ = 1.0f;

    // Feedback HP 60 Hz (prevent sub buildup)
    hpCoeff_  = std::exp(-TWO_PI * 60.0f / sr_);
    hpState_  = 0.0f;
    hpPrevIn_ = 0.0f;

    // Feedback LP 12 kHz (gentle top end rolloff per repeat)
    lpCoeff_  = 1.0f - std::exp(-TWO_PI * 12000.0f / sr_);
    lpState_  = 0.0f;
}

void DigitalDelay::setTime(float sec)
{
    float samples = sec * sr_;
    delaySamples_ = std::clamp(samples, 1.0f,
                               static_cast<float>(maxSamples_ - 1));
}

void DigitalDelay::setFeedback(float fb)
{
    feedback_ = std::clamp(fb, 0.0f, 0.95f);
}

void DigitalDelay::setMix(float mix)
{
    mix_ = std::clamp(mix, 0.0f, 1.0f);
}

void DigitalDelay::reset()
{
    std::fill(buf_.begin(), buf_.end(), 0.0f);
    writePos_  = 0;
    hpState_   = 0.0f;
    hpPrevIn_  = 0.0f;
    lpState_   = 0.0f;
}

float DigitalDelay::process(float input)
{
    // ── Read with linear interpolation ───────────────────────────────
    float readIdx = static_cast<float>(writePos_) - delaySamples_;
    if (readIdx < 0.0f) readIdx += static_cast<float>(maxSamples_);

    int   idx0 = static_cast<int>(readIdx);
    float frac = readIdx - static_cast<float>(idx0);
    int   idx1 = (idx0 + 1) % maxSamples_;

    float wet = buf_[idx0] * (1.0f - frac) + buf_[idx1] * frac;

    // ── Feedback path: HP → LP (no saturation) ──────────────────────
    float fb = wet * feedback_;

    // High-pass 60 Hz (prevent sub buildup)
    hpState_ = hpCoeff_ * (hpState_ + fb - hpPrevIn_);
    hpPrevIn_ = fb;
    fb = hpState_;

    // Low-pass 12 kHz (gentle per-repeat darkening)
    lpState_ += lpCoeff_ * (fb - lpState_);
    fb = lpState_;

    // NaN/Inf protection
    if (!std::isfinite(fb)) {
        reset();
        return input;
    }

    // ── Write to buffer ─────────────────────────────────────────────
    float writeVal = input + fb;
    buf_[writePos_] = std::clamp(dsp::sanitize(writeVal), -10.0f, 10.0f);
    writePos_ = (writePos_ + 1) % maxSamples_;

    return input * (1.0f - mix_) + wet * mix_;
}
