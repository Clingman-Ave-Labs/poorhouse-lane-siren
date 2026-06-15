// led_driver.cpp — APA106 / WS2811 LED driver
//
// Simple, direct LED control: waveform color is always visible, LFO
// depth/rate modulate brightness. No exponential smoothing — cheap
// WS2811 LEDs show stepping artifacts with crossfades.
//
// AP mode: Andúril-inspired Doppler fly-by in white.
// Desktop: compiles as no-op stubs when HAS_WS2811 is not defined.

#include "led_driver.h"
#include "siren_log.h"

#include <cmath>
#include <algorithm>
#include <chrono>
#include <thread>

#ifdef HAS_WS2811
#include <ws2811.h>
#endif

static constexpr float PI = 3.14159265f;

// ─── Gamma 2.2 lookup table ────────────────────────────────────────
// Pre-computed: out = round(pow(in/255, 2.2) * 255)
// Applied in setLed() so all output paths get perceptually correct brightness.
static constexpr uint8_t GAMMA_LUT[256] = {
      0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   1,
      1,   1,   1,   1,   1,   1,   1,   1,   1,   2,   2,   2,   2,   2,   2,   2,
      3,   3,   3,   3,   3,   4,   4,   4,   4,   5,   5,   5,   5,   6,   6,   6,
      6,   7,   7,   7,   8,   8,   8,   9,   9,   9,  10,  10,  11,  11,  11,  12,
     12,  13,  13,  13,  14,  14,  15,  15,  16,  16,  17,  17,  18,  18,  19,  19,
     20,  20,  21,  22,  22,  23,  23,  24,  25,  25,  26,  26,  27,  28,  28,  29,
     30,  30,  31,  32,  33,  33,  34,  35,  36,  36,  37,  38,  39,  40,  40,  41,
     42,  43,  44,  45,  45,  46,  47,  48,  49,  50,  51,  52,  53,  54,  55,  56,
     57,  58,  59,  60,  61,  62,  63,  64,  65,  66,  67,  68,  69,  70,  72,  73,
     74,  75,  76,  77,  79,  80,  81,  82,  83,  85,  86,  87,  88,  90,  91,  92,
     94,  95,  96,  98,  99, 100, 102, 103, 105, 106, 107, 109, 110, 112, 113, 115,
    116, 118, 119, 121, 122, 124, 125, 127, 129, 130, 132, 133, 135, 137, 138, 140,
    142, 143, 145, 147, 148, 150, 152, 154, 155, 157, 159, 161, 163, 164, 166, 168,
    170, 172, 174, 175, 177, 179, 181, 183, 185, 187, 189, 191, 193, 195, 197, 199,
    201, 203, 205, 207, 209, 211, 213, 215, 217, 219, 221, 224, 226, 228, 230, 232,
    234, 237, 239, 241, 243, 245, 248, 250, 252, 255, 255, 255, 255, 255, 255, 255,
};

// ─── Gamma correction helper ────────────────────────────────────────
// Applies gamma LUT with min floor of 3 to prevent WS2811 PWM flicker.
static constexpr auto gamma_correct = [](uint8_t v) -> uint8_t {
    if (v == 0) return 0;
    uint8_t g = GAMMA_LUT[v];
    return g < 3 ? 3 : g;
};

// ─── RGB color type ─────────────────────────────────────────────────

struct RGB {
    uint8_t r, g, b;
};

// Exponential smoothing (only used by AP idle breathing)
static float smooth(float current, float target, float alpha)
{
    return current + alpha * (target - current);
}

// ─── Waveform color palette ─────────────────────────────────────────

static constexpr RGB WAVEFORM_COLORS[] = {
    {255, 160,  40},    // Sine       → Amber
    {  0, 200, 180},    // Triangle   → Teal
    { 30,  80, 255},    // Square     → Electric Blue
    {255, 100,  10},    // RampUp     → Deep Orange
    {  0, 180, 220},    // RampDown   → Cool Cyan
    {255, 220,  30},    // SampleHold → Bright Yellow
    {220,  40, 255},    // ExpRise    → Magenta
    {255, 180, 200},    // ExpFall    → Soft Pink-White
};
static constexpr int NUM_COLORS = static_cast<int>(std::size(WAVEFORM_COLORS));

// ─── Helpers ────────────────────────────────────────────────────────

static RGB scale(RGB c, float brightness)
{
    brightness = std::clamp(brightness, 0.0f, 1.0f);
    return {
        static_cast<uint8_t>(c.r * brightness),
        static_cast<uint8_t>(c.g * brightness),
        static_cast<uint8_t>(c.b * brightness),
    };
}

