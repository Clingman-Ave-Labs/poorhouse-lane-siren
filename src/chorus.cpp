#include "chorus.h"
#include "dsp_utils.h"
#include <cmath>
#include <algorithm>

void Chorus::init(float sampleRate)
{
    sr_ = sampleRate;
    // Buffer large enough for base delay + max modulation
    bufSize_ = static_cast<int>((BASE_DELAY_MS + MOD_DEPTH_MS + 5.0f)
                                * sr_ / 1000.0f) + 2;
    buf_.assign(bufSize_, 0.0f);
    lfoInc_ = rate_ / sr_;
    // Offset voice phases for wider image
    lfoPhase_[0] = 0.0f;
    lfoPhase_[1] = 0.5f;
    writePos_ = 0;
}

void Chorus::setRate(float hz)
{
    rate_ = std::clamp(hz, 0.1f, 5.0f);
    lfoInc_ = rate_ / sr_;
}

void Chorus::setDepth(float depth)
{
    depth_ = std::clamp(depth, 0.0f, 1.0f);
}

void Chorus::setMix(float mix)
{
    mix_ = std::clamp(mix, 0.0f, 1.0f);
}

float Chorus::process(float input)
{
    // Write input to delay buffer
    buf_[writePos_] = input;

    float wet = 0.0f;

    for (int v = 0; v < NUM_VOICES; v++) {
        // Sine LFO per voice
        float lfo = std::sin(2.0f * static_cast<float>(M_PI) * lfoPhase_[v]);

        // Modulated delay in samples
        float delaySamples = (BASE_DELAY_MS + lfo * MOD_DEPTH_MS * depth_)
                             * sr_ / 1000.0f;
        delaySamples = std::max(delaySamples, 1.0f);

        // Fractional read position with linear interpolation
        float readF = static_cast<float>(writePos_) - delaySamples;
        if (readF < 0.0f) readF += static_cast<float>(bufSize_);

        int idx0 = static_cast<int>(readF);
        int idx1 = (idx0 + 1) % bufSize_;
        float frac = readF - static_cast<float>(idx0);
        idx0 = idx0 % bufSize_;

        float sample = buf_[idx0] * (1.0f - frac) + buf_[idx1] * frac;
        wet += dsp::sanitize(sample);

        // Advance LFO
        lfoPhase_[v] += lfoInc_;
        if (lfoPhase_[v] >= 1.0f) lfoPhase_[v] -= 1.0f;
    }

    // Average voices
    wet *= (1.0f / NUM_VOICES);

    // Advance write position
    writePos_ = (writePos_ + 1) % bufSize_;

    return input * (1.0f - mix_) + wet * mix_;
}

void Chorus::reset()
{
    std::fill(buf_.begin(), buf_.end(), 0.0f);
    writePos_ = 0;
    lfoPhase_[0] = 0.0f;
    lfoPhase_[1] = 0.5f;
}
