// reverb_plate.cpp — Dattorro plate reverb
//
// 4 input allpass diffusers in series smear transients into a
// dense initial reflection pattern.  A tank section with two
// cross-coupled paths (each: modulated allpass → delay → LP filter)
// builds the sustained, lush tail characteristic of plate reverbs.
//
// Modulated allpasses add subtle pitch variation (chorus) to avoid
// metallic ringing.  LP filtering in the feedback loop simulates
// the natural high-frequency damping of a steel plate.

#include "reverb_plate.h"
#include "dsp_utils.h"
#include <cmath>
#include <algorithm>

// ─── Reference delays (samples at 48 kHz) ──────────────────────────

// Input diffuser allpass delays — primes for maximal diffusion
static constexpr int DIFF_LENGTHS[] = {142, 107, 379, 277};
static constexpr float DIFF_COEFF   = 0.625f;  // diffusion coefficient

// Tank modulated allpass delays
static constexpr int TANK_AP_LENGTHS[] = {672, 908};

// Tank delay line lengths
static constexpr int TANK_DL_LENGTHS[] = {4453, 3720};

// Modulation parameters
static constexpr float LFO_RATE_HZ = 0.8f;     // subtle chorus
static constexpr float LFO_DEPTH   = 12.0f;    // samples at 48 kHz

// Feedback damping
static constexpr float DAMP_LP_HZ  = 5500.0f;  // plate has brighter tail than springs
static constexpr float INPUT_GAIN  = 0.35f;

// Tank tap positions for stereo decorrelation (used as mono sum here)
// Taps taken from delay lines at various points for dense output
static constexpr int TAP_OFFSETS_A[] = {266, 2974};  // from tank 0 delay
static constexpr int TAP_OFFSETS_B[] = {353, 3627};   // from tank 1 delay

// ─── Input allpass diffuser ─────────────────────────────────────────

float PlateReverb::Allpass::process(float input)
{
    float delayed = buf[pos];
    float w = input - coeff * delayed;

    buf[pos] = dsp::sanitize(w);
    pos = (pos + 1) % static_cast<int>(buf.size());

    return delayed + coeff * w;
}

void PlateReverb::Allpass::reset()
{
    std::fill(buf.begin(), buf.end(), 0.0f);
    pos = 0;
}

// ─── Modulated allpass (tank chorus) ────────────────────────────────

float PlateReverb::ModAllpass::process(float input)
{
    // Advance LFO
    phase += lfoRate;
    if (phase >= 6.2831853f) phase -= 6.2831853f;

    // Modulated read position: base delay +/- LFO
    float modOffset = lfoAmt * std::sin(phase);
    float readPos   = static_cast<float>(wPos) - static_cast<float>(baseDel)
                      - modOffset;
    if (readPos < 0.0f) readPos += static_cast<float>(size);

    // Linear interpolation for fractional delay
    int   rIdx0 = static_cast<int>(readPos) % size;
    int   rIdx1 = (rIdx0 + 1) % size;
    float frac  = readPos - std::floor(readPos);

    float delayed = buf[rIdx0] + frac * (buf[rIdx1] - buf[rIdx0]);

    // Allpass topology
    float w = input - coeff * delayed;
    buf[wPos] = dsp::sanitize(w);
    wPos = (wPos + 1) % size;

    return delayed + coeff * w;
}

void PlateReverb::ModAllpass::reset()
{
    std::fill(buf.begin(), buf.end(), 0.0f);
    wPos  = 0;
    phase = 0.0f;
}

// ─── Simple delay line ──────────────────────────────────────────────

float PlateReverb::Delay::read() const
{
    return buf[pos];
}

void PlateReverb::Delay::write(float v)
{
    buf[pos] = dsp::sanitize(v);
}

void PlateReverb::Delay::advance()
{
    pos = (pos + 1) % static_cast<int>(buf.size());
}

void PlateReverb::Delay::reset()
{
    std::fill(buf.begin(), buf.end(), 0.0f);
    pos = 0;
}

// ─── PlateReverb ────────────────────────────────────────────────────