static uint32_t pack(RGB c)
{
    return (static_cast<uint32_t>(c.r) << 16)
         | (static_cast<uint32_t>(c.g) << 8)
         |  static_cast<uint32_t>(c.b);
}

// ─── Implementation ─────────────────────────────────────────────────

struct LedDriver::Impl {
#ifdef HAS_WS2811
    ws2811_t strip{};
    bool     hw_init = false;
#endif
    bool     ap_idle = false;
    float    ap_idle_phase = 0.0f;
    float    ap_breath = 0.08f;         // smoothed AP breathing brightness
    float    master_brightness = 1.0f;  // user-configurable brightness cap
    int      render_err_count = 0;

    void setLed(RGB c)
    {
#ifdef HAS_WS2811
        if (!hw_init) return;

        RGB gc = { gamma_correct(c.r), gamma_correct(c.g), gamma_correct(c.b) };
        strip.channel[0].leds[0] = pack(gc);

        // Double-render: second render overwrites any frame corrupted
        // by encoder ground noise during DMA.
        ws2811_render(&strip);
        ws2811_return_t ret = ws2811_render(&strip);
        if (ret != WS2811_SUCCESS) {
            if (render_err_count++ < 10)
                slog("LED: render failed: %s", ws2811_get_return_t_str(ret));
        }
#else
        (void)c;
#endif
    }

    void setLedOff()
    {
        setLed({0, 0, 0});
    }

    void sleepMs(int ms)
    {
        std::this_thread::sleep_for(std::chrono::milliseconds(ms));
    }
};

// ─── Public interface ───────────────────────────────────────────────

LedDriver::LedDriver() : pimpl_(std::make_unique<Impl>()) {}
LedDriver::~LedDriver() { shutdown(); }

void LedDriver::setBrightness(float level)
{
    pimpl_->master_brightness = std::clamp(level, 0.0f, 1.0f);
}

bool LedDriver::init(int gpio_pin)
{
#ifdef HAS_WS2811
    auto& s = pimpl_->strip;
    s.freq   = WS2811_TARGET_FREQ;
    s.dmanum = 10;

    s.channel[0].gpionum    = gpio_pin;
    s.channel[0].count      = 1;
    s.channel[0].invert     = 0;
    s.channel[0].brightness = 255;
    s.channel[0].strip_type = WS2811_STRIP_GRB;

    s.channel[1].gpionum = 0;
    s.channel[1].count   = 0;

    ws2811_return_t ret = ws2811_init(&s);
    if (ret != WS2811_SUCCESS) {
        slog("LED: ws2811_init failed: %s", ws2811_get_return_t_str(ret));
        return false;
    }
    pimpl_->hw_init = true;
    slog("LED: Initialised on GPIO %d (DMA %d, freq %d)",
         gpio_pin, s.dmanum, s.freq);

    // Startup self-test: smooth fade through R → G → B
    RGB test_colors[] = {{255,0,0}, {0,255,0}, {0,0,255}};
    const char* test_names[] = {"RED", "GREEN", "BLUE"};
    for (int i = 0; i < 3; i++) {
        // Fade in
        for (int step = 0; step <= 10; step++) {
            float t = static_cast<float>(step) / 10.0f;
            pimpl_->setLed(scale(test_colors[i], t * 0.25f));
            pimpl_->sleepMs(20);
        }
        slog("LED: self-test %s (0x%06X → render)",
             test_names[i], pimpl_->strip.channel[0].leds[0]);
        pimpl_->sleepMs(150);
        // Fade out
        for (int step = 10; step >= 0; step--) {
            float t = static_cast<float>(step) / 10.0f;
            pimpl_->setLed(scale(test_colors[i], t * 0.25f));
            pimpl_->sleepMs(20);
        }
    }
    pimpl_->setLedOff();
    slog("LED: self-test complete");

    return true;
#else
    (void)gpio_pin;
    slog("LED: No ws2811 — stub mode");
    return true;
#endif
}

void LedDriver::update(LfoWave wave, float lfo_out, float lfo_depth,
                        bool gate, float freq, float lfo_rate)
{
    // AP idle mode: gentle white breathing
    if (pimpl_->ap_idle) {
        pimpl_->ap_idle_phase += 0.005f;  // ~0.3 Hz at 60 Hz update rate
        if (pimpl_->ap_idle_phase >= 1.0f)
            pimpl_->ap_idle_phase -= 1.0f;

        float phase = std::sin(2.0f * PI * pimpl_->ap_idle_phase);
        float breath = 0.06f + 0.20f * (phase * phase) * (phase > 0 ? 1.0f : -0.3f);
        breath = std::clamp(breath, 0.04f, 0.26f);

        pimpl_->ap_breath = smooth(pimpl_->ap_breath, breath, 0.050f);
        pimpl_->setLed(scale({255, 255, 255}, pimpl_->ap_breath * pimpl_->master_brightness));
        return;
    }

    // ── Color: snap directly from waveform — no crossfade ───────
    int idx = static_cast<int>(wave);
    if (idx < 0 || idx >= NUM_COLORS) idx = 0;
    RGB color = WAVEFORM_COLORS[idx];

    // ── LFO: map [-1,+1] → [0,1], use directly — no smoothing ──
    float lfo_norm = (lfo_out + 1.0f) * 0.5f;

    // ── Brightness: LFO depth modulates around base level ───────
    float brightness;
    if (gate) {
        // Active: 100% at LFO peak, dips with depth (range 15%–100%)
        brightness = 1.0f - lfo_depth * 0.85f * (1.0f - lfo_norm);
    } else {
        // Idle: clearly visible base + LFO modulation (range 40%–75%)
        brightness = 0.40f + lfo_depth * 0.35f * lfo_norm;
    }

    pimpl_->setLed(scale(color, brightness * pimpl_->master_brightness));
    (void)freq; (void)lfo_rate;
}

void LedDriver::blinkSave()
{
    // Smooth pulse rather than hard blink: fade up, hold, fade down × 3
    for (int i = 0; i < 3; i++) {
        // Fade up
        for (int step = 0; step <= 8; step++) {
            float t = static_cast<float>(step) / 8.0f;
            // Ease-in curve for organic feel
            t = t * t;
            pimpl_->setLed(scale({255, 255, 255}, t * 0.30f));
            pimpl_->sleepMs(12);
        }
        // Brief hold at peak
        pimpl_->sleepMs(30);
        // Fade down
        for (int step = 8; step >= 0; step--) {
            float t = static_cast<float>(step) / 8.0f;
            pimpl_->setLed(scale({255, 255, 255}, t * 0.30f));
            pimpl_->sleepMs(12);
        }
        if (i < 2) pimpl_->sleepMs(40);
    }
}

void LedDriver::playApEnter()
{
    // Andúril-inspired Doppler fly-by: white light simulates a
    // transmission approaching, peaking, and departing.
    //
    // Asymmetric: slow cubic approach → brief peak → fast quadratic departure
    // Duration: ~1.5s (75 steps at 20ms each)

    constexpr int   STEPS    = 75;
    constexpr int   STEP_MS  = 20;
    constexpr float PEAK_POS = 0.6f;

    for (int i = 0; i <= STEPS; i++) {
        float t = static_cast<float>(i) / STEPS;
        float brightness;

        if (t <= PEAK_POS) {
            float tn = t / PEAK_POS;
            brightness = tn * tn * tn;
        } else if (t <= PEAK_POS + 0.1f) {
            brightness = 1.0f;
        } else {
            float tn = (t - PEAK_POS - 0.1f) / (1.0f - PEAK_POS - 0.1f);
            float inv = 1.0f - tn;
            brightness = inv * inv;
        }

        pimpl_->setLed(scale({255, 255, 255}, brightness));
        pimpl_->sleepMs(STEP_MS);
    }

    pimpl_->setLedOff();
}

void LedDriver::playApExit()
{
    // Reverse Doppler: fast approach, slow departure

    constexpr int   STEPS    = 75;
    constexpr int   STEP_MS  = 20;
    constexpr float PEAK_POS = 0.3f;

    for (int i = 0; i <= STEPS; i++) {
        float t = static_cast<float>(i) / STEPS;
        float brightness;

        if (t <= PEAK_POS) {
            float tn = t / PEAK_POS;
            brightness = tn * tn;
        } else if (t <= PEAK_POS + 0.1f) {
            brightness = 1.0f;
        } else {
            float tn = (t - PEAK_POS - 0.1f) / (1.0f - PEAK_POS - 0.1f);
            float inv = 1.0f - tn;
            brightness = inv * inv * inv;
        }

        pimpl_->setLed(scale({255, 255, 255}, brightness));
        pimpl_->sleepMs(STEP_MS);
    }

    pimpl_->setLedOff();
}

void LedDriver::setApIdle(bool active)
{
    pimpl_->ap_idle = active;
    if (active)
        pimpl_->ap_idle_phase = 0.0f;
}

void LedDriver::shutdown()
{
#ifdef HAS_WS2811
    if (pimpl_ && pimpl_->hw_init) {
        pimpl_->setLedOff();
        ws2811_fini(&pimpl_->strip);
        pimpl_->hw_init = false;
        slog("LED: Shutdown");
    }
#endif
}