void PlateReverb::init(float sampleRate)
{
    sampleRate_ = sampleRate;
    float scale = sampleRate / 48000.0f;

    // Input diffuser allpasses
    for (int i = 0; i < NUM_DIFFUSERS; i++) {
        int len = std::max(1, static_cast<int>(DIFF_LENGTHS[i] * scale));
        diffusers_[i].buf.assign(len, 0.0f);
        diffusers_[i].pos   = 0;
        diffusers_[i].coeff = DIFF_COEFF;
    }

    // LP coefficient for feedback damping
    float lp = 1.0f - std::exp(-2.0f * dsp::PI_F * DAMP_LP_HZ / sampleRate);

    // LFO increment per sample
    float lfoInc = 2.0f * dsp::PI_F * LFO_RATE_HZ / sampleRate;
    float lfoAmt = LFO_DEPTH * scale;

    // Tank halves
    for (int i = 0; i < 2; i++) {
        // Modulated allpass
        int apLen  = std::max(1, static_cast<int>(TANK_AP_LENGTHS[i] * scale));
        // Extra headroom for modulation swing
        int apBuf  = apLen + static_cast<int>(lfoAmt) + 4;
        tank_[i].modAp.buf.assign(apBuf, 0.0f);
        tank_[i].modAp.size    = apBuf;
        tank_[i].modAp.wPos    = 0;
        tank_[i].modAp.baseDel = apLen;
        tank_[i].modAp.coeff   = 0.6f;
        tank_[i].modAp.lfoRate = lfoInc;
        tank_[i].modAp.lfoAmt  = lfoAmt;
        // Offset LFO phase between the two halves for decorrelation
        tank_[i].modAp.phase   = (i == 0) ? 0.0f : dsp::PI_F;

        // Delay line
        int dlLen = std::max(1, static_cast<int>(TANK_DL_LENGTHS[i] * scale));
        tank_[i].delay.buf.assign(dlLen, 0.0f);
        tank_[i].delay.pos = 0;

        // LP filter
        tank_[i].lpCoeff = lp;
        tank_[i].lpState = 0.0f;
    }

    dcIn_  = 0.0f;
    dcOut_ = 0.0f;

    setSize(size_);
}

void PlateReverb::setSize(float size)
{
    size_ = std::clamp(size, 0.0f, 1.0f);

    // Feedback: 0.3 (tight room) → 0.85 (long lush plate)
    float fb = 0.3f + size_ * 0.55f;
    for (auto& t : tank_) t.feedback = fb;

    // Diffusion: tighter at low sizes, more smeared at high sizes
    float diff = 0.55f + size_ * 0.2f;  // 0.55 – 0.75
    for (auto& d : diffusers_) d.coeff = diff;

    // Tank allpass diffusion follows main diffusion
    for (auto& t : tank_) t.modAp.coeff = 0.5f + size_ * 0.15f;
}

void PlateReverb::setMix(float mix) { mix_ = std::clamp(mix, 0.0f, 1.0f); }

void PlateReverb::setSuperDrip(bool /*on*/)
{
    // No-op — plate reverb has no spring drip mode
}

void PlateReverb::reset()
{
    for (auto& d : diffusers_) d.reset();
    for (auto& t : tank_) {
        t.modAp.reset();
        t.delay.reset();
        t.lpState = 0.0f;
    }
    dcIn_ = dcOut_ = 0.0f;
}

float PlateReverb::process(float input)
{
    // ── Input diffusion chain ───────────────────────────────────────
    float diffused = input * INPUT_GAIN;
    for (auto& d : diffusers_)
        diffused = d.process(diffused);

    // ── Tank processing ─────────────────────────────────────────────
    // Read cross-feedback from the opposite tank half's delay line
    float crossA = tank_[1].delay.read();  // feeds into tank 0
    float crossB = tank_[0].delay.read();  // feeds into tank 1

    for (int i = 0; i < 2; i++) {
        float cross = (i == 0) ? crossA : crossB;

        // Mix diffused input with cross-feedback
        float tankIn = diffused + tank_[i].feedback * cross;
        tankIn = dsp::fast_tanh(tankIn);  // soft-clip to prevent blowup

        // Modulated allpass
        float apOut = tank_[i].modAp.process(tankIn);

        // LP filter (damping in feedback path)
        tank_[i].lpState += tank_[i].lpCoeff
                          * (apOut - tank_[i].lpState);

        // Write into delay line
        tank_[i].delay.write(tank_[i].lpState);
        tank_[i].delay.advance();
    }

    // ── Output taps ─────────────────────────────────────────────────
    // Multiple taps from both delay lines for dense output
    float wet = 0.0f;
    float scale = sampleRate_ / 48000.0f;

    auto readTap = [&](const Delay& dl, int offsetRef) -> float {
        int offset = std::max(1, static_cast<int>(offsetRef * scale));
        int sz     = static_cast<int>(dl.buf.size());
        offset     = std::min(offset, sz - 1);
        int idx    = (dl.pos - offset + sz) % sz;
        return dl.buf[idx];
    };

    // Taps from tank 0
    for (int t : TAP_OFFSETS_A)
        wet += readTap(tank_[0].delay, t);

    // Taps from tank 1
    for (int t : TAP_OFFSETS_B)
        wet += readTap(tank_[1].delay, t);

    wet *= 0.25f;  // average 4 taps

    // ── DC blocker (~10 Hz high-pass) ───────────────────────────────
    float tmp = wet;
    dcOut_ = 0.9995f * (dcOut_ + wet - dcIn_);
    dcIn_  = tmp;
    wet    = dcOut_;

    // ── NaN / Inf protection ────────────────────────────────────────
    if (!std::isfinite(wet)) {
        reset();
        return input;
    }

    return input * (1.0f - mix_) + wet * mix_;
}
