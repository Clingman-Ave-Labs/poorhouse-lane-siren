// main.cpp — Milestone 5: Full DSP Engine + Parameter Mapping
//
// Usage:
//   dubsiren [--device <alsa_hw>] [--simulate]
//
// Signal chain:
//   LFO + Pitch-env → Oscillator → Envelope → Filter(+env) → Delay → Reverb → Limiter → DAC
//
// Encoder banks (Shift toggles A / B):
//   Enc 1:  A = Base Freq (30–8000 Hz)     B = LFO Depth (0–100%)
//   Enc 2:  A = LFO Rate (0.1–20 Hz)       B = Release Time (10 ms–5 s)
//   Enc 3:  A = Filter Cutoff (20–20 kHz)   B = Filter Resonance (0–95%)
//   Enc 4:  A = Delay Time (1 ms–1.0 s)     B = Delay Mix (0–100%)
//   Enc 5:  A = Delay Feedback (0–95%)      B = Reverb Mix (0–100%)
//
// Buttons:
//   Trigger  (GPIO 4)   Gate volume envelope; resets LFO phase
//   Shift    (GPIO 15)  Hold for Bank B; double-click = standard bank
//                        triple-click = experimental bank
//   Preset   (GPIO 5)   Cycle dub siren preset; Shift+Preset = cycle LFO shape
//                        Long-press (3s) = save to user bank
//
// Preset banks (4 presets each):
//   User:         Boot default — 4 saveable slots (long-press Preset to save)
//   Standard:     Double-click Shift — factory presets
//   Experimental: Triple-click Shift — extreme/crazy presets
//
// LFO shapes (Shift+GPIO 5):
//   Sine → Triangle → Square → RampUp → RampDown → S&H → ExpRise → ExpFall
//
// Pitch envelope switch (GPIO 9/10):
//   Rise / Off / Fall — sweeps pitch on trigger release
//   Filter always darkens on release (DS71-style), independent of switch
//   Preset loads set pitch env; physical switch overrides until next load
//   Triple-tap to Fall = toggle LFO-pitch link (LFO rate follows envelope)
//   Shift + triple-tap to Fall = toggle super drip reverb (heavy dub spring)

#include <cstdio>
#include <cstring>
#include <csignal>
#include <cmath>
#include <cstdlib>
#include <sys/wait.h>
#include <array>
#include <atomic>
#include <algorithm>
#include <chrono>
#include <mutex>
#include <string>
#include <sstream>
#include <thread>
#include <map>

#include <sys/stat.h>
#include <unistd.h>
#include <pwd.h>
#include <fcntl.h>
#include <cerrno>
#include <climits>

#include "gpio_hw.h"
#include "oscillator.h"
#include "audio_engine.h"
#include "lfo.h"
#include "filter.h"
#include "delay.h"
#include "reverb.h"
#include "reverb_plate.h"
#include "reverb_hall.h"
#include "reverb_schroeder.h"
#include "delay_digital.h"
#include "dsp_utils.h"
#include "phaser.h"
#include "chorus.h"
#include "flanger.h"
#include "tape_saturator.h"
#include "ap_mode.h"
#include "web_server.h"
#include "led_driver.h"
#include "siren_log.h"
#include "git_version.h"

static volatile sig_atomic_t g_running = 1;

static void on_signal(int) { g_running = 0; }

// ─── Shared state (main thread → audio thread) ─────────────────────

// Bank A
static std::atomic<float> g_freq{440.0f};
static std::atomic<float> g_lfo_rate{0.35f};
static std::atomic<float> g_filter_cutoff{8000.0f};
static std::atomic<float> g_delay_time{0.375f};
static std::atomic<float> g_delay_feedback{0.55f};

// Bank B
static std::atomic<float> g_lfo_depth{0.35f};
static std::atomic<float> g_release_time{0.050f};  // 50 ms default
static std::atomic<float> g_filter_reso{0.0f};
static std::atomic<float> g_delay_mix{0.30f};
static std::atomic<float> g_reverb_mix{0.35f};
static std::atomic<float> g_sweep_dir{-1.0f};  // filter sweep: -1=Down, 0=Flat, +1=Up

// Fixed (no encoder control)
static constexpr float REVERB_SIZE = 0.65f;

// Controls
static std::atomic<int>   g_waveform{0};
static std::atomic<int>   g_lfo_waveform{0};  // LfoWave index
static std::atomic<float> g_lfo_out{0.0f};   // last LFO output for LED driver
static std::atomic<bool>  g_gate{false};
static std::atomic<bool>  g_shift{false};
static std::atomic<int>   g_pitch_env{0};     // –1 fall, 0 off, +1 rise

// Super drip reverb (default on; secret: hold Shift + triple-tap fall to toggle)
static std::atomic<bool>  g_super_drip{true};
// LFO-pitch-envelope link (default on; secret: triple-tap pitch switch to fall to toggle)
static std::atomic<bool>  g_lfo_pitch_link{true};
static std::atomic<float> g_delay_time_eff{0.375f}; // effective delay time
static std::atomic<float> g_lfo_rate_eff{0.35f};    // effective LFO rate (freq-scaled)

// Reverb/delay type selection (configurable via AP mode web UI)
static std::atomic<int>   g_reverb_type{0};     // 0=Spring, 1=Plate, 2=Hall, 3=Schroeder
static std::atomic<int>   g_delay_type{0};      // 0=Tape, 1=Digital
static std::atomic<float> g_tape_wobble{1.0f};  // tape delay wobble amount (0–1)
static std::atomic<float> g_tape_flutter{1.0f}; // tape delay flutter amount (0–1)

// Effects chain order (configurable via AP mode web UI)
// 0=Filt→Dly→Rev  1=Filt→Rev→Dly  2=Dly→Filt→Rev
// 3=Dly→Rev→Filt  4=Rev→Filt→Dly  5=Rev→Dly→Filt
static std::atomic<int>   g_fx_chain{0};

// Modulation effects wet/dry mix (0 = off, >0 = enabled at that mix level)
static std::atomic<float> g_phaser_mix{0.0f};   // 0–1
static std::atomic<float> g_chorus_mix{0.0f};   // 0–1
static std::atomic<float> g_flanger_mix{0.0f};  // 0–1

// Tape saturator
static std::atomic<float> g_saturator_mix{0.0f};   // 0–1 wet/dry (0=off)
static std::atomic<float> g_saturator_drive{0.5f};  // 0–1 drive amount

// AP mode flag
static std::atomic<bool>  g_ap_mode{false};

// LED settings (persisted to config)
static std::atomic<float> g_led_brightness{1.0f};  // 0–1 master brightness
static std::atomic<bool>  g_led_night_mode{false};  // caps brightness for dark rooms

// LED driver instance (global so callbacks can access it)
static LedDriver g_led;

// Theme (server-side, persisted to config)
static std::atomic<int>   g_theme{0};  // index into theme list
static constexpr std::array<const char*, 16> THEME_IDS = {
    // Dark
    "midnight", "deep-dub", "reggae", "steppers", "roots", "irie",
    "kingston", "dubplate", "soundsystem", "heavyweight",
    // Light
    "silver", "concrete", "driftwood", "overcast", "bleached", "studio"
};
static constexpr int NUM_THEMES = THEME_IDS.size();

// ─── Encoder parameter mapping ──────────────────────────────────────
//
// Table-driven encoder → parameter assignment.  Each encoder (0–4) in
// each bank (A/B) maps to a parameter name string.  Defaults match the
// original hardcoded layout.  Configurable via web UI.

struct EncoderParam {
    const char* name;    // parameter key name
    const char* label;   // display label
    float min_val, max_val, default_val;
    bool  is_log;        // logarithmic (multiplicative) vs linear (additive)
    float step;          // base step size
};

static const EncoderParam PARAM_TABLE[] = {
    // Original 10 parameters (indices 0–9)
    {"freq",           "Frequency",        30.0f,   8000.0f,  440.0f,  true,  1.0594630943592953f},
    {"lfo_rate",       "LFO Rate",         0.1f,    20.0f,    0.35f,   true,  1.12f},
    {"filter_cutoff",  "Filter Cutoff",    20.0f,   20000.0f, 8000.0f, true,  1.122462048309373f},
    {"delay_time",     "Delay Time",       0.001f,  1.0f,     0.375f,  true,  1.10f},
    {"delay_feedback", "Delay Feedback",   0.0f,    0.95f,    0.55f,   false, 0.04f},
    {"lfo_depth",      "LFO Depth",        0.0f,    1.0f,     0.35f,   false, 0.04f},
    {"release_time",   "Release Time",     0.01f,   5.0f,     0.05f,   true,  1.15f},
    {"filter_reso",    "Filter Resonance", 0.0f,    0.95f,    0.0f,    false, 0.03f},
    {"delay_mix",      "Delay Mix",        0.0f,    1.0f,     0.30f,   false, 0.05f},
    {"reverb_mix",     "Reverb Mix",       0.0f,    1.0f,     0.35f,   false, 0.05f},
    // Modulation effects (indices 10–12)
    {"phaser_mix",     "Phaser Wet/Dry",   0.0f,    1.0f,     0.0f,    false, 0.05f},
    {"chorus_mix",     "Chorus Wet/Dry",   0.0f,    1.0f,     0.0f,    false, 0.05f},
    {"flanger_mix",    "Flanger Wet/Dry",  0.0f,    1.0f,     0.0f,    false, 0.05f},
    // Tape saturator (indices 13–14)
    {"saturator_mix",  "Saturator Wet/Dry",0.0f,    1.0f,     0.0f,    false, 0.05f},
    {"saturator_drive","Saturator Drive",  0.0f,    1.0f,     0.5f,    false, 0.04f},
    // Tape delay parameters (indices 15–16)
    {"tape_wobble",    "Tape Wobble",      0.0f,    1.0f,     1.0f,    false, 0.05f},
    {"tape_flutter",   "Tape Flutter",     0.0f,    1.0f,     1.0f,    false, 0.05f},
};
static constexpr int NUM_PARAMS = sizeof(PARAM_TABLE) / sizeof(PARAM_TABLE[0]);

// Default encoder assignments: bank_a[0..4], bank_b[0..4]
// Index into PARAM_TABLE
static int g_encoder_map_a[5] = {0, 1, 2, 3, 4};  // freq, lfo_rate, cutoff, delay_time, delay_fb
static int g_encoder_map_b[5] = {5, 6, 7, 8, 9};  // lfo_depth, release, reso, delay_mix, reverb_mix
static std::mutex g_encoder_map_mutex;

// Per-parameter sensitivity multipliers (scales acceleration curve)
// Tuned defaults: lower values for fine control (pitch, feedback), higher for broad sweeps
static float g_param_sensitivity[17] = {
    1.00f, 0.55f, 0.51f, 1.00f, 0.25f,   // freq, lfo_rate, cutoff, delay_time, delay_fb
    0.78f, 1.00f, 1.00f, 0.44f, 0.44f,   // lfo_depth, release, reso, delay_mix, reverb_mix
    1.0f, 1.0f, 1.0f, 1.0f, 1.0f,        // phaser, chorus, flanger, sat_mix, sat_drive
    1.0f, 1.0f                             // tape_wobble, tape_flutter
};
static std::mutex g_sensitivity_mutex;

static std::atomic<float>* param_atomic_for(const char* name) {
    static const std::map<std::string, std::atomic<float>*> param_map = {
        {"freq", &g_freq}, {"lfo_rate", &g_lfo_rate},
        {"filter_cutoff", &g_filter_cutoff}, {"delay_time", &g_delay_time},
        {"delay_feedback", &g_delay_feedback}, {"lfo_depth", &g_lfo_depth},
        {"release_time", &g_release_time}, {"filter_reso", &g_filter_reso},
        {"delay_mix", &g_delay_mix}, {"reverb_mix", &g_reverb_mix},
        {"phaser_mix", &g_phaser_mix}, {"chorus_mix", &g_chorus_mix},
        {"flanger_mix", &g_flanger_mix}, {"saturator_mix", &g_saturator_mix},
        {"saturator_drive", &g_saturator_drive},
        {"tape_wobble", &g_tape_wobble}, {"tape_flutter", &g_tape_flutter},
    };
    auto it = param_map.find(name);
    return (it != param_map.end()) ? it->second : nullptr;
}

static int find_param_index(const char* name) {
    for (int i = 0; i < NUM_PARAMS; i++)
        if (strcmp(PARAM_TABLE[i].name, name) == 0) return i;
    return -1;
}

// ─── Per-parameter step sizes (base, before acceleration) ───────────
//
// Multiplicative params: acceleration raises base step to a power
//   e.g. pow(SEMITONE, accel) → 1–4 semitones per click
// Additive params:       acceleration multiplies the base step
//   e.g. 0.04 * accel → 4–16 % per click

// Bank A — multiplicative
static constexpr float FREQ_STEP       = 1.0594630943592953f;  // 1 semitone = 2^(1/12)
static constexpr float LFO_RATE_STEP   = 1.12f;                // 12 % per click
static constexpr float CUTOFF_STEP     = 1.122462048309373f;   // 2 semitones = 2^(1/6)
static constexpr float DELAY_TIME_STEP = 1.10f;                // 10 % per click

// Bank A — additive
static constexpr float DELAY_FB_STEP   = 0.04f;                // ~24 steps across 0–95%

// Bank B — additive
static constexpr float LFO_DEPTH_STEP   = 0.04f;               // 4 % per click
static constexpr float RELEASE_TIME_STEP = 1.15f;              // 15 % per click (log)
static constexpr float FILTER_RESO_STEP = 0.03f;               // 3 % (fine near self-osc)
static constexpr float DELAY_MIX_STEP  = 0.05f;                // 5 % per click
static constexpr float REVERB_MIX_STEP = 0.05f;                // 5 % per click

static constexpr float REF_FREQ         = 440.0f;              // neutral scaling point

static void update_link_eff();           // forward declaration
static const char* waveform_name(int w); // forward declaration
static const char* lfo_wave_name(int w); // forward declaration

// ─── Dub Siren Presets ──────────────────────────────────────────────
//
// GPIO 5 (Bank A) cycles through these presets.  Inspired by the
// Benidub DS71 — analog square-wave oscillator through 12dB/oct
// low-pass filter, aggressive LFO-driven siren sounds.  Fat and
// punchy with deterministic LFO shapes only.  All knobs remain
// live after selecting a preset so the performer can tweak from any
// starting point.

struct DubPreset {
    const char* name;
    int         waveform;       // Waveform enum index
    int         lfo_wave;       // LfoWave enum index
    float       freq;           // base frequency (Hz)
    float       lfo_rate;       // LFO rate (Hz)
    float       lfo_depth;      // LFO depth (0–1)
    float       filter_cutoff;  // filter cutoff (Hz)
    float       filter_reso;    // filter resonance (0–0.95)
    float       delay_time;     // delay time (seconds)
    float       delay_feedback; // delay feedback (0–0.95)
    float       delay_mix;      // delay wet/dry (0–1)
    float       reverb_mix;     // reverb wet/dry (0–1)
    float       release_time;   // envelope release (seconds)
    float       sweep_dir;      // filter sweep on release: -1=Down, 0=Flat, +1=Up
    int         pitch_env;      // pitch envelope: -1=Fall, 0=Off, +1=Rise
    int         reverb_type;    // 0=Spring, 1=Plate, 2=Hall, 3=Schroeder
    int         delay_type;     // 0=Tape, 1=Digital
    float       tape_wobble;    // tape wobble amount (0–1)
    float       tape_flutter;   // tape flutter amount (0–1)
    const char* category;       // preset category for web UI grouping
    // Extended fields — modulation & saturator
    float       phaser_mix;     // 0–1 (0=off)
    float       chorus_mix;     // 0–1 (0=off)
    float       flanger_mix;    // 0–1 (0=off)
    float       saturator_mix;  // 0–1 (0=off)
    float       saturator_drive;// 0–1 (-1 = use default 0.5)
    int         fx_chain;       // -1 = leave unchanged, 0–5 = set chain order
};

static constexpr int NUM_PRESETS = 4;

static const DubPreset PRESETS[NUM_PRESETS] = {
    {
        "Slow Wail", 2, 0, 550.0f, 0.1f, 1.00f, 2500.0f, 0.35f,
        0.400f, 0.60f, 0.40f, 0.45f, 2.000f, -1.0f, -1,
        0, 0, 1.0f, 1.0f, "Standard"
    },
    {
        "Machine Gun", 1, 2, 1000.0f, 14.0f, 0.85f, 4000.0f, 0.30f,
        0.180f, 0.60f, 0.40f, 0.30f, 0.500f, -1.0f, 0,
        0, 0, 1.0f, 1.0f, "Standard"
    },
    {
        "Lickshot", 1, 1, 800.0f, 8.0f, 0.80f, 3500.0f, 0.40f,
        0.300f, 0.55f, 0.40f, 0.35f, 0.350f, -1.0f, -1,
        0, 0, 1.0f, 1.0f, "Standard"
    },
    {
        "Droppa", 1, 4, 1000.0f, 4.0f, 0.75f, 3000.0f, 0.45f,
        0.375f, 0.65f, 0.45f, 0.40f, 0.800f, -1.0f, -1,
        0, 0, 1.0f, 1.0f, "Standard"
    },
};

// ─── Experimental Presets ────────────────────────────────────────────
//
// Wild, extreme parameter combinations that push the synth to its
// limits.  Accessible via triple-tap of the Shift (bank) button.

static constexpr int NUM_EXPERIMENTAL = 4;

static const DubPreset EXPERIMENTAL[NUM_EXPERIMENTAL] = {
    {
        "Insect Swarm", 1, 5, 2000.0f, 18.0f, 0.95f, 6000.0f, 0.85f,
        0.050f, 0.90f, 0.70f, 0.60f, 0.150f, +1.0f, +1,
        0, 0, 1.0f, 1.0f, "Experimental"
    },
    {
        "Depth Charge", 0, 7, 120.0f, 0.5f, 1.00f, 800.0f, 0.70f,
        0.800f, 0.92f, 0.80f, 0.85f, 3.000f, -1.0f, -1,
        0, 0, 1.0f, 1.0f, "Experimental"
    },
    {
        "Glitch Storm", 3, 6, 600.0f, 15.0f, 0.90f, 12000.0f, 0.90f,
        0.120f, 0.85f, 0.65f, 0.20f, 0.100f, +1.0f, 0,
        0, 0, 1.0f, 1.0f, "Experimental"
    },
    {
        "Foghorn From Hell", 2, 3, 55.0f, 0.3f, 0.85f, 1200.0f, 0.80f,
        0.600f, 0.88f, 0.75f, 0.90f, 4.000f, -1.0f, -1,
        0, 0, 1.0f, 1.0f, "Experimental"
    },
};

// ─── Full Preset Library ──────────────────────────────────────────────
//
// Comprehensive library of dub siren presets across multiple categories.
// Accessible via the AP mode web UI for browsing and loading into
// user preset slots.

static constexpr int NUM_LIBRARY_PRESETS = 48;

static const DubPreset PRESET_LIBRARY[NUM_LIBRARY_PRESETS] = {
    // ── Classic Dub (8) ─────────────────────────────────────────────
    {
        "King Tubby Aquarium", 2, 0, 440.0f, 0.15f, 0.90f, 2000.0f, 0.40f,
        0.450f, 0.70f, 0.50f, 0.55f, 2.500f, -1.0f, -1,
        0, 0, 1.0f, 1.0f, "Classic Dub"
    },
    {
        "Channel One Riddim", 1, 1, 660.0f, 2.0f, 0.65f, 3000.0f, 0.35f,
        0.375f, 0.55f, 0.35f, 0.40f, 1.200f, -1.0f, 0,
        0, 0, 0.8f, 0.7f, "Classic Dub"
    },
    {
        "Black Ark Madness", 2, 5, 500.0f, 6.0f, 0.80f, 2800.0f, 0.55f,
        0.330f, 0.75f, 0.55f, 0.65f, 1.800f, -1.0f, -1,
        0, 0, 1.0f, 1.0f, "Classic Dub"
    },
    {
        "Scientist Dub", 1, 0, 380.0f, 0.3f, 0.70f, 1800.0f, 0.50f,
        0.500f, 0.65f, 0.45f, 0.50f, 2.000f, -1.0f, -1,
        0, 0, 0.9f, 0.8f, "Classic Dub"
    },
    {
        "Far East Melodica", 3, 0, 520.0f, 0.2f, 0.50f, 4000.0f, 0.20f,
        0.400f, 0.50f, 0.30f, 0.45f, 1.500f, -1.0f, 0,
        0, 0, 0.5f, 0.4f, "Classic Dub"
    },
    {
        "Taxi Connection", 1, 2, 880.0f, 10.0f, 0.75f, 3500.0f, 0.45f,
        0.250f, 0.55f, 0.40f, 0.35f, 0.600f, -1.0f, 0,
        0, 0, 1.0f, 1.0f, "Classic Dub"
    },
    {
        "Digital Steppa", 1, 4, 750.0f, 5.0f, 0.70f, 2500.0f, 0.40f,
        0.300f, 0.60f, 0.40f, 0.30f, 0.800f, -1.0f, -1,
        0, 1, 0.0f, 0.0f, "Classic Dub"
    },
    {
        "Roots Radics", 2, 3, 350.0f, 1.5f, 0.60f, 2200.0f, 0.45f,
        0.375f, 0.70f, 0.45f, 0.50f, 1.500f, -1.0f, -1,
        0, 0, 0.7f, 0.6f, "Classic Dub"
    },

    // ── NJD Style (4) ───────────────────────────────────────────────
    {
        "NJD Classic Wail", 1, 0, 600.0f, 0.08f, 1.00f, 3000.0f, 0.30f,
        0.400f, 0.55f, 0.35f, 0.40f, 2.500f, -1.0f, -1,
        0, 0, 1.0f, 1.0f, "NJD Style"
    },
    {
        "NJD Stutter", 1, 2, 900.0f, 16.0f, 0.90f, 4500.0f, 0.35f,
        0.150f, 0.65f, 0.45f, 0.30f, 0.400f, -1.0f, 0,
        0, 0, 1.0f, 1.0f, "NJD Style"
    },
    {
        "NJD Laser", 1, 1, 1200.0f, 12.0f, 0.85f, 5000.0f, 0.50f,
        0.200f, 0.50f, 0.35f, 0.25f, 0.250f, -1.0f, -1,
        0, 0, 1.0f, 1.0f, "NJD Style"
    },
    {
        "NJD Sub Drop", 0, 7, 200.0f, 0.5f, 1.00f, 1500.0f, 0.60f,
        0.500f, 0.70f, 0.50f, 0.55f, 3.000f, -1.0f, -1,
        0, 0, 0.8f, 0.6f, "NJD Style"
    },

    // ── Sci-Fi (4) ──────────────────────────────────────────────────
    {
        "Dalek Voice", 1, 2, 150.0f, 20.0f, 0.40f, 2000.0f, 0.70f,
        0.020f, 0.60f, 0.50f, 0.30f, 0.500f, 0.0f, 0,
        3, 1, 0.0f, 0.0f, "Sci-Fi"
    },
    {
        "Tardis Sweep", 2, 0, 300.0f, 0.05f, 1.00f, 6000.0f, 0.25f,
        0.600f, 0.80f, 0.60f, 0.70f, 5.000f, +1.0f, +1,
        2, 0, 1.0f, 1.0f, "Sci-Fi"
    },
    {
        "Cyberman March", 1, 2, 400.0f, 8.0f, 0.50f, 3000.0f, 0.80f,
        0.250f, 0.70f, 0.40f, 0.20f, 0.300f, -1.0f, 0,
        3, 1, 0.0f, 0.0f, "Sci-Fi"
    },
    {
        "Space Echo", 2, 0, 700.0f, 0.3f, 0.60f, 5000.0f, 0.15f,
        0.500f, 0.85f, 0.70f, 0.60f, 2.000f, 0.0f, 0,
        1, 0, 0.6f, 0.5f, "Sci-Fi"
    },

    // ── Modern Dub (4) ──────────────────────────────────────────────
    {
        "Iration Steppa", 1, 4, 800.0f, 3.0f, 0.80f, 2500.0f, 0.50f,
        0.350f, 0.65f, 0.45f, 0.45f, 1.200f, -1.0f, -1,
        0, 0, 1.0f, 1.0f, "Modern Dub"
    },
    {
        "Jah Shaka Quake", 0, 3, 80.0f, 0.4f, 1.00f, 600.0f, 0.75f,
        0.700f, 0.90f, 0.75f, 0.80f, 4.000f, -1.0f, -1,
        0, 0, 1.0f, 1.0f, "Modern Dub"
    },
    {
        "Aba Shanti Power", 1, 1, 950.0f, 6.0f, 0.85f, 3500.0f, 0.55f,
        0.280f, 0.60f, 0.40f, 0.40f, 0.700f, -1.0f, -1,
        0, 0, 0.9f, 0.8f, "Modern Dub"
    },
    {
        "Channel One Sound", 2, 0, 440.0f, 0.2f, 0.75f, 2800.0f, 0.40f,
        0.375f, 0.55f, 0.35f, 0.50f, 1.800f, -1.0f, -1,
        0, 0, 0.7f, 0.6f, "Modern Dub"
    },

    // ── Experimental (4) ────────────────────────────────────────────
    {
        "Granular Storm", 3, 5, 1500.0f, 20.0f, 1.00f, 8000.0f, 0.90f,
        0.030f, 0.92f, 0.80f, 0.50f, 0.080f, +1.0f, +1,
        1, 1, 0.0f, 0.0f, "Experimental"
    },
    {
        "Ring Mod Chaos", 1, 6, 3000.0f, 19.0f, 0.95f, 10000.0f, 0.85f,
        0.080f, 0.88f, 0.65f, 0.40f, 0.120f, +1.0f, 0,
        3, 0, 1.0f, 1.0f, "Experimental"
    },
    {
        "Bit Crusher", 1, 5, 1000.0f, 15.0f, 0.80f, 4000.0f, 0.70f,
        0.100f, 0.80f, 0.55f, 0.15f, 0.200f, 0.0f, 0,
        3, 1, 0.0f, 0.0f, "Experimental"
    },
    {
        "FM Madness", 0, 6, 250.0f, 18.0f, 1.00f, 15000.0f, 0.50f,
        0.060f, 0.85f, 0.70f, 0.55f, 0.300f, +1.0f, +1,
        2, 0, 1.0f, 1.0f, "Experimental"
    },

    // ── Utility (4) ─────────────────────────────────────────────────
    {
        "Pure Tone", 0, 0, 440.0f, 1.0f, 0.00f, 20000.0f, 0.00f,
        0.375f, 0.00f, 0.00f, 0.00f, 0.300f, 0.0f, 0,
        0, 1, 0.0f, 0.0f, "Utility"
    },
    {
        "Test Sweep", 2, 0, 200.0f, 0.05f, 1.00f, 10000.0f, 0.00f,
        0.375f, 0.00f, 0.00f, 0.00f, 5.000f, 0.0f, 0,
        0, 1, 0.0f, 0.0f, "Utility"
    },
    {
        "Click Track", 1, 2, 1000.0f, 4.0f, 1.00f, 20000.0f, 0.00f,
        0.250f, 0.00f, 0.00f, 0.00f, 0.010f, 0.0f, 0,
        0, 1, 0.0f, 0.0f, "Utility"
    },
    {
        "Sub Bass", 0, 0, 60.0f, 0.5f, 0.30f, 200.0f, 0.60f,
        0.500f, 0.40f, 0.25f, 0.20f, 2.000f, -1.0f, -1,
        0, 0, 0.5f, 0.3f, "Utility"
    },

    // ── Sirens & Alerts (8) ──────────────────────────────────────────
    {
        "Police Wail (US)", 1, 0, 700.0f, 0.5f, 0.95f, 5000.0f, 0.20f,
        0.300f, 0.30f, 0.15f, 0.10f, 1.500f, 0.0f, 0,
        0, 1, 0.0f, 0.0f, "Sirens & Alerts"
    },
    {
        "Police Yelp", 1, 2, 900.0f, 6.0f, 0.90f, 6000.0f, 0.15f,
        0.200f, 0.20f, 0.10f, 0.08f, 0.400f, 0.0f, 0,
        0, 1, 0.0f, 0.0f, "Sirens & Alerts"
    },
    {
        "European Two-Tone", 1, 2, 600.0f, 1.5f, 0.70f, 4000.0f, 0.15f,
        0.250f, 0.20f, 0.10f, 0.05f, 0.800f, 0.0f, 0,
        0, 1, 0.0f, 0.0f, "Sirens & Alerts"
    },
    {
        "Tornado Siren", 2, 3, 400.0f, 0.08f, 1.00f, 3000.0f, 0.35f,
        0.500f, 0.40f, 0.20f, 0.30f, 5.000f, 0.0f, 0,
        0, 0, 0.8f, 0.6f, "Sirens & Alerts"
    },
    {
        "Air Raid", 2, 0, 500.0f, 0.25f, 1.00f, 3500.0f, 0.40f,
        0.400f, 0.50f, 0.25f, 0.35f, 4.000f, 0.0f, 0,
        0, 0, 1.0f, 0.8f, "Sirens & Alerts"
    },
    {
        "Ambulance", 1, 0, 800.0f, 1.0f, 0.80f, 4500.0f, 0.20f,
        0.250f, 0.25f, 0.12f, 0.08f, 0.600f, 0.0f, 0,
        0, 1, 0.0f, 0.0f, "Sirens & Alerts"
    },
    {
        "Ship Horn", 2, 0, 85.0f, 0.15f, 0.40f, 800.0f, 0.65f,
        0.600f, 0.55f, 0.40f, 0.70f, 3.500f, -1.0f, -1,
        0, 0, 1.0f, 1.0f, "Sirens & Alerts"
    },
    {
        "Nuclear Alert", 1, 2, 1200.0f, 2.5f, 1.00f, 8000.0f, 0.30f,
        0.180f, 0.60f, 0.35f, 0.20f, 0.500f, +1.0f, 0,
        3, 1, 0.0f, 0.0f, "Sirens & Alerts"
    },

    // ── Sound FX (4) ─────────────────────────────────────────────────
    {
        "Sonar Ping", 0, 0, 2400.0f, 0.8f, 0.00f, 3000.0f, 0.50f,
        0.800f, 0.85f, 0.70f, 0.75f, 0.150f, 0.0f, -1,
        2, 0, 0.5f, 0.3f, "Sound FX"
    },
    {
        "Laser Zap", 2, 7, 3500.0f, 12.0f, 0.95f, 12000.0f, 0.60f,
        0.100f, 0.70f, 0.40f, 0.25f, 0.080f, 0.0f, -1,
        1, 1, 0.0f, 0.0f, "Sound FX"
    },
    {
        "UFO Landing", 0, 0, 300.0f, 0.12f, 1.00f, 5000.0f, 0.70f,
        0.450f, 0.80f, 0.55f, 0.65f, 3.000f, 0.0f, +1,
        2, 0, 1.0f, 1.0f, "Sound FX"
    },
    {
        "Geiger Counter", 1, 5, 1800.0f, 8.0f, 1.00f, 15000.0f, 0.10f,
        0.050f, 0.70f, 0.30f, 0.10f, 0.010f, 0.0f, 0,
        3, 1, 0.0f, 0.0f, "Sound FX"
    },

    // ── Dub FX (8) — modulation/saturator showcase ─────────────────
    {
        "Phase Dub", 2, 0, 440.0f, 0.15f, 0.80f, 2500.0f, 0.35f,
        0.400f, 0.65f, 0.45f, 0.50f, 2.000f, -1.0f, -1,
        0, 0, 1.0f, 1.0f, "Dub FX",
        0.65f, 0.0f, 0.0f, 0.0f, 0.5f, 0
    },
    {
        "Chorus Siren", 1, 1, 700.0f, 3.0f, 0.70f, 3500.0f, 0.40f,
        0.350f, 0.55f, 0.35f, 0.40f, 1.200f, -1.0f, -1,
        0, 0, 0.8f, 0.7f, "Dub FX",
        0.0f, 0.70f, 0.0f, 0.0f, 0.5f, 0
    },
    {
        "Flange Madness", 1, 4, 900.0f, 5.0f, 0.85f, 4000.0f, 0.50f,
        0.250f, 0.70f, 0.50f, 0.35f, 0.800f, -1.0f, -1,
        0, 0, 1.0f, 1.0f, "Dub FX",
        0.0f, 0.0f, 0.75f, 0.0f, 0.5f, 0
    },
    {
        "Tape Warmth", 2, 0, 350.0f, 0.2f, 0.60f, 2000.0f, 0.45f,
        0.450f, 0.70f, 0.50f, 0.55f, 2.500f, -1.0f, -1,
        0, 0, 1.0f, 1.0f, "Dub FX",
        0.0f, 0.0f, 0.0f, 0.75f, 0.70f, 0
    },
    {
        "Full FX Stack", 1, 0, 550.0f, 0.12f, 0.90f, 3000.0f, 0.30f,
        0.400f, 0.60f, 0.40f, 0.50f, 2.000f, -1.0f, -1,
        0, 0, 1.0f, 1.0f, "Dub FX",
        0.40f, 0.35f, 0.0f, 0.50f, 0.55f, 0
    },
    {
        "Phaser Steppa", 1, 4, 800.0f, 4.0f, 0.75f, 2800.0f, 0.50f,
        0.300f, 0.65f, 0.45f, 0.40f, 1.000f, -1.0f, -1,
        0, 0, 0.9f, 0.8f, "Dub FX",
        0.80f, 0.0f, 0.0f, 0.30f, 0.45f, 2
    },
    {
        "Saturated Echo", 2, 3, 500.0f, 1.5f, 0.65f, 2200.0f, 0.55f,
        0.500f, 0.80f, 0.60f, 0.45f, 1.800f, -1.0f, -1,
        0, 0, 0.7f, 0.6f, "Dub FX",
        0.0f, 0.0f, 0.0f, 0.85f, 0.80f, 1
    },
    {
        "Chorus Flange Dub", 0, 0, 300.0f, 0.08f, 1.00f, 1800.0f, 0.60f,
        0.600f, 0.75f, 0.55f, 0.65f, 3.000f, -1.0f, -1,
        0, 0, 1.0f, 1.0f, "Dub FX",
        0.0f, 0.55f, 0.50f, 0.0f, 0.5f, 0
    },
};

static std::atomic<int> g_preset{0};    // current preset index (0–3)

static void apply_dub_preset(const DubPreset& p)
{
    g_waveform.store(p.waveform);
    g_lfo_waveform.store(p.lfo_wave);
    g_freq.store(p.freq);
    g_lfo_rate.store(p.lfo_rate);
    g_lfo_depth.store(p.lfo_depth);
    g_filter_cutoff.store(p.filter_cutoff);
    g_filter_reso.store(p.filter_reso);
    g_delay_time.store(p.delay_time);
    g_delay_feedback.store(p.delay_feedback);
    g_delay_mix.store(p.delay_mix);
    g_reverb_mix.store(p.reverb_mix);
    g_release_time.store(p.release_time);
    g_sweep_dir.store(p.sweep_dir);
    g_pitch_env.store(p.pitch_env);
    g_reverb_type.store(p.reverb_type);
    g_delay_type.store(p.delay_type);
    g_tape_wobble.store(p.tape_wobble);
    g_tape_flutter.store(p.tape_flutter);
    // Extended FX fields
    g_phaser_mix.store(p.phaser_mix);
    g_chorus_mix.store(p.chorus_mix);
    g_flanger_mix.store(p.flanger_mix);
    g_saturator_mix.store(p.saturator_mix);
    g_saturator_drive.store(p.saturator_drive >= 0.0f ? p.saturator_drive : 0.5f);
    if (p.fx_chain >= 0) g_fx_chain.store(p.fx_chain);

    update_link_eff();
}

// apply_preset and apply_experimental are defined after UserPreset/bank arrays
static void apply_preset(int idx);
static void apply_experimental(int idx);

// ─── User preset bank ────────────────────────────────────────────────

static constexpr int NUM_USER_PRESETS = 4;
static constexpr char PRESET_V3_HEADER[] = "DUBSIREN_PRESETS_V3";
static constexpr char PRESET_V2_HEADER[] = "DUBSIREN_PRESETS_V2";
static constexpr char PRESET_V1_HEADER[] = "DUBSIREN_PRESETS_V1";

struct UserPreset {
    bool  saved;             // true = has user data, false = factory copy
    char  name[32];          // user-assigned name
    int   waveform;
    int   lfo_wave;
    float freq;
    float lfo_rate;
    float lfo_depth;
    float filter_cutoff;
    float filter_reso;
    float delay_time;
    float delay_feedback;
    float delay_mix;
    float reverb_mix;
    float release_time;
    float sweep_dir;
    int   pitch_env;         // -1=Fall, 0=Off, +1=Rise
    bool  super_drip;
    int   reverb_type;       // 0=Spring, 1=Plate, 2=Hall, 3=Schroeder
    int   delay_type;        // 0=Tape, 1=Digital
    float tape_wobble;       // tape wobble amount (0–1)
    float tape_flutter;      // tape flutter amount (0–1)
};

static UserPreset g_user_presets[NUM_USER_PRESETS];
static UserPreset g_standard_presets[NUM_PRESETS];
static UserPreset g_experimental_presets[NUM_EXPERIMENTAL];

// Preview state — snapshot before preview, restore on stop
static UserPreset g_preview_snapshot;
static std::atomic<bool> g_previewing{false};

// Bank mode: USER (boot default), STANDARD (factory), EXPERIMENTAL
enum class BankMode { USER, STANDARD, EXPERIMENTAL };
static std::atomic<int> g_bank_mode{static_cast<int>(BankMode::USER)};

// Pending double-click state for bank toggle on Shift button
static std::atomic<bool> g_shift_dblclick_pending{false};
static std::chrono::steady_clock::time_point g_shift_dblclick_time{};
static int g_shift_clicks = 0;
static std::chrono::steady_clock::time_point g_shift_first_click{};

// ─── Overlay / mount-point helpers ───────────────────────────────────

// True when the root filesystem is overlayFS (kiosk / read-only mode).
// Writes to the overlay go to a RAM-backed upper layer and vanish on
// power loss, so we must detect this and route presets elsewhere.
static bool is_overlay_root()
{
    FILE* f = fopen("/proc/mounts", "r");
    if (!f) return false;
    char line[512];
    while (fgets(line, sizeof(line), f)) {
        // /proc/mounts format: device mount_point fs_type options ...
        // Parse fields explicitly to avoid false positives from
        // "overlay" appearing in mount options of a non-overlay root.
        char device[256], mount_point[256], fs_type[256];
        if (sscanf(line, "%255s %255s %255s", device, mount_point, fs_type) == 3) {
            if (strcmp(mount_point, "/") == 0 && strcmp(fs_type, "overlay") == 0) {
                fclose(f);
                return true;
            }
        }
    }
    fclose(f);
    return false;
}

// True when `path` is a mount point (device differs from its parent).
// Used to detect an active bind-mount at <install_dir>/data.
static bool is_mount_point(const char* dir_path)
{
    struct stat st, parent_st;
    if (stat(dir_path, &st) != 0) return false;

    char parent[PATH_MAX];
    snprintf(parent, sizeof(parent), "%s/..", dir_path);
    if (stat(parent, &parent_st) != 0) return false;

    return st.st_dev != parent_st.st_dev;
}

// Flag set when preset storage is known to be volatile (overlay RAM).
static bool g_volatile_presets = false;

// ─── Preset file path ────────────────────────────────────────────────

static const char* preset_file_path()
{
    static char path[PATH_MAX] = {};
    if (path[0]) return path;

    const bool overlay = is_overlay_root();
    if (overlay)
        printf("  Overlay FS detected — looking for persistent storage\n");

    // Resolve install dir from executable path:
    //   /home/<user>/dubsiren/build/dubsiren  →  /home/<user>/dubsiren
    // base buffer leaves room for appending suffixes when no persist mount.
    // persist buffer leaves room for "/mnt/persist" + base, and the result
    // must still fit suffixes up to 30 chars in path[PATH_MAX].
    static constexpr size_t BASE_ROOM = 48;           // room for suffixes
    static constexpr size_t PERSIST_ROOM = 31;        // 30-char suffix + NUL
    char base[PATH_MAX - BASE_ROOM];
    ssize_t len = readlink("/proc/self/exe", base, sizeof(base) - 1);
    if (len > 0 && len < (ssize_t)sizeof(base) - 1) {
        base[len] = '\0';
        // Strip "/build/dubsiren" (go up two path components)
        char* slash = strrchr(base, '/');         // strip binary name
        if (slash) *slash = '\0';
        slash = strrchr(base, '/');               // strip "build"
        if (slash) *slash = '\0';

        // When overlayFS is active, the bind-mount at <base>/data may not
        // be available (race, mount failure, running outside systemd).
        // Writes to the overlay look fine but vanish on power loss.
        // Bypass the overlay entirely by writing straight to the persist
        // mount if it exists — this is the real read-write disk.
        bool use_persist = false;
        char persist[PATH_MAX - PERSIST_ROOM];
        if (strlen(base) + sizeof("/mnt/persist") < sizeof(persist)) {
            snprintf(persist, sizeof(persist), "/mnt/persist%s", base);

            // Ensure the directory tree exists on the persist mount
            // BEFORE checking stat().  Without this, a mounted /mnt/persist
            // that lacks the install-dir subtree would cause stat() to fail
            // and presets would silently fall through to volatile overlay RAM.
            if (overlay) {
                char pd[PATH_MAX];
                snprintf(pd, sizeof(pd), "%s/data", persist);
                if (mkdir(pd, 0755) != 0 && errno != EEXIST)
                    fprintf(stderr, "  !!! Failed to create directory %s: %s\n", pd, strerror(errno));
                snprintf(pd, sizeof(pd), "%s/data/presets", persist);
                if (mkdir(pd, 0755) != 0 && errno != EEXIST)
                    fprintf(stderr, "  !!! Failed to create directory %s: %s\n", pd, strerror(errno));
            }

            struct stat st;
            use_persist = (stat(persist, &st) == 0 && S_ISDIR(st.st_mode));

            // On overlay FS the persist mount is critical.  If it isn't
            // visible yet (boot race / slow mount) retry a few times.
            if (!use_persist && overlay) {
                for (int attempt = 1; attempt <= 3 && !use_persist; attempt++) {
                    fprintf(stderr,
                        "  !!! Persist mount not ready — retry %d/3 ...\n",
                        attempt);
                    sleep(1);
                    use_persist = (stat(persist, &st) == 0
                                   && S_ISDIR(st.st_mode));
                }
                if (!use_persist) {
                    fprintf(stderr,
                        "  !!! /mnt/persist not available — ensure-persist.sh may not have run.\n"
                        "  !!! To set up manually:  sudo /usr/local/lib/dubsiren/ensure-persist.sh %s\n",
                        base);
                }
            }
        }

        // Determine the storage root.
        // Priority: 1) bind-mount at base/data  2) /mnt/persist  3) base
        // The bind-mount is preferred because ensure-persist.sh sets it up
        // with correct ownership.  The persist mount is a fallback that
        // writes directly to the real disk.
        const char* root = nullptr;
        const char* storage_label = nullptr;

        if (overlay) {
            // Check the systemd bind-mount at <base>/data first — this
            // is what ensure-persist.sh sets up with correct permissions.
            char data_dir[PATH_MAX];
            snprintf(data_dir, sizeof(data_dir), "%s/data", base);
            if (is_mount_point(data_dir)) {
                root = base;
                storage_label = "bind-mount — durable";
            } else if (use_persist) {
                // Bind mount unavailable, but persist mount exists.
                // Fall back to writing directly to the persist mount.
                root = persist;
                storage_label = "persist mount — durable";
            } else {
                // No persistent storage available.  Writes will go to
                // the overlay RAM layer and be LOST on power cycle.
                root = base;
                storage_label = "VOLATILE (overlay RAM) — WILL NOT PERSIST";
                g_volatile_presets = true;
                fprintf(stderr,
                    "\n"
                    "  !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!\n"
                    "  !!!  WARNING: No persistent storage available  !!!\n"
                    "  !!!  User presets will be LOST on power cycle  !!!\n"
                    "  !!!  Check mnt-persist.mount / data bind mount !!!\n"
                    "  !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!\n"
                    "\n");
            }
        } else {
            // Not on overlay FS — regular read-write filesystem
            root = base;
            storage_label = "local filesystem — durable";
        }

        // Create required directories
        snprintf(path, sizeof(path), "%s/data", root);
        if (mkdir(path, 0755) != 0 && errno != EEXIST)
            fprintf(stderr, "  !!! Failed to create directory: %s\n", path);
        snprintf(path, sizeof(path), "%s/data/presets", root);
        if (mkdir(path, 0755) != 0 && errno != EEXIST)
            fprintf(stderr, "  !!! Failed to create directory: %s\n", path);

        // Write-test: verify the selected path is actually writable.
        // If not (permission issues), fall through to the next option.
        // Uses mkstemp to avoid predictable filenames.
        auto write_test = [](const char* dir) -> bool {
            char tmpl[PATH_MAX];
            snprintf(tmpl, sizeof(tmpl), "%s/.write-test-XXXXXX", dir);
            int fd = mkstemp(tmpl);
            if (fd >= 0) {
                close(fd);
                unlink(tmpl);
                return true;
            }
            return false;
        };

        if (overlay && !g_volatile_presets) {
            char presets_dir[PATH_MAX];
            snprintf(presets_dir, sizeof(presets_dir),
                     "%s/data/presets", root);
            if (!write_test(presets_dir)) {
                fprintf(stderr,
                    "  !!! Write test FAILED for %s: %s\n"
                    "  !!! Trying next storage option...\n",
                    root, strerror(errno));

                // Try the other durable option
                if (root == base && use_persist) {
                    root = persist;
                    storage_label = "persist mount — durable (fallback)";
                } else if (root == persist) {
                    char data_dir2[PATH_MAX];
                    snprintf(data_dir2, sizeof(data_dir2), "%s/data", base);
                    if (is_mount_point(data_dir2)) {
                        root = base;
                        storage_label = "bind-mount — durable (fallback)";
                    }
                }

                // Ensure dirs exist for the fallback root (idempotent)
                snprintf(path, sizeof(path), "%s/data", root);
                mkdir(path, 0755);
                snprintf(path, sizeof(path), "%s/data/presets", root);
                mkdir(path, 0755);

                snprintf(presets_dir, sizeof(presets_dir),
                         "%s/data/presets", root);
                if (!write_test(presets_dir)) {
                    fprintf(stderr,
                        "  !!! Write test FAILED for fallback too: %s\n",
                        strerror(errno));
                    root = base;
                    storage_label =
                        "VOLATILE (overlay RAM) — WILL NOT PERSIST";
                    g_volatile_presets = true;
                    fprintf(stderr,
                        "\n"
                        "  !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!\n"
                        "  !!!  WARNING: No writable storage available    !!!\n"
                        "  !!!  User presets will be LOST on power cycle  !!!\n"
                        "  !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!\n"
                        "\n");
                }
            }
        }

        snprintf(path, sizeof(path), "%s/data/presets/user_presets.txt", root);
        printf("  Preset file: %s\n", path);
        printf("  Storage:     %s\n", storage_label);
        return path;
    }

    // Fallback: use HOME-based path (non-overlay systems / development)
    const char* home = getenv("HOME");
    if (!home || home[0] == '\0') {
        struct passwd* pw = getpwuid(getuid());
        if (pw) home = pw->pw_dir;
    }
    if (!home || home[0] == '\0') home = "/tmp";
    snprintf(path, sizeof(path), "%s/.config", home);
    if (mkdir(path, 0755) != 0 && errno != EEXIST)
        fprintf(stderr, "  !!! Failed to create directory: %s\n", path);
    snprintf(path, sizeof(path), "%s/.config/dubsiren", home);
    if (mkdir(path, 0755) != 0 && errno != EEXIST)
        fprintf(stderr, "  !!! Failed to create directory: %s\n", path);
    snprintf(path, sizeof(path), "%s/.config/dubsiren/user_presets.txt", home);

    if (overlay) {
        g_volatile_presets = true;
        fprintf(stderr,
            "  !!! HOME fallback on overlay FS — presets WILL NOT PERSIST\n");
    }
    printf("  Preset file: %s (HOME fallback%s)\n", path,
           overlay ? " — VOLATILE" : "");
    return path;
}

// ─── Config file path (derived from preset path) ─────────────────────
// The preset path is e.g. .../data/presets/user_presets.txt
// We go up two dirs to get .../data, then append config/siren_config.txt

static const char* config_file_path()
{
    static char cfg_path[PATH_MAX] = {};
    if (cfg_path[0]) return cfg_path;

    const char* ppath = preset_file_path();
    // Copy and strip "/presets/user_presets.txt" to get the data dir
    char data_dir[PATH_MAX];
    snprintf(data_dir, sizeof(data_dir), "%s", ppath);
    char* slash = strrchr(data_dir, '/');   // strip "user_presets.txt"
    if (slash) *slash = '\0';
    slash = strrchr(data_dir, '/');          // strip "presets"
    if (slash) *slash = '\0';

    // Create config directory — data_dir is already well under PATH_MAX
    // since it's derived from preset_file_path() with components stripped
    size_t dlen = strlen(data_dir);
    if (dlen + 25 >= PATH_MAX) {  // "/config/siren_config.txt" = 24 chars + NUL
        fprintf(stderr, "  !!! Config path too long\n");
        snprintf(cfg_path, sizeof(cfg_path), "/tmp/siren_config.txt");
        return cfg_path;
    }
    char config_dir[PATH_MAX];
    snprintf(config_dir, sizeof(config_dir), "%.*s/config",
             (int)(sizeof(config_dir) - 8), data_dir);
    if (mkdir(config_dir, 0755) != 0 && errno != EEXIST)
        fprintf(stderr, "  !!! Failed to create directory: %s\n", config_dir);

    snprintf(cfg_path, sizeof(cfg_path), "%.*s/siren_config.txt",
             (int)(sizeof(cfg_path) - 18), config_dir);
    printf("  Config file: %s\n", cfg_path);
    return cfg_path;
}

// ─── Save / Load siren configuration ─────────────────────────────────

static constexpr const char* CONFIG_V1_HEADER = "DUBSIREN_CONFIG_V1";

static std::mutex g_config_save_mutex;

static void save_siren_config()
{
    std::lock_guard<std::mutex> lock(g_config_save_mutex);
    const char* path = config_file_path();

    char tmp[PATH_MAX];
    snprintf(tmp, sizeof(tmp), "%s.tmp", path);

    FILE* f = fopen(tmp, "w");
    if (!f) {
        fprintf(stderr, "  !!! Failed to save config: %s\n", tmp);
        return;
    }

    fprintf(f, "%s\n", CONFIG_V1_HEADER);
    fprintf(f, "reverb_type=%d\n",    g_reverb_type.load());
    fprintf(f, "delay_type=%d\n",     g_delay_type.load());
    fprintf(f, "tape_wobble=%.6f\n",  g_tape_wobble.load());
    fprintf(f, "tape_flutter=%.6f\n", g_tape_flutter.load());
    fprintf(f, "fx_chain=%d\n",       g_fx_chain.load());
    fprintf(f, "lfo_pitch_link=%d\n", g_lfo_pitch_link.load() ? 1 : 0);
    fprintf(f, "super_drip=%d\n",     g_super_drip.load() ? 1 : 0);
    fprintf(f, "sweep_dir=%.6f\n",    g_sweep_dir.load());
    fprintf(f, "phaser_mix=%.6f\n",    g_phaser_mix.load());
    fprintf(f, "chorus_mix=%.6f\n",   g_chorus_mix.load());
    fprintf(f, "flanger_mix=%.6f\n",  g_flanger_mix.load());
    fprintf(f, "saturator_mix=%.6f\n", g_saturator_mix.load());
    fprintf(f, "saturator_drive=%.6f\n", g_saturator_drive.load());
    fprintf(f, "active_bank=%d\n",    g_bank_mode.load());
    fprintf(f, "active_preset=%d\n",  g_preset.load());
    fprintf(f, "theme=%s\n",          THEME_IDS[std::clamp(g_theme.load(), 0, NUM_THEMES - 1)]);
    fprintf(f, "led_brightness=%.6f\n", g_led_brightness.load());
    fprintf(f, "led_night_mode=%d\n",   g_led_night_mode.load() ? 1 : 0);

    // Encoder mapping
    {
        std::lock_guard<std::mutex> lk(g_encoder_map_mutex);
        for (int i = 0; i < 5; i++)
            fprintf(f, "enc_a%d=%d\n", i, g_encoder_map_a[i]);
        for (int i = 0; i < 5; i++)
            fprintf(f, "enc_b%d=%d\n", i, g_encoder_map_b[i]);
    }

    // Parameter sensitivity
    {
        std::lock_guard<std::mutex> lk(g_sensitivity_mutex);
        for (int i = 0; i < NUM_PARAMS; i++)
            fprintf(f, "sens_%d=%.6f\n", i, g_param_sensitivity[i]);
    }

    fflush(f);
    fsync(fileno(f));
    fclose(f);

    if (rename(tmp, path) != 0) {
        fprintf(stderr, "  !!! Failed to rename config file: %s → %s (%s)\n",
                tmp, path, strerror(errno));
        return;
    }

    // Sync directory for durability
    char dir[PATH_MAX];
    snprintf(dir, sizeof(dir), "%s", path);
    char* sl = strrchr(dir, '/');
    if (sl) {
        *sl = '\0';
        int dfd = open(dir, O_RDONLY);
        if (dfd >= 0) { fsync(dfd); close(dfd); }
    }

    printf("  Config written to %s\n", path);
}

static void load_siren_config()
{
    const char* path = config_file_path();
    FILE* f = fopen(path, "r");
    if (!f) {
        printf("  No saved config at %s — using defaults\n", path);
        return;
    }

    char header[64];
    if (!fgets(header, sizeof(header), f)) {
        fprintf(stderr, "  !!! Empty config file — ignoring\n");
        fclose(f);
        return;
    }
    // Strip newline
    size_t hlen = strlen(header);
    while (hlen > 0 && (header[hlen-1] == '\n' || header[hlen-1] == '\r'))
        header[--hlen] = '\0';

    if (strcmp(header, CONFIG_V1_HEADER) != 0) {
        fprintf(stderr, "  !!! Unknown config header '%s' — ignoring\n", header);
        fclose(f);
        return;
    }

    char line[256];
    while (fgets(line, sizeof(line), f)) {
        // Strip newline
        size_t ll = strlen(line);
        while (ll > 0 && (line[ll-1] == '\n' || line[ll-1] == '\r'))
            line[--ll] = '\0';

        char* eq = strchr(line, '=');
        if (!eq) continue;
        *eq = '\0';
        const char* key = line;
        const char* val = eq + 1;

        if (strcmp(key, "reverb_type") == 0)
            g_reverb_type.store(atoi(val));
        else if (strcmp(key, "delay_type") == 0)
            g_delay_type.store(atoi(val));
        else if (strcmp(key, "tape_wobble") == 0)
            g_tape_wobble.store(static_cast<float>(atof(val)));
        else if (strcmp(key, "tape_flutter") == 0)
            g_tape_flutter.store(static_cast<float>(atof(val)));
        else if (strcmp(key, "fx_chain") == 0)
            g_fx_chain.store(atoi(val));
        else if (strcmp(key, "lfo_pitch_link") == 0)
            g_lfo_pitch_link.store(atoi(val) != 0);
        else if (strcmp(key, "super_drip") == 0)
            g_super_drip.store(atoi(val) != 0);
        else if (strcmp(key, "sweep_dir") == 0)
            g_sweep_dir.store(static_cast<float>(atof(val)));
        else if (strcmp(key, "phaser_mix") == 0)
            g_phaser_mix.store(static_cast<float>(atof(val)));
        else if (strcmp(key, "chorus_mix") == 0)
            g_chorus_mix.store(static_cast<float>(atof(val)));
        else if (strcmp(key, "flanger_mix") == 0)
            g_flanger_mix.store(static_cast<float>(atof(val)));
        else if (strcmp(key, "saturator_mix") == 0)
            g_saturator_mix.store(static_cast<float>(atof(val)));
        else if (strcmp(key, "saturator_drive") == 0)
            g_saturator_drive.store(static_cast<float>(atof(val)));
        // backwards compat: old boolean phaser/chorus/flanger keys
        else if (strcmp(key, "phaser") == 0)
            g_phaser_mix.store(atoi(val) != 0 ? 0.5f : 0.0f);
        else if (strcmp(key, "chorus") == 0)
            g_chorus_mix.store(atoi(val) != 0 ? 0.5f : 0.0f);
        else if (strcmp(key, "flanger") == 0)
            g_flanger_mix.store(atoi(val) != 0 ? 0.5f : 0.0f);
        else if (strcmp(key, "active_bank") == 0)
            g_bank_mode.store(atoi(val));
        else if (strcmp(key, "active_preset") == 0)
            g_preset.store(atoi(val));
        else if (strcmp(key, "theme") == 0) {
            // Try name first, fall back to legacy integer index
            bool found = false;
            for (int i = 0; i < NUM_THEMES; i++) {
                if (strcmp(val, THEME_IDS[i]) == 0) { g_theme.store(i); found = true; break; }
            }
            if (!found) {
                int t = atoi(val);
                if (t >= 0 && t < NUM_THEMES) g_theme.store(t);
            }
        }
        else if (strcmp(key, "led_brightness") == 0)
            g_led_brightness.store(std::clamp(static_cast<float>(atof(val)), 0.0f, 1.0f));
        else if (strcmp(key, "led_night_mode") == 0)
            g_led_night_mode.store(atoi(val) != 0);
        // Encoder mapping
        else if (strncmp(key, "enc_a", 5) == 0 && key[5] >= '0' && key[5] <= '4') {
            int idx = key[5] - '0';
            int v = atoi(val);
            if (v >= 0 && v < NUM_PARAMS) g_encoder_map_a[idx] = v;
        }
        else if (strncmp(key, "enc_b", 5) == 0 && key[5] >= '0' && key[5] <= '4') {
            int idx = key[5] - '0';
            int v = atoi(val);
            if (v >= 0 && v < NUM_PARAMS) g_encoder_map_b[idx] = v;
        }
        else if (strncmp(key, "sens_", 5) == 0) {
            int idx = atoi(key + 5);
            if (idx >= 0 && idx < NUM_PARAMS) {
                float v = static_cast<float>(atof(val));
                if (v >= 0.25f && v <= 4.0f) {
                    std::lock_guard<std::mutex> lk(g_sensitivity_mutex);
                    g_param_sensitivity[idx] = v;
                }
            }
        }
    }

    fclose(f);
    printf("  Config loaded from %s\n", path);
}

// ─── Snapshot current state into a UserPreset ────────────────────────

static UserPreset snapshot_current()
{
    UserPreset u;
    u.saved         = true;
    snprintf(u.name, sizeof(u.name), "User Preset");
    u.waveform      = g_waveform.load();
    u.lfo_wave      = g_lfo_waveform.load();
    u.freq          = g_freq.load();
    u.lfo_rate      = g_lfo_rate.load();
    u.lfo_depth     = g_lfo_depth.load();
    u.filter_cutoff = g_filter_cutoff.load();
    u.filter_reso   = g_filter_reso.load();
    u.delay_time    = g_delay_time.load();
    u.delay_feedback= g_delay_feedback.load();
    u.delay_mix     = g_delay_mix.load();
    u.reverb_mix    = g_reverb_mix.load();
    u.release_time  = g_release_time.load();
    u.sweep_dir     = g_sweep_dir.load();
    u.pitch_env     = g_pitch_env.load();
    u.super_drip    = g_super_drip.load();
    u.reverb_type   = g_reverb_type.load();
    u.delay_type    = g_delay_type.load();
    u.tape_wobble   = g_tape_wobble.load();
    u.tape_flutter  = g_tape_flutter.load();
    return u;
}

// Verify that the preset file was written correctly (read-back check)
static bool verify_preset_file(const char* path)
{
    FILE* f = fopen(path, "r");
    if (!f) {
        fprintf(stderr, "  !!! Verify FAILED: cannot reopen %s: %s\n",
                path, strerror(errno));
        return false;
    }
    char header[64];
    if (!fgets(header, sizeof(header), f) ||
        (strncmp(header, PRESET_V3_HEADER, sizeof(PRESET_V3_HEADER) - 1) != 0 &&
         strncmp(header, PRESET_V2_HEADER, sizeof(PRESET_V2_HEADER) - 1) != 0)) {
        fprintf(stderr, "  !!! Verify FAILED: bad header in %s\n", path);
        fclose(f);
        return false;
    }
    int lines = 0;
    char line[512];
    while (fgets(line, sizeof(line), f)) lines++;
    fclose(f);
    if (lines != NUM_USER_PRESETS) {
        fprintf(stderr, "  !!! Verify FAILED: expected %d presets, got %d in %s\n",
                NUM_USER_PRESETS, lines, path);
        return false;
    }
    return true;
}

// ─── Save / load user presets to disk ────────────────────────────────

static std::mutex g_preset_save_mutex;

static void save_user_presets()
{
    std::lock_guard<std::mutex> lock(g_preset_save_mutex);
    const char* path = preset_file_path();

    // Write to temp file, then rename for power-safety
    char tmp[PATH_MAX];
    snprintf(tmp, sizeof(tmp), "%s.tmp", path);

    FILE* f = fopen(tmp, "w");
    if (!f) {
        fprintf(stderr, "  !!! Failed to save presets: %s\n", tmp);
        return;
    }

    fprintf(f, "%s\n", PRESET_V3_HEADER);
    for (int i = 0; i < NUM_USER_PRESETS; i++) {
        const UserPreset& u = g_user_presets[i];
        // V3 format: all V2 fields + name + reverb_type + delay_type + wobble + flutter
        // Name is quoted to handle spaces
        fprintf(f, "%d \"%s\" %d %d %.6f %.6f %.6f %.6f %.6f %.6f %.6f %.6f %.6f %.6f %.6f %d %d %d %d %.6f %.6f\n",
                u.saved ? 1 : 0,
                u.name,
                u.waveform, u.lfo_wave,
                u.freq, u.lfo_rate, u.lfo_depth,
                u.filter_cutoff, u.filter_reso,
                u.delay_time, u.delay_feedback, u.delay_mix,
                u.reverb_mix, u.release_time, u.sweep_dir,
                u.pitch_env,
                u.super_drip ? 1 : 0,
                u.reverb_type, u.delay_type,
                u.tape_wobble, u.tape_flutter);
    }

    fflush(f);
    fsync(fileno(f));
    fclose(f);
    if (rename(tmp, path) != 0) {
        fprintf(stderr, "  !!! Failed to rename preset file: %s → %s (%s)\n",
                tmp, path, strerror(errno));
        return;
    }

    // Sync the directory so the rename is durable across power loss
    char dir[PATH_MAX];
    snprintf(dir, sizeof(dir), "%s", path);
    char* slash = strrchr(dir, '/');
    if (slash) {
        *slash = '\0';
        int dfd = open(dir, O_RDONLY);
        if (dfd >= 0) { fsync(dfd); close(dfd); }
    }

    printf("  Presets written to %s\n", path);

    if (!verify_preset_file(path))
        fprintf(stderr, "  !!! WARNING: Preset file verification failed!\n");

    if (g_volatile_presets) {
        fprintf(stderr,
            "  !!! WARNING: Preset saved to volatile storage — "
            "will be lost on power cycle!\n");
    }
}

// Validate that a loaded preset's enum fields are in range
static bool validate_preset(const UserPreset& u, int pitch_env, int index)
{
    if (u.waveform < 0 || u.waveform >= static_cast<int>(Waveform::COUNT) ||
        u.lfo_wave < 0 || u.lfo_wave >= static_cast<int>(LfoWave::COUNT) ||
        u.sweep_dir < -1 || u.sweep_dir > 1 ||
        pitch_env < -1 || pitch_env > 1) {
        fprintf(stderr, "  !!! Preset %d has invalid values — skipping\n", index);
        return false;
    }
    return true;
}

static void load_user_presets()
{
    const char* path = preset_file_path();
    FILE* f = fopen(path, "r");
    if (!f) {
        printf("  No saved presets at %s — using factory defaults\n", path);
        return;
    }

    char header[64];
    if (!fgets(header, sizeof(header), f)) {
        fprintf(stderr, "  !!! Empty preset file — ignoring\n");
        fclose(f);
        return;
    }

    bool v3 = (strncmp(header, PRESET_V3_HEADER, sizeof(PRESET_V3_HEADER) - 1) == 0);
    bool v2 = (strncmp(header, PRESET_V2_HEADER, sizeof(PRESET_V2_HEADER) - 1) == 0);
    bool v1 = (strncmp(header, PRESET_V1_HEADER, sizeof(PRESET_V1_HEADER) - 1) == 0);
    if (!v1 && !v2 && !v3) {
        fprintf(stderr, "  !!! Invalid preset file — ignoring\n");
        fclose(f);
        return;
    }

    // V1 had 8 slots, V2/V3 have 4 — only load what fits
    int slots_in_file = v1 ? 8 : NUM_USER_PRESETS;

    for (int i = 0; i < slots_in_file; i++) {
        // V1 files have 8 presets but we only have 4 slots now
        bool skip = (i >= NUM_USER_PRESETS);
        UserPreset tmp;
        UserPreset& u = skip ? tmp : g_user_presets[i];

        if (v3) {
            // V3 format: saved "name" waveform lfo_wave freq ... reverb_type delay_type wobble flutter
            int saved, pe, sd, rt, dt;
            char name_buf[32] = {};
            // Read saved flag
            if (fscanf(f, " %d", &saved) != 1) break;
            // Read quoted name
            int ch;
            while ((ch = fgetc(f)) != EOF && ch != '"') {}  // skip to opening quote
            if (ch == EOF) break;
            int ni = 0;
            while ((ch = fgetc(f)) != EOF && ch != '"' && ni < 31)
                name_buf[ni++] = static_cast<char>(ch);
            name_buf[ni] = '\0';
            // Read remaining fields
            int n = fscanf(f, " %d %d %f %f %f %f %f %f %f %f %f %f %f %d %d %d %d %f %f",
                           &u.waveform, &u.lfo_wave,
                           &u.freq, &u.lfo_rate, &u.lfo_depth,
                           &u.filter_cutoff, &u.filter_reso,
                           &u.delay_time, &u.delay_feedback, &u.delay_mix,
                           &u.reverb_mix, &u.release_time, &u.sweep_dir,
                           &pe, &sd, &rt, &dt,
                           &u.tape_wobble, &u.tape_flutter);
            if (n == 19) {
                if (!validate_preset(u, pe, i)) { u.saved = false; continue; }
                u.saved       = (saved != 0);
                snprintf(u.name, sizeof(u.name), "%s", name_buf);
                u.pitch_env   = pe;
                u.super_drip  = (sd != 0);
                u.reverb_type = rt;
                u.delay_type  = dt;
            } else {
                break;
            }
        } else if (v2) {
            int saved, pe, sd;
            int n = fscanf(f, "%d %d %d %f %f %f %f %f %f %f %f %f %f %f %d %d",
                           &saved,
                           &u.waveform, &u.lfo_wave,
                           &u.freq, &u.lfo_rate, &u.lfo_depth,
                           &u.filter_cutoff, &u.filter_reso,
                           &u.delay_time, &u.delay_feedback, &u.delay_mix,
                           &u.reverb_mix, &u.release_time, &u.sweep_dir,
                           &pe, &sd);
            if (n == 16) {
                if (!validate_preset(u, pe, i)) { u.saved = false; continue; }
                u.saved       = (saved != 0);
                snprintf(u.name, sizeof(u.name), "User %d", i + 1);
                u.pitch_env   = pe;
                u.super_drip  = (sd != 0);
                u.reverb_type = 0;  // V2 default: spring
                u.delay_type  = 0;  // V2 default: tape
                u.tape_wobble  = 1.0f;
                u.tape_flutter = 1.0f;
            } else {
                break;
            }
        } else {
            // V1 format: 17 fields (includes delay_link, lfo_pitch_link)
            int saved, dl, lpl, sd;
            int n = fscanf(f, "%d %d %d %f %f %f %f %f %f %f %f %f %f %f %d %d %d",
                           &saved,
                           &u.waveform, &u.lfo_wave,
                           &u.freq, &u.lfo_rate, &u.lfo_depth,
                           &u.filter_cutoff, &u.filter_reso,
                           &u.delay_time, &u.delay_feedback, &u.delay_mix,
                           &u.reverb_mix, &u.release_time, &u.sweep_dir,
                           &dl, &lpl, &sd);
            if (n == 17) {
                if (!validate_preset(u, 0, i)) { u.saved = false; continue; }
                u.saved       = (saved != 0);
                snprintf(u.name, sizeof(u.name), "User %d", i + 1);
                u.pitch_env   = 0;
                u.super_drip  = (sd != 0);
                u.reverb_type = 0;
                u.delay_type  = 0;
                u.tape_wobble  = 1.0f;
                u.tape_flutter = 1.0f;
            } else {
                break;
            }
        }
    }

    fclose(f);
    int loaded_count = 0;
    for (int j = 0; j < NUM_USER_PRESETS; j++)
        if (g_user_presets[j].saved) loaded_count++;
    const char* ver_str = v3 ? "V3" : (v2 ? "V2→V3 migration" : "V1→V3 migration");
    printf("  User presets loaded from %s (%s, %d/%d slots populated)\n",
           path, ver_str, loaded_count, NUM_USER_PRESETS);
}

static void init_user_presets()
{
    // Start with factory defaults in all user slots
    for (int i = 0; i < NUM_USER_PRESETS; i++) {
        const DubPreset& f = PRESETS[i % NUM_PRESETS];
        UserPreset& u = g_user_presets[i];
        u.saved         = false;
        snprintf(u.name, sizeof(u.name), "%s", f.name);
        u.waveform      = f.waveform;
        u.lfo_wave      = f.lfo_wave;
        u.freq          = f.freq;
        u.lfo_rate      = f.lfo_rate;
        u.lfo_depth     = f.lfo_depth;
        u.filter_cutoff = f.filter_cutoff;
        u.filter_reso   = f.filter_reso;
        u.delay_time    = f.delay_time;
        u.delay_feedback= f.delay_feedback;
        u.delay_mix     = f.delay_mix;
        u.reverb_mix    = f.reverb_mix;
        u.release_time  = f.release_time;
        u.sweep_dir     = f.sweep_dir;
        u.pitch_env     = f.pitch_env;
        u.super_drip    = true;
        u.reverb_type   = f.reverb_type;
        u.delay_type    = f.delay_type;
        u.tape_wobble   = f.tape_wobble;
        u.tape_flutter  = f.tape_flutter;
    }

    // Migrate from old ~/.config/dubsiren/ location if new path has no file
    const char* new_path = preset_file_path();
    if (access(new_path, F_OK) != 0) {
        const char* home = getenv("HOME");
        if (!home || home[0] == '\0') {
            struct passwd* pw = getpwuid(getuid());
            if (pw) home = pw->pw_dir;
        }
        if (home && home[0] != '\0') {
            char old_path[PATH_MAX];
            snprintf(old_path, sizeof(old_path),
                     "%s/.config/dubsiren/user_presets.txt", home);
            if (access(old_path, R_OK) == 0) {
                // Copy old file to new persistent location
                FILE* src = fopen(old_path, "r");
                FILE* dst = fopen(new_path, "w");
                if (src && dst) {
                    char buf[4096];
                    size_t n;
                    while ((n = fread(buf, 1, sizeof(buf), src)) > 0)
                        fwrite(buf, 1, n, dst);
                    fflush(dst);
                    fsync(fileno(dst));
                    printf("  Migrated presets from %s\n", old_path);
                }
                if (src) fclose(src);
                if (dst) fclose(dst);
            }
        }
    }

    // Override with saved data from disk
    load_user_presets();
}

// ─── Helper: copy DubPreset → UserPreset ────────────────────────────

static void copy_dub_to_user(const DubPreset& f, UserPreset& u)
{
    u.saved         = true;
    snprintf(u.name, sizeof(u.name), "%s", f.name);
    u.waveform      = f.waveform;
    u.lfo_wave      = f.lfo_wave;
    u.freq          = f.freq;
    u.lfo_rate      = f.lfo_rate;
    u.lfo_depth     = f.lfo_depth;
    u.filter_cutoff = f.filter_cutoff;
    u.filter_reso   = f.filter_reso;
    u.delay_time    = f.delay_time;
    u.delay_feedback= f.delay_feedback;
    u.delay_mix     = f.delay_mix;
    u.reverb_mix    = f.reverb_mix;
    u.release_time  = f.release_time;
    u.sweep_dir     = f.sweep_dir;
    u.pitch_env     = f.pitch_env;
    u.super_drip    = false;
    u.reverb_type   = f.reverb_type;
    u.delay_type    = f.delay_type;
    u.tape_wobble   = f.tape_wobble;
    u.tape_flutter  = f.tape_flutter;
}

// ─── Bank preset file path helper ───────────────────────────────────

static const char* bank_file_path(const char* filename)
{
    static char std_path[PATH_MAX] = {};
    static char exp_path[PATH_MAX] = {};
    char* buf;
    if (strcmp(filename, "standard_presets.txt") == 0)
        buf = std_path;
    else
        buf = exp_path;
    if (buf[0]) return buf;

    // Derive from preset_file_path() by replacing the filename
    const char* ppath = preset_file_path();
    char dir[PATH_MAX];
    snprintf(dir, sizeof(dir), "%s", ppath);
    char* slash = strrchr(dir, '/');
    if (slash) *slash = '\0';
    snprintf(buf, PATH_MAX, "%s/%s", dir, filename);
    return buf;
}

// ─── Save / load bank presets (standard + experimental) ─────────────

static void save_bank_presets(UserPreset* bank, int count, const char* filename)
{
    std::lock_guard<std::mutex> lock(g_preset_save_mutex);
    const char* path = bank_file_path(filename);

    char tmp[PATH_MAX];
    snprintf(tmp, sizeof(tmp), "%s.tmp", path);

    FILE* f = fopen(tmp, "w");
    if (!f) {
        fprintf(stderr, "  !!! Failed to save bank presets: %s\n", tmp);
        return;
    }

    fprintf(f, "%s\n", PRESET_V3_HEADER);
    for (int i = 0; i < count; i++) {
        const UserPreset& u = bank[i];
        fprintf(f, "%d \"%s\" %d %d %.6f %.6f %.6f %.6f %.6f %.6f %.6f %.6f %.6f %.6f %.6f %d %d %d %d %.6f %.6f\n",
                u.saved ? 1 : 0,
                u.name,
                u.waveform, u.lfo_wave,
                u.freq, u.lfo_rate, u.lfo_depth,
                u.filter_cutoff, u.filter_reso,
                u.delay_time, u.delay_feedback, u.delay_mix,
                u.reverb_mix, u.release_time, u.sweep_dir,
                u.pitch_env,
                u.super_drip ? 1 : 0,
                u.reverb_type, u.delay_type,
                u.tape_wobble, u.tape_flutter);
    }

    fflush(f);
    fsync(fileno(f));
    fclose(f);
    if (rename(tmp, path) != 0) {
        fprintf(stderr, "  !!! Failed to rename bank file: %s → %s (%s)\n",
                tmp, path, strerror(errno));
        return;
    }

    char dir[PATH_MAX];
    snprintf(dir, sizeof(dir), "%s", path);
    char* sl = strrchr(dir, '/');
    if (sl) {
        *sl = '\0';
        int dfd = open(dir, O_RDONLY);
        if (dfd >= 0) { fsync(dfd); close(dfd); }
    }
    printf("  Bank presets written to %s\n", path);
}

static void load_bank_presets(UserPreset* bank, int count, const char* filename)
{
    const char* path = bank_file_path(filename);
    FILE* f = fopen(path, "r");
    if (!f) return;  // no saved file — keep defaults

    char header[64];
    if (!fgets(header, sizeof(header), f) ||
        strncmp(header, PRESET_V3_HEADER, sizeof(PRESET_V3_HEADER) - 1) != 0) {
        fclose(f);
        return;
    }

    for (int i = 0; i < count; i++) {
        int saved, pe, sd, rt, dt;
        char name_buf[32] = {};
        if (fscanf(f, " %d", &saved) != 1) break;
        int ch;
        while ((ch = fgetc(f)) != EOF && ch != '"') {}
        if (ch == EOF) break;
        int ni = 0;
        while ((ch = fgetc(f)) != EOF && ch != '"' && ni < 31)
            name_buf[ni++] = static_cast<char>(ch);
        name_buf[ni] = '\0';
        UserPreset& u = bank[i];
        int n = fscanf(f, " %d %d %f %f %f %f %f %f %f %f %f %f %f %d %d %d %d %f %f",
                       &u.waveform, &u.lfo_wave,
                       &u.freq, &u.lfo_rate, &u.lfo_depth,
                       &u.filter_cutoff, &u.filter_reso,
                       &u.delay_time, &u.delay_feedback, &u.delay_mix,
                       &u.reverb_mix, &u.release_time, &u.sweep_dir,
                       &pe, &sd, &rt, &dt,
                       &u.tape_wobble, &u.tape_flutter);
        if (n == 19) {
            if (!validate_preset(u, pe, i)) { u.saved = false; continue; }
            u.saved       = (saved != 0);
            snprintf(u.name, sizeof(u.name), "%s", name_buf);
            u.pitch_env   = pe;
            u.super_drip  = (sd != 0);
            u.reverb_type = rt;
            u.delay_type  = dt;
        } else {
            break;
        }
    }
    fclose(f);
    printf("  Bank presets loaded from %s\n", path);
}

static void save_standard_presets()
{
    save_bank_presets(g_standard_presets, NUM_PRESETS, "standard_presets.txt");
}

static void save_experimental_presets()
{
    save_bank_presets(g_experimental_presets, NUM_EXPERIMENTAL, "experimental_presets.txt");
}

static void init_bank_presets()
{
    // Initialize standard bank from factory defaults
    for (int i = 0; i < NUM_PRESETS; i++)
        copy_dub_to_user(PRESETS[i], g_standard_presets[i]);
    // Initialize experimental bank from factory defaults
    for (int i = 0; i < NUM_EXPERIMENTAL; i++)
        copy_dub_to_user(EXPERIMENTAL[i], g_experimental_presets[i]);

    // Override with saved data from disk (if any)
    load_bank_presets(g_standard_presets, NUM_PRESETS, "standard_presets.txt");
    load_bank_presets(g_experimental_presets, NUM_EXPERIMENTAL, "experimental_presets.txt");
}

// ─── Apply user preset ──────────────────────────────────────────────

static void apply_user_preset(const UserPreset& u)
{
    g_waveform.store(u.waveform);
    g_lfo_waveform.store(u.lfo_wave);
    g_freq.store(u.freq);
    g_lfo_rate.store(u.lfo_rate);
    g_lfo_depth.store(u.lfo_depth);
    g_filter_cutoff.store(u.filter_cutoff);
    g_filter_reso.store(u.filter_reso);
    g_delay_time.store(u.delay_time);
    g_delay_feedback.store(u.delay_feedback);
    g_delay_mix.store(u.delay_mix);
    g_reverb_mix.store(u.reverb_mix);
    g_release_time.store(u.release_time);
    g_sweep_dir.store(u.sweep_dir);
    g_pitch_env.store(u.pitch_env);
    g_super_drip.store(u.super_drip);
    g_reverb_type.store(u.reverb_type);
    g_delay_type.store(u.delay_type);
    g_tape_wobble.store(u.tape_wobble);
    g_tape_flutter.store(u.tape_flutter);

    update_link_eff();
}

static void apply_preset(int idx)
{
    apply_user_preset(g_standard_presets[idx % NUM_PRESETS]);
}

static void apply_experimental(int idx)
{
    apply_user_preset(g_experimental_presets[idx % NUM_EXPERIMENTAL]);
}

// ─── Bank name helper ─────────────────────────────────────────────────

static const char* bank_name(BankMode m)
{
    switch (m) {
    case BankMode::USER:         return "USER";
    case BankMode::STANDARD:     return "STANDARD";
    case BankMode::EXPERIMENTAL: return "EXPERIMENTAL";
    }
    return "?";
}

// ─── Switch to a specific bank ────────────────────────────────────────

static void switch_bank(BankMode mode)
{
    g_bank_mode.store(static_cast<int>(mode));
    int idx = g_preset.load();
    switch (mode) {
    case BankMode::USER:
        if (idx >= NUM_USER_PRESETS) { idx = 0; g_preset.store(idx); }
        apply_user_preset(g_user_presets[idx]);
        slog("BANK: USER slot %d%s", idx + 1,
             g_user_presets[idx].saved ? "" : " (factory copy)");
        break;
    case BankMode::STANDARD:
        if (idx >= NUM_PRESETS) { idx = 0; g_preset.store(idx); }
        apply_user_preset(g_standard_presets[idx]);
        slog("BANK: STANDARD \"%s\"", g_standard_presets[idx].name);
        break;
    case BankMode::EXPERIMENTAL:
        if (idx >= NUM_EXPERIMENTAL) { idx = 0; g_preset.store(idx); }
        apply_user_preset(g_experimental_presets[idx]);
        slog("BANK: EXPERIMENTAL \"%s\"", g_experimental_presets[idx].name);
        break;
    }
}

// ─── Toggle bank: switch to target, or back to USER if already there ─

static void toggle_bank(BankMode target)
{
    auto current = static_cast<BankMode>(g_bank_mode.load());
    switch_bank(current == target ? BankMode::USER : target);
}

// ─── Cycle to next preset in current bank ────────────────────────────

// ─── Save current state to user bank slot ─────────────────────────────

static void save_current_to_user_bank()
{
    int idx = g_preset.load();
    if (idx >= NUM_USER_PRESETS) idx = 0;
    g_user_presets[idx] = snapshot_current();
    save_user_presets();

    // Switch to user bank if not already
    auto bm = static_cast<BankMode>(g_bank_mode.load());
    if (bm != BankMode::USER) {
        g_preset.store(idx);
        g_bank_mode.store(static_cast<int>(BankMode::USER));
    }

    slog("SAVED to USER %d (Freq:%.0fHz %s LFO:%s)",
         idx + 1, g_freq.load(),
         waveform_name(g_waveform.load()),
         lfo_wave_name(g_lfo_waveform.load()));

    // Visual feedback: triple white blink
    g_led.blinkSave();
}

// ─── Cycle to next preset in current bank ────────────────────────────

static void cycle_preset()
{
    auto mode = static_cast<BankMode>(g_bank_mode.load());
    int count = (mode == BankMode::USER)         ? NUM_USER_PRESETS
              : (mode == BankMode::STANDARD)     ? NUM_PRESETS
              :                                    NUM_EXPERIMENTAL;
    int idx = (g_preset.load() + 1) % count;
    g_preset.store(idx);

    switch (mode) {
    case BankMode::USER:
        apply_user_preset(g_user_presets[idx]);
        slog("PRESET: USER %d \"%s\"%s", idx + 1, g_user_presets[idx].name,
             g_user_presets[idx].saved ? "" : " (factory copy)");
        printf("  USER %d%s  %s LFO:%s %.1fHz@%.0f%%  "
               "Dly:%.0fms FB%.0f%% Mix%.0f%%  Rev:%.0f%%\n",
               idx + 1,
               g_user_presets[idx].saved ? "" : " (factory copy)",
               waveform_name(g_user_presets[idx].waveform),
               lfo_wave_name(g_user_presets[idx].lfo_wave),
               g_user_presets[idx].lfo_rate,
               g_user_presets[idx].lfo_depth * 100.0f,
               g_user_presets[idx].delay_time * 1000.0f,
               g_user_presets[idx].delay_feedback * 100.0f,
               g_user_presets[idx].delay_mix * 100.0f,
               g_user_presets[idx].reverb_mix * 100.0f);
        break;
    case BankMode::STANDARD: {
        apply_preset(idx);
        const UserPreset& sp = g_standard_presets[idx];
        slog("PRESET: STANDARD %d \"%s\"", idx + 1, sp.name);
        printf("    %s  LFO:%s %.1fHz@%.0f%%  "
               "Dly:%.0fms FB%.0f%% Mix%.0f%%  "
               "Rev:%.0f%%\n",
               waveform_name(sp.waveform),
               lfo_wave_name(sp.lfo_wave),
               sp.lfo_rate, sp.lfo_depth * 100.0f,
               sp.delay_time * 1000.0f,
               sp.delay_feedback * 100.0f,
               sp.delay_mix * 100.0f,
               sp.reverb_mix * 100.0f);
        break;
    }
    case BankMode::EXPERIMENTAL: {
        apply_experimental(idx);
        const UserPreset& ep = g_experimental_presets[idx];
        slog("PRESET: EXP %d \"%s\"", idx + 1, ep.name);
        printf("    %s  LFO:%s %.1fHz@%.0f%%  "
               "Dly:%.0fms FB%.0f%% Mix%.0f%%  "
               "Rev:%.0f%%\n",
               waveform_name(ep.waveform),
               lfo_wave_name(ep.lfo_wave),
               ep.lfo_rate, ep.lfo_depth * 100.0f,
               ep.delay_time * 1000.0f,
               ep.delay_feedback * 100.0f,
               ep.delay_mix * 100.0f,
               ep.reverb_mix * 100.0f);
        break;
    }
    }
}

// ─── Helpers ────────────────────────────────────────────────────────

static const char* waveform_name(int w)
{
    static const char* names[] = {"Sine", "Square", "Saw", "Triangle"};
    return names[w % static_cast<int>(Waveform::COUNT)];
}

static const char* lfo_wave_name(int w)
{
    static const char* names[] = {
        "Sine", "Triangle", "Square", "RampUp", "RampDown",
        "S&H", "ExpRise", "ExpFall"
    };
    return names[w % static_cast<int>(LfoWave::COUNT)];
}

static void usage(const char *prog)
{
    fprintf(stderr,
        "Usage: %s [OPTIONS]\n"
        "\n"
        "  --device <hw>   ALSA device string  (default: hw:0,0)\n"
        "  --simulate      Bypass GPIO — use keyboard simulation\n"
        "  -h, --help      Show this help\n", prog);
}

// ─── Encoder acceleration ───────────────────────────────────────────
//
// Returns a multiplier (1.0–4.0) based on how fast the encoder is
// being turned.  Slow deliberate turns → 1×, fast spins → 4×.

static float encoder_accel(int id)
{
    using clock = std::chrono::steady_clock;
    static clock::time_point prev[gpio::NUM_ENCODERS] = {};
    static float smoothed[gpio::NUM_ENCODERS] = {1.0f, 1.0f, 1.0f, 1.0f, 1.0f};

    auto now   = clock::now();
    auto delta = std::chrono::duration_cast<std::chrono::milliseconds>(
                     now - prev[id]).count();
    prev[id] = now;

    float raw;
    if (delta > 200) {
        raw = 1.0f;           // first event or long pause
    } else {
        constexpr long  SLOW_MS  = 120;         // threshold for 1× (deliberate)
        constexpr long  FAST_MS  = 25;          // threshold for max (fast spin)
        constexpr float MAX_MULT = 4.0f;

        float t = static_cast<float>(SLOW_MS - delta)
                / static_cast<float>(SLOW_MS - FAST_MS);
        t = std::clamp(t, 0.0f, 1.0f);
        raw = 1.0f + t * (MAX_MULT - 1.0f);
    }

    // Exponential smoothing: fast attack, slow decay
    constexpr float SMOOTH_UP   = 0.6f;   // ramp up quickly (responsive)
    constexpr float SMOOTH_DOWN = 0.15f;   // ramp down gently (no jarring drop)
    float alpha = (raw > smoothed[id]) ? SMOOTH_UP : SMOOTH_DOWN;
    smoothed[id] += alpha * (raw - smoothed[id]);

    return smoothed[id];
}

// ─── Multi-click detector ────────────────────────────────────────────
//
// Reusable helper for triple-click (or N-click) detection on buttons
// and switches.  Tracks timestamps and fires when N clicks land within
// the given time window.

struct MultiClickDetector {
    int  required;        // number of clicks to trigger
    long window_ms;       // max ms from first to last click
    long reset_ms;        // reset if idle longer than this after first click

    std::chrono::steady_clock::time_point stamps[4] = {};
    int count = 0;

    // Call on each click.  Returns true when the pattern fires.
    bool click()
    {
        using clock = std::chrono::steady_clock;
        auto now = clock::now();

        if (count > 0) {
            auto since = std::chrono::duration_cast<
                             std::chrono::milliseconds>(
                             now - stamps[0]).count();
            if (since > reset_ms) count = 0;
        }

        stamps[count++] = now;

        if (count >= required) {
            auto span = std::chrono::duration_cast<
                            std::chrono::milliseconds>(
                            stamps[count - 1] - stamps[0]).count();
            count = 0;
            return span < window_ms;
        }
        return false;
    }
};

// ─── LFO-pitch scaling helper ────────────────────────────────────────
//
// Recomputes the effective delay time and LFO rate from their base
// values and the current frequency.  Called from the control layer
// whenever freq, delay time, or LFO rate changes.
//
// LFO rate always tracks pitch (faster modulation at higher pitches).

static void update_link_eff()
{
    float t = g_delay_time.load();
    float r = g_lfo_rate.load();
    float freq = g_freq.load();
    float ratio = freq / REF_FREQ;

    // LFO rate always tracks pitch
    r = r * ratio;
    r = std::clamp(r, 0.1f, 20.0f);

    g_delay_time_eff.store(t);
    g_lfo_rate_eff.store(r);
}

// ─── main ───────────────────────────────────────────────────────────

// ─── 3-button AP mode detection state ────────────────────────────────
//
// Trigger + Shift + Preset held simultaneously for 3 seconds enters
// AP mode.  1.5 seconds in, a rising tone provides audio feedback.

static std::atomic<bool> g_btn_trigger{false};
static std::atomic<bool> g_btn_shift{false};
static std::atomic<bool> g_btn_preset{false};

static bool g_combo_active = false;
static std::chrono::steady_clock::time_point g_combo_start{};
static bool g_combo_feedback_given = false;
// Set when combo activates, cleared when all buttons released — suppresses
// normal button-release actions (save preset, bank toggle) so they don't
// fire after the user releases a 3-button hold.
static std::atomic<bool> g_combo_suppress{false};
// Grace period: after a combo fires (enter/exit AP), ignore new combos
// until all buttons have been released for at least 2 seconds.
static std::chrono::steady_clock::time_point g_combo_release_time{};
static bool g_combo_cooldown = false;

// Forward declarations for AP mode
static void enter_ap_mode();
static void exit_ap_mode();

static void check_ap_combo(AudioEngine& audio)
{
    bool t = g_btn_trigger.load();
    bool s = g_btn_shift.load();
    bool p = g_btn_preset.load();
    bool none = !t && !s && !p;
    bool all  = t && s && p;

    // Grace period: after a combo fires, require all buttons released for 2s
    // before accepting a new combo.  This prevents the enter-AP hold from
    // immediately starting the exit-AP countdown on release.
    if (g_combo_cooldown) {
        if (!none) {
            // Buttons still held/pressed — keep resetting the release clock
            g_combo_release_time = std::chrono::steady_clock::now();
            return;
        }
        auto since_release = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now() - g_combo_release_time).count();
        if (since_release < 2000) return;
        g_combo_cooldown = false;
        g_combo_suppress.store(false);
        slog("AP COMBO: Cooldown cleared — ready for new combo");
    }

    if (all && !g_combo_active) {
        g_combo_active = true;
        g_combo_suppress.store(true);
        g_combo_start = std::chrono::steady_clock::now();
        g_combo_feedback_given = false;
        slog("AP COMBO: All 3 buttons held — hold for 3s (ap=%s)",
             g_ap_mode.load() ? "active" : "off");
    } else if (!all) {
        if (g_combo_active)
            slog("AP COMBO: Released (button dropped)");
        g_combo_active = false;
        // Clear suppress once all buttons are released
        if (none)
            g_combo_suppress.store(false);
        return;
    }

    if (!g_combo_active) return;

    auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now() - g_combo_start).count();

    if (elapsed >= 1500 && !g_combo_feedback_given) {
        g_combo_feedback_given = true;
        slog("AP COMBO: Keep holding (%.1fs)...",
             (3000 - elapsed) / 1000.0f);
    }

    if (elapsed >= 3000) {
        g_combo_active = false;
        g_combo_cooldown = true;
        g_combo_release_time = std::chrono::steady_clock::now();
        slog("AP COMBO: 3s reached — %s AP mode",
             g_ap_mode.load() ? "exiting" : "entering");
        if (g_ap_mode.load()) {
            exit_ap_mode();
        } else {
            enter_ap_mode();
        }
    }
}

// ─── AP mode entry / exit ───────────────────────────────────────────

// Flag set by web UI "exit AP" button, checked in main loop
static std::atomic<bool> g_ap_exit_requested{false};

static web_server::Callbacks build_web_callbacks()
{
    web_server::Callbacks cb;

    cb.get_all_presets = []() -> std::string {
        auto bank_json = [](const UserPreset* bank, int count) -> std::string {
            std::string j = "[";
            for (int i = 0; i < count; i++) {
                if (i > 0) j += ",";
                j += "{\"name\":\"" + std::string(bank[i].name) + "\","
                     "\"slot\":" + std::to_string(i) + ","
                     "\"saved\":" + (bank[i].saved ? "true" : "false") + ","
                     "\"waveform\":" + std::to_string(bank[i].waveform) + ","
                     "\"freq\":" + std::to_string(bank[i].freq) + "}";
            }
            j += "]";
            return j;
        };
        int active_bank = g_bank_mode.load();
        int active_preset = g_preset.load();
        std::string json = "{\"user\":" + bank_json(g_user_presets, NUM_USER_PRESETS) + ","
            "\"standard\":" + bank_json(g_standard_presets, NUM_PRESETS) + ","
            "\"experimental\":" + bank_json(g_experimental_presets, NUM_EXPERIMENTAL) + ","
            "\"active_bank\":" + std::to_string(active_bank) + ","
            "\"active_preset\":" + std::to_string(active_preset) + ","
            "\"library\":[";
        for (int i = 0; i < NUM_LIBRARY_PRESETS; i++) {
            if (i > 0) json += ",";
            json += "{\"name\":\"" + std::string(PRESET_LIBRARY[i].name) + "\","
                    "\"index\":" + std::to_string(i) + ","
                    "\"category\":\"" + std::string(PRESET_LIBRARY[i].category) + "\"}";
        }
        json += "]}";
        return json;
    };

    cb.get_preset_state = []() -> std::string {
        return "{\"freq\":" + std::to_string(g_freq.load()) +
            ",\"waveform\":" + std::to_string(g_waveform.load()) +
            ",\"lfo_rate\":" + std::to_string(g_lfo_rate.load()) +
            ",\"lfo_depth\":" + std::to_string(g_lfo_depth.load()) +
            ",\"filter_cutoff\":" + std::to_string(g_filter_cutoff.load()) +
            ",\"filter_reso\":" + std::to_string(g_filter_reso.load()) +
            ",\"delay_time\":" + std::to_string(g_delay_time.load()) +
            ",\"delay_feedback\":" + std::to_string(g_delay_feedback.load()) +
            ",\"delay_mix\":" + std::to_string(g_delay_mix.load()) +
            ",\"reverb_mix\":" + std::to_string(g_reverb_mix.load()) +
            ",\"release_time\":" + std::to_string(g_release_time.load()) +
            ",\"reverb_type\":" + std::to_string(g_reverb_type.load()) +
            ",\"delay_type\":" + std::to_string(g_delay_type.load()) +
            ",\"tape_wobble\":" + std::to_string(g_tape_wobble.load()) +
            ",\"tape_flutter\":" + std::to_string(g_tape_flutter.load()) +
            ",\"fx_chain\":" + std::to_string(g_fx_chain.load()) + "}";
    };

    cb.apply_preset = [](const std::string& category, int index) -> bool {
        if (category == "user" && index >= 0 && index < NUM_USER_PRESETS) {
            g_bank_mode.store(static_cast<int>(BankMode::USER));
            g_preset.store(index);
            apply_user_preset(g_user_presets[index]);
        } else if (category == "standard" && index >= 0 && index < NUM_PRESETS) {
            g_bank_mode.store(static_cast<int>(BankMode::STANDARD));
            g_preset.store(index);
            apply_user_preset(g_standard_presets[index]);
        } else if (category == "experimental" && index >= 0 && index < NUM_EXPERIMENTAL) {
            g_bank_mode.store(static_cast<int>(BankMode::EXPERIMENTAL));
            g_preset.store(index);
            apply_user_preset(g_experimental_presets[index]);
        } else if (category == "library" && index >= 0 && index < NUM_LIBRARY_PRESETS) {
            apply_dub_preset(PRESET_LIBRARY[index]);
        } else {
            return false;
        }
        save_siren_config();
        slog("WEB: Applied preset %s[%d]", category.c_str(), index);
        return true;
    };

    cb.save_to_slot = [](int slot, const std::string& name) -> bool {
        if (slot < 0 || slot >= NUM_USER_PRESETS) return false;
        g_user_presets[slot] = snapshot_current();
        snprintf(g_user_presets[slot].name, sizeof(g_user_presets[slot].name),
                 "%s", name.c_str());
        save_user_presets();
        return true;
    };

    cb.swap_slots = [](int a, int b) -> bool {
        if (a < 0 || a >= NUM_USER_PRESETS || b < 0 || b >= NUM_USER_PRESETS || a == b)
            return false;
        UserPreset tmp = g_user_presets[a];
        g_user_presets[a] = g_user_presets[b];
        g_user_presets[b] = tmp;
        save_user_presets();
        printf("AP: Swapped user slots %d <-> %d\n", a + 1, b + 1);
        return true;
    };

    cb.load_to_slot = [](int slot, const std::string& category, int index) -> bool {
        // Legacy endpoint — delegates to bank slot load for user bank
        if (slot < 0 || slot >= NUM_USER_PRESETS) return false;
        const DubPreset* src = nullptr;
        if (category == "standard" && index >= 0 && index < NUM_PRESETS)
            src = &PRESETS[index];
        else if (category == "experimental" && index >= 0 && index < NUM_EXPERIMENTAL)
            src = &EXPERIMENTAL[index];
        else if (category == "library" && index >= 0 && index < NUM_LIBRARY_PRESETS)
            src = &PRESET_LIBRARY[index];
        else
            return false;
        copy_dub_to_user(*src, g_user_presets[slot]);
        save_user_presets();
        return true;
    };

    // Load a library/factory preset into any bank's slot
    cb.load_to_bank_slot = [](const std::string& bank, int slot,
                              const std::string& category, int index) -> bool {
        // Determine source preset
        const DubPreset* src = nullptr;
        if (category == "standard" && index >= 0 && index < NUM_PRESETS)
            src = &PRESETS[index];
        else if (category == "experimental" && index >= 0 && index < NUM_EXPERIMENTAL)
            src = &EXPERIMENTAL[index];
        else if (category == "library" && index >= 0 && index < NUM_LIBRARY_PRESETS)
            src = &PRESET_LIBRARY[index];
        else
            return false;

        UserPreset* target = nullptr;
        int max_slot = 0;
        if (bank == "user") {
            target = g_user_presets; max_slot = NUM_USER_PRESETS;
        } else if (bank == "standard") {
            target = g_standard_presets; max_slot = NUM_PRESETS;
        } else if (bank == "experimental") {
            target = g_experimental_presets; max_slot = NUM_EXPERIMENTAL;
        } else {
            return false;
        }
        if (slot < 0 || slot >= max_slot) return false;

        copy_dub_to_user(*src, target[slot]);

        if (bank == "user") save_user_presets();
        else if (bank == "standard") save_standard_presets();
        else save_experimental_presets();

        printf("AP: Loaded \"%s\" into %s slot %d\n", src->name, bank.c_str(), slot + 1);
        return true;
    };

    // Save current DSP state to any bank's slot
    cb.save_to_bank_slot = [](const std::string& bank, int slot,
                              const std::string& name) -> bool {
        UserPreset* target = nullptr;
        int max_slot = 0;
        if (bank == "user") {
            target = g_user_presets; max_slot = NUM_USER_PRESETS;
        } else if (bank == "standard") {
            target = g_standard_presets; max_slot = NUM_PRESETS;
        } else if (bank == "experimental") {
            target = g_experimental_presets; max_slot = NUM_EXPERIMENTAL;
        } else {
            return false;
        }
        if (slot < 0 || slot >= max_slot) return false;

        target[slot] = snapshot_current();
        snprintf(target[slot].name, sizeof(target[slot].name), "%s", name.c_str());

        if (bank == "user") save_user_presets();
        else if (bank == "standard") save_standard_presets();
        else save_experimental_presets();

        printf("AP: Saved to %s slot %d as \"%s\"\n", bank.c_str(), slot + 1, name.c_str());
        return true;
    };

    cb.get_siren_options = []() -> std::string {
        int theme_idx = std::clamp(g_theme.load(), 0, NUM_THEMES - 1);
        return "{\"reverb_type\":" + std::to_string(g_reverb_type.load()) +
            ",\"delay_type\":" + std::to_string(g_delay_type.load()) +
            ",\"tape_wobble\":" + std::to_string(g_tape_wobble.load()) +
            ",\"tape_flutter\":" + std::to_string(g_tape_flutter.load()) +
            ",\"fx_chain\":" + std::to_string(g_fx_chain.load()) +
            ",\"lfo_pitch_link\":" + (g_lfo_pitch_link.load() ? "true" : "false") +
            ",\"super_drip\":" + (g_super_drip.load() ? "true" : "false") +
            ",\"sweep_dir\":" + std::to_string(static_cast<int>(g_sweep_dir.load())) +
            ",\"phaser_mix\":" + std::to_string(g_phaser_mix.load()) +
            ",\"chorus_mix\":" + std::to_string(g_chorus_mix.load()) +
            ",\"flanger_mix\":" + std::to_string(g_flanger_mix.load()) +
            ",\"saturator_mix\":" + std::to_string(g_saturator_mix.load()) +
            ",\"saturator_drive\":" + std::to_string(g_saturator_drive.load()) +
            ",\"led_brightness\":" + std::to_string(g_led_brightness.load()) +
            ",\"led_night_mode\":" + (g_led_night_mode.load() ? "true" : "false") +
            ",\"theme\":\"" + THEME_IDS[theme_idx] + "\"}";
    };

    cb.set_siren_options = [](const std::string& body) -> bool {
        // Simple key=value parsing from form data
        auto get_val = [&](const std::string& key) -> std::string {
            auto pos = body.find(key + "=");
            if (pos == std::string::npos) return "";
            auto start = pos + key.size() + 1;
            auto end = body.find('&', start);
            if (end == std::string::npos) end = body.size();
            return body.substr(start, end - start);
        };

        auto rv = get_val("reverb_type");
        if (!rv.empty()) g_reverb_type.store(std::stoi(rv));
        auto dv = get_val("delay_type");
        if (!dv.empty()) g_delay_type.store(std::stoi(dv));
        auto wv = get_val("tape_wobble");
        if (!wv.empty()) g_tape_wobble.store(std::stof(wv));
        auto fv = get_val("tape_flutter");
        if (!fv.empty()) g_tape_flutter.store(std::stof(fv));
        auto fc = get_val("fx_chain");
        if (!fc.empty()) g_fx_chain.store(std::stoi(fc));
        auto lp = get_val("lfo_pitch_link");
        if (!lp.empty()) g_lfo_pitch_link.store(lp == "1" || lp == "true");
        auto sd = get_val("super_drip");
        if (!sd.empty()) g_super_drip.store(sd == "1" || sd == "true");
        auto sw = get_val("sweep_dir");
        if (!sw.empty()) g_sweep_dir.store(std::stof(sw));
        auto pm = get_val("phaser_mix");
        if (!pm.empty()) g_phaser_mix.store(std::stof(pm));
        auto cm = get_val("chorus_mix");
        if (!cm.empty()) g_chorus_mix.store(std::stof(cm));
        auto fm = get_val("flanger_mix");
        if (!fm.empty()) g_flanger_mix.store(std::stof(fm));
        auto sm = get_val("saturator_mix");
        if (!sm.empty()) g_saturator_mix.store(std::stof(sm));
        auto sdr = get_val("saturator_drive");
        if (!sdr.empty()) g_saturator_drive.store(std::stof(sdr));
        auto lb = get_val("led_brightness");
        if (!lb.empty()) g_led_brightness.store(std::clamp(std::stof(lb), 0.0f, 1.0f));
        auto lnm = get_val("led_night_mode");
        if (!lnm.empty()) g_led_night_mode.store(lnm == "1" || lnm == "true");
        auto th = get_val("theme");
        if (!th.empty()) {
            for (int i = 0; i < NUM_THEMES; i++) {
                if (th == THEME_IDS[i]) { g_theme.store(i); break; }
            }
        }

        save_siren_config();
        return true;
    };

    cb.wifi_scan = []() -> std::string {
        // Run iwlist scan and parse results
        FILE* pipe = popen("sudo iwlist wlan0 scan 2>/dev/null | "
                           "grep -E 'ESSID:|Signal level' | "
                           "paste - - | head -40", "r");
        if (!pipe) return "{\"networks\":[]}";

        // Deduplicate SSIDs — keep strongest signal per SSID
        std::map<std::string, int> seen;  // ssid → best signal
        char line[512];
        while (fgets(line, sizeof(line), pipe)) {
            char* essid = strstr(line, "ESSID:\"");
            char* signal = strstr(line, "Signal level=");
            if (essid && signal) {
                essid += 7;
                char* end = strchr(essid, '"');
                if (end) *end = '\0';

                signal += 13;
                int level = atoi(signal);

                std::string ssid_str(essid);
                if (ssid_str.empty()) continue;  // skip hidden networks
                auto it = seen.find(ssid_str);
                if (it == seen.end() || level > it->second)
                    seen[ssid_str] = level;
            }
        }
        pclose(pipe);

        std::string json = "{\"networks\":[";
        bool first = true;
        for (const auto& kv : seen) {
            if (!first) json += ",";
            first = false;
            json += "{\"ssid\":\"" + kv.first + "\","
                    "\"signal\":" + std::to_string(kv.second) + "}";
        }
        json += "]}";
        return json;
    };

    cb.wifi_connect = [](const std::string& ssid, const std::string& password) -> bool {
        // Use nmcli to save credentials. This creates or updates the
        // NetworkManager connection profile (no duplicates).
        char cmd[512];

        // Delete any existing profile for this SSID to avoid duplicates
        snprintf(cmd, sizeof(cmd),
            "sudo nmcli connection delete id \"%s\" 2>/dev/null",
            ssid.c_str());
        system(cmd);

        // Create new connection profile (saved to disk, available on next boot)
        if (password.empty()) {
            snprintf(cmd, sizeof(cmd),
                "sudo nmcli connection add type wifi ifname wlan0 con-name \"%s\" "
                "ssid \"%s\" -- wifi-sec.key-mgmt none",
                ssid.c_str(), ssid.c_str());
        } else {
            snprintf(cmd, sizeof(cmd),
                "sudo nmcli connection add type wifi ifname wlan0 con-name \"%s\" "
                "ssid \"%s\" -- wifi-sec.key-mgmt wpa-psk wifi-sec.psk \"%s\"",
                ssid.c_str(), ssid.c_str(), password.c_str());
        }
        int ret = system(cmd);
        slog("WIFI: Saved credentials for '%s' (exit %d)", ssid.c_str(), ret);
        return ret == 0;
    };

    cb.wifi_status = []() -> std::string {
        FILE* pipe = popen("iwgetid -r 2>/dev/null", "r");
        std::string ssid;
        if (pipe) {
            char buf[128];
            if (fgets(buf, sizeof(buf), pipe)) {
                ssid = buf;
                while (!ssid.empty() && (ssid.back() == '\n' || ssid.back() == '\r'))
                    ssid.pop_back();
            }
            pclose(pipe);
        }
        return "{\"connected\":" + std::string(ssid.empty() ? "false" : "true") +
               ",\"ssid\":\"" + ssid + "\"}";
    };

    // ── WiFi connection test ────────────────────────────────────────
    //
    // In concurrent mode (uap0): wlan0 is free — test directly without
    // touching the AP.  User stays connected to the config portal.
    //
    // In fallback mode (wlan0): must cycle the AP to free wlan0 for
    // the test.  User gets disconnected and reconnects after.

    static std::atomic<int>  wifi_test_state{0};  // 0=idle 1=running 2=done
    static std::string       wifi_test_result_msg;
    static bool              wifi_test_success{false};
    static std::string       wifi_test_ssid;
    static std::mutex        wifi_test_mtx;

    cb.wifi_test = [&](const std::string& ssid, const std::string& password) -> bool {
        if (wifi_test_state.load() == 1) return false;  // already running

        // Save credentials via nmcli (same as wifi_connect)
        cb.wifi_connect(ssid, password);

        wifi_test_state.store(1);
        {
            std::lock_guard<std::mutex> lk(wifi_test_mtx);
            wifi_test_ssid = ssid;
            wifi_test_success = false;
            wifi_test_result_msg = "testing";
        }

        bool concurrent = ap_mode::is_concurrent();
        slog("WIFI TEST: Starting connection test for '%s' (concurrent=%s)",
             ssid.c_str(), concurrent ? "yes" : "no");

        std::thread([ssid, concurrent]() {
            char cmd[512];

            // Remember current SSID so we can reconnect on failure
            std::string prev_ssid;
            if (concurrent) {
                FILE* pipe = popen("iwgetid -r wlan0 2>/dev/null", "r");
                if (pipe) {
                    char buf[128] = {};
                    if (fgets(buf, sizeof(buf), pipe)) {
                        prev_ssid = buf;
                        while (!prev_ssid.empty() &&
                               (prev_ssid.back() == '\n' || prev_ssid.back() == '\r'))
                            prev_ssid.pop_back();
                    }
                    pclose(pipe);
                }
                slog("WIFI TEST: Currently on '%s' — disconnecting for test",
                     prev_ssid.c_str());
                system("sudo nmcli device disconnect wlan0 2>/dev/null");
                usleep(1000000);
            }

            if (!concurrent) {
                // Must cycle AP to free wlan0
                slog("WIFI TEST: Stopping AP for connection test");
                ap_mode::stop_ap();
                system("sudo nmcli device set wlan0 managed yes 2>/dev/null");
                usleep(2000000);
            }

            // Tell NetworkManager to activate this specific connection
            snprintf(cmd, sizeof(cmd),
                "sudo nmcli connection up \"%s\" 2>/dev/null", ssid.c_str());
            int ret = system(cmd);
            slog("WIFI TEST: 'nmcli connection up' exit %d", ret);

            // Poll for connection (up to 15 seconds)
            // Verify the SSID matches what we're testing (not a stale connection)
            bool connected = false;
            for (int i = 0; i < 15; i++) {
                usleep(1000000);

                FILE* pipe = popen("iwgetid -r wlan0 2>/dev/null", "r");
                if (pipe) {
                    char buf[128] = {};
                    if (fgets(buf, sizeof(buf), pipe)) {
                        std::string got(buf);
                        while (!got.empty() && (got.back() == '\n' || got.back() == '\r'))
                            got.pop_back();
                        if (!got.empty() && got == ssid) {
                            slog("WIFI TEST: Connected to '%s' after %ds", got.c_str(), i + 1);

                            // Wait a moment for DHCP to assign IP
                            usleep(2000000);

                            // Grab the wlan0 IP
                            FILE* ip_pipe = popen("ip -4 addr show wlan0 2>/dev/null | "
                                                   "awk '/inet /{print $2}' | cut -d/ -f1", "r");
                            std::string ip;
                            if (ip_pipe) {
                                char ipbuf[64] = {};
                                if (fgets(ipbuf, sizeof(ipbuf), ip_pipe)) {
                                    ip = ipbuf;
                                    while (!ip.empty() && (ip.back() == '\n' || ip.back() == '\r'))
                                        ip.pop_back();
                                }
                                pclose(ip_pipe);
                            }

                            if (ip.empty()) {
                                slog("WIFI TEST: Associated but no IP — DHCP failed");
                                pclose(pipe);
                                break;  // fall through to failure
                            }

                            connected = true;

                            // Get hostname for .local address
                            std::string hostname;
                            FILE* hn_pipe = popen("hostname 2>/dev/null", "r");
                            if (hn_pipe) {
                                char hnbuf[128] = {};
                                if (fgets(hnbuf, sizeof(hnbuf), hn_pipe)) {
                                    hostname = hnbuf;
                                    while (!hostname.empty() &&
                                           (hostname.back() == '\n' || hostname.back() == '\r'))
                                        hostname.pop_back();
                                }
                                pclose(hn_pipe);
                            }

                            std::string addr_info;
                            if (!hostname.empty())
                                addr_info = hostname + ".local";
                            if (!ip.empty()) {
                                if (!addr_info.empty()) addr_info += " / ";
                                addr_info += ip;
                            }

                            {
                                std::lock_guard<std::mutex> lk(wifi_test_mtx);
                                wifi_test_success = true;
                                wifi_test_result_msg = "Connected to '" + got + "'"
                                    + (addr_info.empty() ? "" : " — reach siren at " + addr_info);
                            }
                            pclose(pipe);
                            break;
                        } else if (!got.empty()) {
                            slog("WIFI TEST: wlan0 on '%s' (wanted '%s') — ignoring",
                                 got.c_str(), ssid.c_str());
                        }
                    }
                    pclose(pipe);
                }
            }

            if (!connected) {
                slog("WIFI TEST: Failed to connect to '%s' within 15s", ssid.c_str());
                // Delete the bad connection profile so it doesn't interfere
                snprintf(cmd, sizeof(cmd),
                    "sudo nmcli connection delete id \"%s\" 2>/dev/null", ssid.c_str());
                system(cmd);
                std::lock_guard<std::mutex> lk(wifi_test_mtx);
                wifi_test_success = false;
                wifi_test_result_msg = "Failed to connect to '" + ssid
                    + "' — check password and try again";
            }

            // Restore connectivity
            if (!concurrent) {
                slog("WIFI TEST: Restarting AP");
                usleep(1000000);
                system("sudo nmcli device disconnect wlan0 2>/dev/null");
                usleep(500000);
                ap_mode::start_ap();
            } else if (!connected && !prev_ssid.empty()) {
                // Reconnect to previous network after failed test
                slog("WIFI TEST: Reconnecting to '%s'", prev_ssid.c_str());
                snprintf(cmd, sizeof(cmd),
                    "sudo nmcli connection up \"%s\" 2>/dev/null", prev_ssid.c_str());
                system(cmd);
            }

            slog("WIFI TEST: Complete (success=%s)", wifi_test_success ? "yes" : "no");
            wifi_test_state.store(2);
        }).detach();

        return true;
    };

    cb.wifi_test_result = []() -> std::string {
        int state = wifi_test_state.load();
        std::lock_guard<std::mutex> lk(wifi_test_mtx);
        std::string status;
        switch (state) {
        case 0: status = "idle"; break;
        case 1: status = "running"; break;
        case 2: status = "done"; break;
        }
        // Escape quotes in result message
        std::string escaped;
        for (char c : wifi_test_result_msg) {
            if (c == '"') escaped += "\\\"";
            else if (c == '\\') escaped += "\\\\";
            else escaped += c;
        }
        return "{\"state\":\"" + status + "\","
               "\"success\":" + (wifi_test_success ? "true" : "false") + ","
               "\"concurrent\":" + (ap_mode::is_concurrent() ? "true" : "false") + ","
               "\"ssid\":\"" + wifi_test_ssid + "\","
               "\"message\":\"" + escaped + "\"}";
    };

    // ── Repo root (derived from executable path) ─────────────────────
    // /home/<user>/dubsiren/build/dubsiren → /home/<user>/dubsiren
    static std::string repo_root;
    {
        char exe[PATH_MAX] = {};
        ssize_t len = readlink("/proc/self/exe", exe, sizeof(exe) - 1);
        if (len > 0) {
            exe[len] = '\0';
            char* slash = strrchr(exe, '/');  // strip binary name
            if (slash) *slash = '\0';
            slash = strrchr(exe, '/');        // strip "build"
            if (slash) *slash = '\0';
            repo_root = exe;
        }
        if (repo_root.empty()) repo_root = "/home/pi/dubsiren";  // fallback

        // On overlay FS the binary may run from /mnt/persist/... but the
        // working git repo is in the overlay at the normal path.  Prefer
        // the normal path if it has a .git directory.
        if (repo_root.find("/mnt/persist") == 0) {
            std::string normal = repo_root.substr(strlen("/mnt/persist"));
            struct stat st;
            if (stat((normal + "/.git").c_str(), &st) == 0) {
                slog("UPDATE: Using overlay repo %s (not %s)", normal.c_str(), repo_root.c_str());
                repo_root = normal;
            }
        }
        slog("UPDATE: repo_root = %s", repo_root.c_str());
    }

    // Git command prefix — passes safe.directory on every invocation so it
    // works regardless of HOME, gitconfig, or which user owns the repo.
    // This avoids the "dubious ownership" error when running as root.
    static const std::string git_cmd =
        "git -c safe.directory='" + repo_root + "' ";

    // ── Update: progress tracking state ─────────────────────────────
    // Shared between the background update thread and the status endpoint.
    static std::mutex              upd_mtx;
    static std::string             upd_stage;     // "idle","fetch","pull","build","deploy","done","error"
    static int                     upd_pct;       // 0–100
    static std::string             upd_error;
    static std::string             upd_log;       // verbose command output
    static std::atomic<bool>       upd_running{false};

    auto set_upd = [](const std::string& stage, int pct, const std::string& err = "") {
        std::lock_guard<std::mutex> lk(upd_mtx);
        upd_stage = stage;
        upd_pct   = pct;
        upd_error = err;
    };

    // Append a line to the verbose log (thread-safe)
    auto log_append = [](const std::string& line) {
        std::lock_guard<std::mutex> lk(upd_mtx);
        upd_log += line;
    };

    set_upd("idle", 0);

    // Sanitize branch names to prevent command injection via popen/system
    auto sanitize_branch = [](const std::string& branch) -> std::string {
        std::string safe;
        for (char c : branch) {
            if (std::isalnum(c) || c == '-' || c == '_' || c == '.' || c == '/') safe += c;
        }
        return safe;
    };

    cb.update_branches = []() -> std::string {
        // Fetch all remotes first
        system(("cd " + repo_root + " && " + git_cmd + " fetch --all 2>/dev/null").c_str());
        FILE* pipe = popen(("cd " + repo_root + " && " +
                           git_cmd + " branch -r --format='%(refname:lstrip=3)' 2>/dev/null").c_str(), "r");
        if (!pipe) return "{\"branches\":[\"main\"]}";

        std::string json = "{\"branches\":[";
        char line[256];
        bool first = true;
        while (fgets(line, sizeof(line), pipe)) {
            // Strip newline
            size_t len = strlen(line);
            if (len > 0 && line[len - 1] == '\n') line[len - 1] = '\0';
            // Skip HEAD pointer
            if (strncmp(line, "HEAD", 4) == 0) continue;
            if (strlen(line) == 0) continue;
            if (!first) json += ",";
            json += "\"" + std::string(line) + "\"";
            first = false;
        }
        pclose(pipe);
        json += "]}";
        return json;
    };

    cb.update_check = [sanitize_branch](const std::string& raw_branch) -> std::string {
        std::string branch = sanitize_branch(raw_branch);
        slog("UPDATE: Checking branch '%s' in %s", branch.c_str(), repo_root.c_str());

        // Log current HEAD
        {
            FILE* p = popen(("cd " + repo_root + " && " + git_cmd + "rev-parse --short HEAD 2>/dev/null").c_str(), "r");
            char h[64] = {};
            if (p) { fgets(h, sizeof(h), p); pclose(p); }
            size_t hl = strlen(h); if (hl > 0 && h[hl-1] == '\n') h[hl-1] = '\0';

            FILE* p2 = popen(("cd " + repo_root + " && " + git_cmd + "rev-parse --abbrev-ref HEAD 2>/dev/null").c_str(), "r");
            char br[128] = {};
            if (p2) { fgets(br, sizeof(br), p2); pclose(p2); }
            size_t bl = strlen(br); if (bl > 0 && br[bl-1] == '\n') br[bl-1] = '\0';

            slog("UPDATE: Local HEAD = %s (%s)", h, br);
        }

        std::string cmd = "cd " + repo_root + " && " + git_cmd + "fetch origin "
                          + branch + " 2>&1";
        FILE* fp = popen(cmd.c_str(), "r");
        std::string fetch_out;
        if (fp) {
            char buf[256];
            while (fgets(buf, sizeof(buf), fp)) fetch_out += buf;
            pclose(fp);
        }
        if (!fetch_out.empty()) slog("UPDATE: fetch output: %s", fetch_out.c_str());

        // Log remote ref after fetch
        {
            std::string rc = "cd " + repo_root + " && " + git_cmd + "rev-parse --short origin/" + branch + " 2>/dev/null";
            FILE* p = popen(rc.c_str(), "r");
            char h[64] = {};
            if (p) { fgets(h, sizeof(h), p); pclose(p); }
            size_t hl = strlen(h); if (hl > 0 && h[hl-1] == '\n') h[hl-1] = '\0';
            slog("UPDATE: Remote origin/%s = %s", branch.c_str(), h);
        }

        std::string log_cmd = "cd " + repo_root + " && "
                              + git_cmd + "log HEAD..origin/" + branch +
                              " --oneline 2>/dev/null";
        FILE* pipe = popen(log_cmd.c_str(), "r");
        if (!pipe)
            return "{\"available\":false,\"error\":\"git error\"}";

        char line[256];
        int count = 0;
        std::string commits;
        while (fgets(line, sizeof(line), pipe)) {
            count++;
            if (count <= 5) commits += line;  // keep first 5 for log
        }
        pclose(pipe);

        slog("UPDATE: %d new commit(s) on origin/%s", count, branch.c_str());

        // Get date of latest commit on the remote branch
        std::string date_str;
        {
            std::string dc = "cd " + repo_root + " && " + git_cmd + "log -1 --format=%ci origin/" + branch + " 2>/dev/null";
            FILE* dp = popen(dc.c_str(), "r");
            char dbuf[64] = {};
            if (dp) { fgets(dbuf, sizeof(dbuf), dp); pclose(dp); }
            size_t dl = strlen(dbuf); if (dl > 0 && dbuf[dl-1] == '\n') dbuf[dl-1] = '\0';
            date_str = dbuf;
        }

        return "{\"available\":" + std::string(count > 0 ? "true" : "false") +
               ",\"count\":" + std::to_string(count) +
               ",\"latest_date\":\"" + date_str + "\"}";
    };

    cb.update_install = [set_upd, log_append, sanitize_branch](const std::string& raw_branch) -> bool {
        std::string branch = sanitize_branch(raw_branch);
        // Reject if already running
        bool expected = false;
        if (!upd_running.compare_exchange_strong(expected, true))
            return false;

        // Clear log for new install
        {
            std::lock_guard<std::mutex> lk(upd_mtx);
            upd_log.clear();
        }

        // Launch background thread so the HTTP response returns immediately
        std::thread([set_upd, log_append, branch]() {
            // Run a command, capture output into the verbose log, return exit code
            auto run = [&log_append](const std::string& cmd) -> int {
                log_append("$ " + cmd + "\n");
                FILE* pipe = popen((cmd + " 2>&1").c_str(), "r");
                if (!pipe) {
                    log_append("[error: popen failed]\n");
                    return -1;
                }
                char buf[512];
                while (fgets(buf, sizeof(buf), pipe)) {
                    log_append(std::string(buf));
                }
                int status = pclose(pipe);
                int code = WIFEXITED(status) ? WEXITSTATUS(status) : -1;
                if (code != 0)
                    log_append("[exit code: " + std::to_string(code) + "]\n");
                return code;
            };

            // Stage 0: Back up user data (5%)
            // On kiosk (overlay FS) devices, data/ is bind-mounted from
            // persistent storage.  Copy settings to a safe location in case
            // the deploy overwrites the data dir structure.
            set_upd("backup", 5);
            run("cp -a " + repo_root + "/data /tmp/dubsiren-data-backup 2>/dev/null");

            // Stage 1: Fetch (15%)
            set_upd("fetch", 15);
            if (run("cd " + repo_root + " && " + git_cmd + "fetch origin " + branch) != 0) {
                set_upd("error", 0, "git fetch failed");
                upd_running.store(false);
                return;
            }

            // Stage 2: Pull / checkout (25%)
            // Use git reset to avoid merge conflicts — user shouldn't have
            // local source changes on the Pi.  The data/ dir is bind-mounted
            // separately so it won't be affected by git operations.
            set_upd("pull", 25);
            if (run("cd " + repo_root + " && " + git_cmd + "checkout " + branch) != 0) {
                set_upd("error", 0, "git checkout failed");
                upd_running.store(false);
                return;
            }
            if (run("cd " + repo_root + " && " + git_cmd + "reset --hard origin/" + branch) != 0) {
                set_upd("error", 0, "git reset failed");
                upd_running.store(false);
                return;
            }

            // Stage 3: Build — cmake (40%)
            // Clear CMakeCache to avoid stale NOTFOUND entries (e.g. ws2811
            // headers found in a new location after library reinstall).
            set_upd("build", 40);
            run("rm -f " + repo_root + "/build/CMakeCache.txt");
            if (run("mkdir -p " + repo_root + "/build && cd " + repo_root + "/build && cmake ..") != 0) {
                set_upd("error", 0, "cmake failed");
                upd_running.store(false);
                return;
            }

            // Stage 3b: Build — make (40→85%)
            set_upd("build", 55);
            if (run("cd " + repo_root + "/build && make -j1") != 0) {
                set_upd("error", 0, "build failed");
                upd_running.store(false);
                return;
            }
            set_upd("build", 85);

            // Stage 4: Deploy to persistent storage (88%)
            // deploy-to-persist.sh copies binary + syncs git repo to real disk
            // so updates survive reboot on kiosk/overlay FS devices.
            set_upd("deploy", 88);
            if (run("cd " + repo_root + " && sudo ./scripts/deploy-to-persist.sh") != 0) {
                set_upd("error", 0, "deploy failed");
                upd_running.store(false);
                return;
            }

            // Stage 4b: Update systemd service file (92%)
            // Ensures fixes to User=, paths, capabilities are applied
            // without needing a full reinstall from setup.sh.
            set_upd("deploy", 92);
            run("cd " + repo_root + " && sudo ./scripts/update-service.sh " + repo_root);

            // Stage 5: Restore user data if needed (95%)
            // If the data dir lost its bind mount or files were overwritten,
            // restore from backup.  The bind mount makes this unlikely, but
            // this is a safety net.
            set_upd("deploy", 95);
            run("if [ ! -f " + repo_root + "/data/presets/user_presets.txt ] && "
                "[ -f /tmp/dubsiren-data-backup/presets/user_presets.txt ]; then "
                "cp -a /tmp/dubsiren-data-backup/* " + repo_root + "/data/ 2>/dev/null; fi");
            run("rm -rf /tmp/dubsiren-data-backup 2>/dev/null");

            // Done (100%)
            set_upd("done", 100);
            upd_running.store(false);
        }).detach();

        return true;
    };

    cb.update_status = []() -> std::string {
        std::lock_guard<std::mutex> lk(upd_mtx);
        std::string json = "{\"stage\":\"" + upd_stage + "\""
                           ",\"progress\":" + std::to_string(upd_pct);
        if (!upd_error.empty())
            json += ",\"error\":\"" + upd_error + "\"";
        json += "}";
        return json;
    };

    cb.update_log = []() -> std::string {
        std::lock_guard<std::mutex> lk(upd_mtx);
        // JSON-escape the log text
        std::string escaped;
        escaped.reserve(upd_log.size() + 64);
        for (char c : upd_log) {
            switch (c) {
                case '"':  escaped += "\\\""; break;
                case '\\': escaped += "\\\\"; break;
                case '\n': escaped += "\\n";  break;
                case '\r': escaped += "\\r";  break;
                case '\t': escaped += "\\t";  break;
                default:
                    if (static_cast<unsigned char>(c) < 0x20)
                        escaped += ' ';
                    else
                        escaped += c;
            }
        }
        return "{\"log\":\"" + escaped + "\"}";
    };

    cb.backup_create = []() -> std::string {
        std::string json = "{\"version\":\"DUBSIREN_BACKUP_V1\",\"presets\":[";
        for (int i = 0; i < NUM_USER_PRESETS; i++) {
            const auto& u = g_user_presets[i];
            if (i > 0) json += ",";
            char buf[512];
            snprintf(buf, sizeof(buf),
                "{\"name\":\"%s\",\"saved\":%s,\"waveform\":%d,\"lfo_wave\":%d,"
                "\"freq\":%.6f,\"lfo_rate\":%.6f,\"lfo_depth\":%.6f,"
                "\"filter_cutoff\":%.6f,\"filter_reso\":%.6f,"
                "\"delay_time\":%.6f,\"delay_feedback\":%.6f,\"delay_mix\":%.6f,"
                "\"reverb_mix\":%.6f,\"release_time\":%.6f,\"sweep_dir\":%.6f,"
                "\"pitch_env\":%d,\"super_drip\":%s,"
                "\"reverb_type\":%d,\"delay_type\":%d,"
                "\"tape_wobble\":%.6f,\"tape_flutter\":%.6f}",
                u.name, u.saved ? "true" : "false",
                u.waveform, u.lfo_wave,
                u.freq, u.lfo_rate, u.lfo_depth,
                u.filter_cutoff, u.filter_reso,
                u.delay_time, u.delay_feedback, u.delay_mix,
                u.reverb_mix, u.release_time, u.sweep_dir,
                u.pitch_env, u.super_drip ? "true" : "false",
                u.reverb_type, u.delay_type,
                u.tape_wobble, u.tape_flutter);
            json += buf;
        }
        json += "],\"config\":{";
        json += "\"reverb_type\":" + std::to_string(g_reverb_type.load()) +
            ",\"delay_type\":" + std::to_string(g_delay_type.load()) +
            ",\"tape_wobble\":" + std::to_string(g_tape_wobble.load()) +
            ",\"tape_flutter\":" + std::to_string(g_tape_flutter.load()) +
            ",\"fx_chain\":" + std::to_string(g_fx_chain.load()) +
            ",\"lfo_pitch_link\":" + (g_lfo_pitch_link.load() ? "true" : "false") +
            ",\"super_drip\":" + (g_super_drip.load() ? "true" : "false") +
            ",\"phaser_mix\":" + std::to_string(g_phaser_mix.load()) +
            ",\"chorus_mix\":" + std::to_string(g_chorus_mix.load()) +
            ",\"flanger_mix\":" + std::to_string(g_flanger_mix.load()) +
            ",\"saturator_mix\":" + std::to_string(g_saturator_mix.load()) +
            ",\"saturator_drive\":" + std::to_string(g_saturator_drive.load()) +
            ",\"theme\":\"" + THEME_IDS[std::clamp(g_theme.load(), 0, NUM_THEMES - 1)] + "\"";
        json += "}}";
        return json;
    };

    cb.backup_restore = [](const std::string& json) -> bool {
        // Minimal JSON helpers
        auto json_int = [&](const std::string& src, const char* key) -> int {
            std::string needle = std::string("\"") + key + "\":";
            auto pos = src.find(needle);
            if (pos == std::string::npos) return -999;
            return atoi(src.c_str() + pos + needle.size());
        };
        auto json_float = [&](const std::string& src, const char* key) -> float {
            std::string needle = std::string("\"") + key + "\":";
            auto pos = src.find(needle);
            if (pos == std::string::npos) return -999.0f;
            return static_cast<float>(atof(src.c_str() + pos + needle.size()));
        };
        auto json_bool = [&](const std::string& src, const char* key) -> bool {
            std::string needle = std::string("\"") + key + "\":";
            auto pos = src.find(needle);
            if (pos == std::string::npos) return false;
            auto vs = pos + needle.size();
            while (vs < src.size() && src[vs] == ' ') vs++;
            return src[vs] == 't';
        };

        // Verify backup version
        if (json.find("\"DUBSIREN_BACKUP_V1\"") == std::string::npos) {
            fprintf(stderr, "  !!! Invalid backup format\n");
            return false;
        }

        // Restore config section
        auto cfg_start = json.find("\"config\":{");
        if (cfg_start != std::string::npos) {
            auto cfg_end = json.find('}', cfg_start + 10);
            if (cfg_end != std::string::npos) {
                std::string cfg = json.substr(cfg_start, cfg_end - cfg_start + 1);
                int rv = json_int(cfg, "reverb_type");
                if (rv != -999) g_reverb_type.store(rv);
                int dv = json_int(cfg, "delay_type");
                if (dv != -999) g_delay_type.store(dv);
                float tw = json_float(cfg, "tape_wobble");
                if (tw != -999.0f) g_tape_wobble.store(tw);
                float tf = json_float(cfg, "tape_flutter");
                if (tf != -999.0f) g_tape_flutter.store(tf);
                int fc = json_int(cfg, "fx_chain");
                if (fc != -999) g_fx_chain.store(fc);
                g_lfo_pitch_link.store(json_bool(cfg, "lfo_pitch_link"));
                g_super_drip.store(json_bool(cfg, "super_drip"));
                float pmix = json_float(cfg, "phaser_mix");
                if (pmix != -999.0f) g_phaser_mix.store(pmix);
                else g_phaser_mix.store(json_bool(cfg, "phaser") ? 0.5f : 0.0f);
                float cmix = json_float(cfg, "chorus_mix");
                if (cmix != -999.0f) g_chorus_mix.store(cmix);
                else g_chorus_mix.store(json_bool(cfg, "chorus") ? 0.5f : 0.0f);
                float fmix = json_float(cfg, "flanger_mix");
                if (fmix != -999.0f) g_flanger_mix.store(fmix);
                else g_flanger_mix.store(json_bool(cfg, "flanger") ? 0.5f : 0.0f);
                float smix = json_float(cfg, "saturator_mix");
                if (smix != -999.0f) g_saturator_mix.store(smix);
                float sdrive = json_float(cfg, "saturator_drive");
                if (sdrive != -999.0f) g_saturator_drive.store(sdrive);
                // Restore theme
                auto tpos = cfg.find("\"theme\":\"");
                if (tpos != std::string::npos) {
                    auto ts = tpos + 9;
                    auto te = cfg.find('"', ts);
                    if (te != std::string::npos) {
                        std::string tid = cfg.substr(ts, te - ts);
                        for (int i = 0; i < NUM_THEMES; i++) {
                            if (tid == THEME_IDS[i]) { g_theme.store(i); break; }
                        }
                    }
                }
            }
        }

        // Restore presets
        auto presets_pos = json.find("\"presets\":[");
        if (presets_pos != std::string::npos) {
            size_t pos = presets_pos + 11;
            for (int i = 0; i < NUM_USER_PRESETS && pos < json.size(); i++) {
                auto obj_start = json.find('{', pos);
                if (obj_start == std::string::npos) break;
                auto obj_end = json.find('}', obj_start);
                if (obj_end == std::string::npos) break;
                std::string obj = json.substr(obj_start, obj_end - obj_start + 1);

                // Extract name
                auto name_pos = obj.find("\"name\":\"");
                if (name_pos != std::string::npos) {
                    auto ns = name_pos + 8;
                    auto ne = obj.find('"', ns);
                    if (ne != std::string::npos)
                        snprintf(g_user_presets[i].name,
                                 sizeof(g_user_presets[i].name),
                                 "%s", obj.substr(ns, ne - ns).c_str());
                }

                g_user_presets[i].saved          = json_bool(obj, "saved");
                g_user_presets[i].waveform       = json_int(obj, "waveform");
                g_user_presets[i].lfo_wave       = json_int(obj, "lfo_wave");
                g_user_presets[i].freq           = json_float(obj, "freq");
                g_user_presets[i].lfo_rate       = json_float(obj, "lfo_rate");
                g_user_presets[i].lfo_depth      = json_float(obj, "lfo_depth");
                g_user_presets[i].filter_cutoff  = json_float(obj, "filter_cutoff");
                g_user_presets[i].filter_reso    = json_float(obj, "filter_reso");
                g_user_presets[i].delay_time     = json_float(obj, "delay_time");
                g_user_presets[i].delay_feedback = json_float(obj, "delay_feedback");
                g_user_presets[i].delay_mix      = json_float(obj, "delay_mix");
                g_user_presets[i].reverb_mix     = json_float(obj, "reverb_mix");
                g_user_presets[i].release_time   = json_float(obj, "release_time");
                g_user_presets[i].sweep_dir      = json_float(obj, "sweep_dir");
                g_user_presets[i].pitch_env      = json_int(obj, "pitch_env");
                g_user_presets[i].super_drip     = json_bool(obj, "super_drip");
                g_user_presets[i].reverb_type    = json_int(obj, "reverb_type");
                g_user_presets[i].delay_type     = json_int(obj, "delay_type");
                g_user_presets[i].tape_wobble    = json_float(obj, "tape_wobble");
                g_user_presets[i].tape_flutter   = json_float(obj, "tape_flutter");

                pos = obj_end + 1;
            }
        }

        save_siren_config();
        save_user_presets();
        printf("  Backup restored successfully\n");
        return true;
    };

    // ── Live DSP state ─────────────────────────────────────────────
    cb.get_dsp_state = []() -> std::string {
        return "{\"freq\":" + std::to_string(g_freq.load()) +
            ",\"lfo_rate\":" + std::to_string(g_lfo_rate.load()) +
            ",\"lfo_depth\":" + std::to_string(g_lfo_depth.load()) +
            ",\"filter_cutoff\":" + std::to_string(g_filter_cutoff.load()) +
            ",\"filter_reso\":" + std::to_string(g_filter_reso.load()) +
            ",\"delay_time\":" + std::to_string(g_delay_time.load()) +
            ",\"delay_feedback\":" + std::to_string(g_delay_feedback.load()) +
            ",\"delay_mix\":" + std::to_string(g_delay_mix.load()) +
            ",\"reverb_mix\":" + std::to_string(g_reverb_mix.load()) +
            ",\"release_time\":" + std::to_string(g_release_time.load()) +
            ",\"waveform\":" + std::to_string(g_waveform.load()) +
            ",\"lfo_waveform\":" + std::to_string(g_lfo_waveform.load()) +
            ",\"pitch_env\":" + std::to_string(g_pitch_env.load()) +
            ",\"sweep_dir\":" + std::to_string(g_sweep_dir.load()) +
            ",\"reverb_type\":" + std::to_string(g_reverb_type.load()) +
            ",\"delay_type\":" + std::to_string(g_delay_type.load()) +
            ",\"tape_wobble\":" + std::to_string(g_tape_wobble.load()) +
            ",\"tape_flutter\":" + std::to_string(g_tape_flutter.load()) +
            ",\"fx_chain\":" + std::to_string(g_fx_chain.load()) +
            ",\"lfo_pitch_link\":" + (g_lfo_pitch_link.load() ? "true" : "false") +
            ",\"super_drip\":" + (g_super_drip.load() ? "true" : "false") +
            ",\"phaser_mix\":" + std::to_string(g_phaser_mix.load()) +
            ",\"chorus_mix\":" + std::to_string(g_chorus_mix.load()) +
            ",\"flanger_mix\":" + std::to_string(g_flanger_mix.load()) +
            ",\"saturator_mix\":" + std::to_string(g_saturator_mix.load()) +
            ",\"saturator_drive\":" + std::to_string(g_saturator_drive.load()) +
            ",\"active_bank\":" + std::to_string(g_bank_mode.load()) +
            ",\"active_preset\":" + std::to_string(g_preset.load()) + "}";
    };

    cb.set_dsp_param = [](const std::string& name, float value) -> bool {
        auto* atom = param_atomic_for(name.c_str());
        if (atom) {
            int idx = find_param_index(name.c_str());
            if (idx >= 0) {
                value = std::clamp(value, PARAM_TABLE[idx].min_val,
                                          PARAM_TABLE[idx].max_val);
            }
            atom->store(value);
            if (name == "freq" || name == "lfo_rate" || name == "delay_time")
                update_link_eff();
            return true;
        }
        // Non-knob parameters
        if (name == "waveform") {
            g_waveform.store(std::clamp(static_cast<int>(value), 0, 3));
            return true;
        }
        if (name == "lfo_waveform") {
            g_lfo_waveform.store(std::clamp(static_cast<int>(value), 0, 7));
            return true;
        }
        if (name == "pitch_env") {
            g_pitch_env.store(std::clamp(static_cast<int>(value), -1, 1));
            return true;
        }
        if (name == "sweep_dir") {
            g_sweep_dir.store(std::clamp(value, -1.0f, 1.0f));
            return true;
        }
        if (name == "reverb_type") {
            g_reverb_type.store(std::clamp(static_cast<int>(value), 0, 3));
            return true;
        }
        if (name == "delay_type") {
            g_delay_type.store(std::clamp(static_cast<int>(value), 0, 1));
            return true;
        }
        if (name == "tape_wobble") {
            g_tape_wobble.store(std::clamp(value, 0.0f, 1.0f)); return true;
        }
        if (name == "tape_flutter") {
            g_tape_flutter.store(std::clamp(value, 0.0f, 1.0f)); return true;
        }
        if (name == "fx_chain") {
            g_fx_chain.store(std::clamp(static_cast<int>(value), 0, 5)); return true;
        }
        if (name == "lfo_pitch_link") {
            g_lfo_pitch_link.store(value > 0.5f); return true;
        }
        if (name == "super_drip") {
            g_super_drip.store(value > 0.5f); return true;
        }
        if (name == "phaser_mix") {
            g_phaser_mix.store(std::clamp(value, 0.0f, 1.0f)); return true;
        }
        if (name == "chorus_mix") {
            g_chorus_mix.store(std::clamp(value, 0.0f, 1.0f)); return true;
        }
        if (name == "flanger_mix") {
            g_flanger_mix.store(std::clamp(value, 0.0f, 1.0f)); return true;
        }
        if (name == "saturator_mix") {
            g_saturator_mix.store(std::clamp(value, 0.0f, 1.0f)); return true;
        }
        if (name == "saturator_drive") {
            g_saturator_drive.store(std::clamp(value, 0.0f, 1.0f)); return true;
        }
        return false;
    };

    cb.reset_defaults = []() -> bool {
        g_freq.store(440.0f);
        g_lfo_rate.store(0.35f);
        g_filter_cutoff.store(8000.0f);
        g_delay_time.store(0.375f);
        g_delay_feedback.store(0.55f);
        g_lfo_depth.store(0.35f);
        g_release_time.store(0.050f);
        g_filter_reso.store(0.0f);
        g_delay_mix.store(0.30f);
        g_reverb_mix.store(0.35f);
        g_waveform.store(0);
        g_lfo_waveform.store(0);
        g_pitch_env.store(0);
        g_sweep_dir.store(-1.0f);
        g_reverb_type.store(0);
        g_delay_type.store(0);
        g_tape_wobble.store(1.0f);
        g_tape_flutter.store(1.0f);
        g_fx_chain.store(0);
        g_lfo_pitch_link.store(true);
        g_super_drip.store(true);
        g_phaser_mix.store(0.0f);
        g_chorus_mix.store(0.0f);
        g_flanger_mix.store(0.0f);
        g_saturator_mix.store(0.0f);
        g_saturator_drive.store(0.5f);
        update_link_eff();
        save_siren_config();
        slog("WEB: Reset to factory defaults");
        return true;
    };

    // ── Preview (snapshot/restore) ──────────────────────────────────
    cb.preview_start = [](const std::string& body) {
        // Snapshot current state before preview
        g_preview_snapshot = snapshot_current();
        g_previewing.store(true);

        // Parse index from body (form: category=library&index=N)
        auto get_val = [&](const std::string& key) -> std::string {
            auto pos = body.find(key + "=");
            if (pos == std::string::npos) return "";
            auto start = pos + key.size() + 1;
            auto end = body.find('&', start);
            if (end == std::string::npos) end = body.size();
            return body.substr(start, end - start);
        };
        auto cat = get_val("category");
        auto idx_str = get_val("index");
        int idx = 0;
        try { idx = std::stoi(idx_str); } catch (...) {}

        if (cat == "library" && idx >= 0 && idx < NUM_LIBRARY_PRESETS) {
            apply_dub_preset(PRESET_LIBRARY[idx]);
            slog("WEB: Preview started — library[%d] \"%s\"",
                   idx, PRESET_LIBRARY[idx].name);
        }
    };

    cb.preview_stop = []() {
        if (g_previewing.load()) {
            apply_user_preset(g_preview_snapshot);
            g_previewing.store(false);
            slog("WEB: Preview stopped — restored previous state");
        }
    };

    // ── System info ────────────────────────────────────────────────
    cb.get_system_info = []() -> std::string {
        std::string json = "{";

        // CPU temperature
        FILE* tf = fopen("/sys/class/thermal/thermal_zone0/temp", "r");
        float cpu_temp = 0;
        if (tf) {
            int milli;
            if (fscanf(tf, "%d", &milli) == 1)
                cpu_temp = milli / 1000.0f;
            fclose(tf);
        }

        // Uptime
        FILE* uf = fopen("/proc/uptime", "r");
        float uptime_sec = 0;
        if (uf) {
            fscanf(uf, "%f", &uptime_sec);
            fclose(uf);
        }
        int up_days = static_cast<int>(uptime_sec) / 86400;
        int up_hours = (static_cast<int>(uptime_sec) % 86400) / 3600;
        int up_mins = (static_cast<int>(uptime_sec) % 3600) / 60;

        // Memory
        FILE* mf = fopen("/proc/meminfo", "r");
        long mem_total = 0, mem_avail = 0;
        if (mf) {
            char line[256];
            while (fgets(line, sizeof(line), mf)) {
                if (strncmp(line, "MemTotal:", 9) == 0)
                    sscanf(line + 9, "%ld", &mem_total);
                else if (strncmp(line, "MemAvailable:", 13) == 0)
                    sscanf(line + 13, "%ld", &mem_avail);
            }
            fclose(mf);
        }
        long mem_used = mem_total - mem_avail;

        char buf[512];
        snprintf(buf, sizeof(buf),
            "\"cpu_temp\":%.1f,\"uptime_days\":%d,\"uptime_hours\":%d,"
            "\"uptime_mins\":%d,\"mem_total_mb\":%ld,\"mem_used_mb\":%ld,"
            "\"git_branch\":\"%s\",\"git_commit\":\"%s\"",
            cpu_temp, up_days, up_hours, up_mins,
            mem_total / 1024, mem_used / 1024,
            GIT_BRANCH, GIT_COMMIT);
        json += buf;
        json += "}";
        return json;
    };

    cb.reboot_system = []() -> bool {
        slog("WEB: Reboot requested");
        return system("sudo reboot") == 0;
    };

    cb.restart_service = []() -> bool {
        slog("WEB: Service restart requested");
        return system("sudo systemctl restart dubsiren.service") == 0;
    };

    // ── Encoder mapping ─────────────────────────────────────────────
    cb.get_encoder_map = []() -> std::string {
        std::lock_guard<std::mutex> lk(g_encoder_map_mutex);
        std::string json = "{\"bank_a\":[";
        for (int i = 0; i < 5; i++) {
            if (i > 0) json += ",";
            json += std::to_string(g_encoder_map_a[i]);
        }
        json += "],\"bank_b\":[";
        for (int i = 0; i < 5; i++) {
            if (i > 0) json += ",";
            json += std::to_string(g_encoder_map_b[i]);
        }
        json += "],\"params\":[";
        for (int i = 0; i < NUM_PARAMS; i++) {
            if (i > 0) json += ",";
            json += "{\"name\":\"" + std::string(PARAM_TABLE[i].name) + "\","
                    "\"label\":\"" + std::string(PARAM_TABLE[i].label) + "\"}";
        }
        json += "]}";
        return json;
    };

    cb.set_encoder_map = [](const std::string& body) -> bool {
        // Parse form data: a0=N&a1=N&...&b0=N&b1=N&...
        auto get_val = [&](const std::string& key) -> int {
            auto pos = body.find(key + "=");
            if (pos == std::string::npos) return -1;
            auto start = pos + key.size() + 1;
            auto end = body.find('&', start);
            if (end == std::string::npos) end = body.size();
            return atoi(body.substr(start, end - start).c_str());
        };
        {
            std::lock_guard<std::mutex> lk(g_encoder_map_mutex);
            for (int i = 0; i < 5; i++) {
                int v = get_val("a" + std::to_string(i));
                if (v >= 0 && v < NUM_PARAMS) g_encoder_map_a[i] = v;
                v = get_val("b" + std::to_string(i));
                if (v >= 0 && v < NUM_PARAMS) g_encoder_map_b[i] = v;
            }
        }
        save_siren_config();
        slog("WEB: Encoder mapping updated");
        return true;
    };

    // ── Encoder sensitivity ────────────────────────────────────────
    cb.get_encoder_sensitivity = []() -> std::string {
        std::lock_guard<std::mutex> lk(g_sensitivity_mutex);
        std::string json = "{\"params\":[";
        for (int i = 0; i < NUM_PARAMS; i++) {
            if (i > 0) json += ",";
            char buf[128];
            snprintf(buf, sizeof(buf), "{\"name\":\"%s\",\"label\":\"%s\",\"sensitivity\":%.6f}",
                     PARAM_TABLE[i].name, PARAM_TABLE[i].label, g_param_sensitivity[i]);
            json += buf;
        }
        json += "]}";
        return json;
    };

    cb.set_encoder_sensitivity = [](const std::string& body) -> bool {
        // Parse form body into key=value map
        std::map<std::string, std::string> params;
        std::istringstream stream(body);
        std::string pair;
        while (std::getline(stream, pair, '&')) {
            auto eq = pair.find('=');
            if (eq != std::string::npos)
                params[pair.substr(0, eq)] = pair.substr(eq + 1);
        }
        auto get_val = [&](const std::string& key) -> float {
            auto it = params.find(key);
            if (it == params.end()) return -1.0f;
            return static_cast<float>(atof(it->second.c_str()));
        };
        {
            std::lock_guard<std::mutex> lk(g_sensitivity_mutex);
            for (int i = 0; i < NUM_PARAMS; i++) {
                float v = get_val("s" + std::to_string(i));
                if (v >= 0.25f && v <= 4.0f) g_param_sensitivity[i] = v;
            }
        }
        save_siren_config();
        slog("WEB: Encoder sensitivity updated");
        return true;
    };

    cb.get_system_log = []() -> std::string {
        return siren_log::to_json();
    };

    cb.exit_ap = []() {
        slog("AP: Exit requested via web UI");
        g_ap_exit_requested.store(true);
    };

    cb.is_ap_active = []() -> bool {
        return g_ap_mode.load();
    };

    return cb;
}

static void enter_ap_mode()
{
    if (g_ap_mode.load()) return;

    slog("ENTERING AP CONFIG MODE");

    // Clear gate stuck from 3-button combo hold
    g_gate.store(false);

    // Doppler fly-by LED animation (replaces audio wind-up cue)
    g_led.playApEnter();

    // Audio keeps running so users can preview presets via trigger button
    g_ap_mode.store(true);

    // Start the access point
    if (!ap_mode::start_ap()) {
        slog("!!! Failed to start AP mode");
        g_ap_mode.store(false);
        g_led.setApIdle(false);
        return;
    }

    // Gentle white breathing while AP is active
    g_led.setApIdle(true);

    // Web server is already running (started at boot) — no need to start it here
    slog("AP MODE ACTIVE — Connect to '%s' at %s",
         ap_mode::get_ssid().c_str(), ap_mode::get_ip());
}

static void exit_ap_mode()
{
    if (!g_ap_mode.load()) return;

    slog("EXITING AP CONFIG MODE");

    // Stop AP idle breathing, play exit Doppler
    g_led.setApIdle(false);
    g_led.playApExit();

    // Only stop the AP infrastructure — web server keeps running
    ap_mode::stop_ap();
    g_ap_mode.store(false);
    g_ap_exit_requested.store(false);

    // Audio was never stopped — siren is immediately usable
    slog("SIREN MODE RESTORED");
}

// ─── main ───────────────────────────────────────────────────────────

int main(int argc, char *argv[])
{
    std::string device = "hw:0,0";
    bool simulate = false;

    // ── Parse CLI args ──────────────────────────────────────────────
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--device") == 0) {
            if (i + 1 >= argc) {
                fprintf(stderr, "--device requires an argument\n");
                return 1;
            }
            device = argv[++i];
        } else if (strcmp(argv[i], "--simulate") == 0) {
            simulate = true;
        } else if (strcmp(argv[i], "-h") == 0 ||
                   strcmp(argv[i], "--help") == 0) {
            usage(argv[0]);
            return 0;
        } else {
            fprintf(stderr, "Unknown option: %s\n", argv[i]);
            usage(argv[0]);
            return 1;
        }
    }

    // ── Banner ──────────────────────────────────────────────────────
    slog("Poorhouse Lane - Siren V4 starting (%s @ %s)", GIT_BRANCH, GIT_COMMIT);
    slog("ALSA device: %s  Mode: %s", device.c_str(), simulate ? "SIMULATE" : "GPIO");
    printf("Poorhouse Lane - Siren V4\n");
    printf("Milestone 5: Full DSP Engine + Parameter Mapping\n");
    printf("  ALSA device : %s\n", device.c_str());
    printf("  Mode        : %s\n\n", simulate ? "SIMULATE" : "GPIO");

    signal(SIGINT,  on_signal);
    signal(SIGTERM, on_signal);

    // ── Audio-thread-only DSP state ─────────────────────────────────
    //
    // These objects are captured by reference in the audio callback
    // and are only ever touched from the audio thread.

    Oscillator      osc;
    LFO             lfo;
    MoogFilter      filter;
    TapeDelay       delay;
    DigitalDelay    delay_digital;
    Reverb          reverb;
    PlateReverb     reverb_plate;
    HallReverb      reverb_hall;
    SchroederReverb reverb_schroeder;
    Phaser          phaser;
    Chorus          chorus;
    Flanger         flanger;
    TapeSaturator   saturator;

    float env_level      = 0.0f;
    float attack_rate    = 0.0f;
    float release_rate   = 0.0f;
    float sr_f           = 48000.0f;    // set after init, read in callback

    float pitch_env_mult  = 1.0f;
    float sweep_phase     = 0.0f;   // 0→1 linear ramp during release
    bool  prev_gate       = false;
    bool  pe_active       = false;   // true after trigger release
    float rise_factor     = 1.0f;
    float fall_factor     = 1.0f;

    constexpr int SAMPLE_RATE = 48000;

    AudioEngine audio;

    // ── Audio callback ──────────────────────────────────────────────

    auto audio_cb = [&](float* buf, int frames) {
        const auto rlx = std::memory_order_relaxed;

        // Load all parameters once per frame
        float base_freq = g_freq.load(rlx);
        int   waveform  = g_waveform.load(rlx);
        bool  gate      = g_gate.load(rlx);
        float lfo_r     = g_lfo_rate_eff.load(rlx);
        float lfo_d     = g_lfo_depth.load(rlx);
        float cutoff    = g_filter_cutoff.load(rlx);
        float reso      = g_filter_reso.load(rlx);
        float dly_t     = g_delay_time_eff.load(rlx);
        float dly_fb    = g_delay_feedback.load(rlx);
        float dly_mix   = g_delay_mix.load(rlx);
        float rev_mix   = g_reverb_mix.load(rlx);
        float rel_t     = g_release_time.load(rlx);
        int   pe_mode   = g_pitch_env.load(rlx);
        bool  lfo_link  = g_lfo_pitch_link.load(rlx);
        float sweep_dir = g_sweep_dir.load(rlx);

        // Recompute release rate and pitch-envelope factors from release time.
        // Pitch sweeps exactly 3 octaves over the release duration so the
        // pitch slide and volume fade finish together.
        float rel_samples = rel_t * sr_f;
        release_rate = 1.0f / rel_samples;
        rise_factor  = std::pow(2.0f,  3.0f / rel_samples);  // 3 oct up
        fall_factor  = std::pow(2.0f, -3.0f / rel_samples);  // 3 oct down
        float sweep_inc  = 1.0f / rel_samples;               // linear phase increment

        int   rev_type  = g_reverb_type.load(rlx);
        int   dly_type  = g_delay_type.load(rlx);
        float wobble    = g_tape_wobble.load(rlx);
        float flutter   = g_tape_flutter.load(rlx);
        int   fx_chain  = g_fx_chain.load(rlx);
        float phaser_mix = g_phaser_mix.load(rlx);
        float chorus_mix = g_chorus_mix.load(rlx);
        float flanger_mix = g_flanger_mix.load(rlx);
        float sat_mix   = g_saturator_mix.load(rlx);
        float sat_drive = g_saturator_drive.load(rlx);

        // Update DSP modules (once per frame)
        // lfo rate set per-sample below (optionally tracks pitch envelope)
        lfo.setWaveform(static_cast<LfoWave>(g_lfo_waveform.load(rlx)));
        osc.setWaveform(static_cast<Waveform>(waveform));
        filter.setResonance(reso);

        // Update active delay
        if (dly_type == 0) {
            delay.setTime(dly_t);
            delay.setFeedback(dly_fb);
            delay.setMix(dly_mix);
            delay.setRepitchRate(0.3f);
            delay.setWobbleAmount(wobble);
            delay.setFlutterAmount(flutter);
        } else {
            delay_digital.setTime(dly_t);
            delay_digital.setFeedback(dly_fb);
            delay_digital.setMix(dly_mix);
        }

        // Update active reverb
        bool super_drip = g_super_drip.load(rlx);
        switch (rev_type) {
        case 0:
            reverb.setSize(REVERB_SIZE);
            reverb.setMix(rev_mix);
            reverb.setSuperDrip(super_drip);
            break;
        case 1:
            reverb_plate.setSize(REVERB_SIZE);
            reverb_plate.setMix(rev_mix);
            break;
        case 2:
            reverb_hall.setSize(REVERB_SIZE);
            reverb_hall.setMix(rev_mix);
            break;
        case 3:
            reverb_schroeder.setSize(REVERB_SIZE);
            reverb_schroeder.setMix(rev_mix);
            break;
        }

        // Gate edge — envelopes start on release, reset on press
        if (gate && !prev_gate) {
            pitch_env_mult = 1.0f;      // reset on new trigger press
            sweep_phase    = 0.0f;      // filter sweep restarts from cutoff
            pe_active      = false;     // no sweep while held
            lfo.resetPhase();           // consistent attacks
        }
        if (!gate && prev_gate)
            pe_active = true;           // start sweep on release
        prev_gate = gate;

        // Per-sample processing
        for (int i = 0; i < frames; i++) {

            // Pitch envelope — sweep only after trigger release, only
            // when the pitch switch is set to Rise or Fall.
            if (pe_active && pe_mode != 0) {
                pitch_env_mult *= (pe_mode > 0) ? rise_factor
                                                : fall_factor;
                pitch_env_mult  = std::clamp(pitch_env_mult, 0.125f, 8.0f);
            }

            // Sweep phase: 0 while held (static cutoff), ramps 0→1 on release.
            if (pe_active)
                sweep_phase = std::min(sweep_phase + sweep_inc, 1.0f);

            // LFO rate optionally tracks pitch envelope (secret mode)
            lfo.setRate(lfo_link ? lfo_r * pitch_env_mult : lfo_r);

            // LFO → exponential pitch modulation (pitch only, not filter)
            float lfo_out = lfo.tick();
            float lfo_mod = std::exp2(lfo_out * lfo_d);
            float freq = base_freq * pitch_env_mult * lfo_mod;
            freq = std::clamp(freq, 20.0f, 20000.0f);
            osc.setFrequency(freq);

            // Filter: static at cutoff while held; sweeps directionally on release.
            // sweep_dir * 3 octaves at sweep_phase=1: +1→8x brighter, -1→1/8 darker.
            float eff_cutoff = std::clamp(
                cutoff * std::exp2(sweep_dir * 3.0f * sweep_phase),
                20.0f, 20000.0f);
            filter.setCutoff(eff_cutoff);

            // Oscillator
            float s = osc.tick();

            // Volume envelope (linear attack / release)
            if (gate)
                env_level += attack_rate;
            else
                env_level -= release_rate;
            env_level = std::clamp(env_level, 0.0f, 1.0f);
            s *= env_level;

            // Effects chain — three blocks: Filter, Delay, Reverb
            // Order is configurable via g_fx_chain
            auto do_filter = [&](float x) -> float {
                return filter.process(x);
            };
            auto do_delay = [&](float x) -> float {
                return (dly_type == 0) ? delay.process(x)
                                       : delay_digital.process(x);
            };
            auto do_reverb = [&](float x) -> float {
                switch (rev_type) {
                case 1:  return reverb_plate.process(x);
                case 2:  return reverb_hall.process(x);
                case 3:  return reverb_schroeder.process(x);
                default: return reverb.process(x);
                }
            };

            // Apply effects in configured order
            switch (fx_chain) {
            default:
            case 0:  s = do_filter(s); s = do_delay(s);  s = do_reverb(s); break;
            case 1:  s = do_filter(s); s = do_reverb(s); s = do_delay(s);  break;
            case 2:  s = do_delay(s);  s = do_filter(s); s = do_reverb(s); break;
            case 3:  s = do_delay(s);  s = do_reverb(s); s = do_filter(s); break;
            case 4:  s = do_reverb(s); s = do_filter(s); s = do_delay(s);  break;
            case 5:  s = do_reverb(s); s = do_delay(s);  s = do_filter(s); break;
            }

            // Tape saturator (post-chain, pre-modulation)
            if (sat_mix > 0.0f) {
                saturator.setDrive(sat_drive);
                saturator.setMix(sat_mix);
                s = saturator.process(s);
            }

            // Modulation effects (post-chain, pre-limiter)
            if (phaser_mix > 0.0f) {
                phaser.setMix(phaser_mix);
                s = phaser.process(s);
            }
            if (chorus_mix > 0.0f) {
                chorus.setMix(chorus_mix);
                s = chorus.process(s);
            }
            if (flanger_mix > 0.0f) {
                flanger.setMix(flanger_mix);
                s = flanger.process(s);
            }

            // Output soft limiter — transparent at normal levels,
            // warm saturation when pushed (resonance, feedback, etc.)
            s = dsp::fast_tanh(s * 1.2f) * (1.0f / 1.1f);

            buf[i] = s;

            // Stash last LFO output for LED (overwritten each sample, cheap)
            if (i == frames - 1)
                g_lfo_out.store(lfo_out, std::memory_order_relaxed);
        }
    };

    // ── Init audio engine ───────────────────────────────────────────

    if (audio.init(device, SAMPLE_RATE, audio_cb)) {
        float sr = static_cast<float>(audio.sampleRate());
        sr_f = sr;
        osc.setSampleRate(sr);
        lfo.setSampleRate(sr);
        filter.setSampleRate(sr);
        delay.init(sr, 1.5f);
        delay.setRepitchRate(0.3f);
        delay_digital.init(sr, 1.5f);
        reverb.init(sr);
        reverb.setSuperDrip(true);
        reverb_plate.init(sr);
        reverb_hall.init(sr);
        reverb_schroeder.init(sr);
        phaser.init(sr);
        chorus.init(sr);
        flanger.init(sr);
        saturator.init(sr);

        attack_rate  = 1.0f / (0.005f * sr);           // ~5 ms
        release_rate = 1.0f / (0.050f * sr);            // ~50 ms

        audio.start();
    } else {
        fprintf(stderr, "Audio engine init failed — exiting so systemd can restart\n");
        return 1;
    }

    // ── GPIO callbacks ──────────────────────────────────────────────

    HwCallbacks cb;

    cb.on_encoder = [](int id, int dir) {
        if (id < 0 || id > 4) return;
        bool  shift = g_shift.load();
        float accel = encoder_accel(id);

        int param_idx;
        {
            std::lock_guard<std::mutex> lk(g_encoder_map_mutex);
            param_idx = shift ? g_encoder_map_b[id] : g_encoder_map_a[id];
        }
        if (param_idx < 0 || param_idx >= NUM_PARAMS) return;

        const auto& p = PARAM_TABLE[param_idx];
        auto* atom = param_atomic_for(p.name);
        if (!atom) return;

        float val = atom->load();
        float sens;
        {
            std::lock_guard<std::mutex> lk(g_sensitivity_mutex);
            sens = g_param_sensitivity[param_idx];
        }
        float eff_accel = 1.0f + (accel - 1.0f) * sens;
        if (p.is_log) {
            float step = std::pow(p.step, eff_accel);
            val *= (dir > 0) ? step : (1.0f / step);
        } else {
            val += dir * p.step * eff_accel;
        }
        val = std::clamp(val, p.min_val, p.max_val);
        atom->store(val);

        // Update linked effective parameters if needed
        if (param_idx == 0 || param_idx == 1 || param_idx == 3)
            update_link_eff();

        printf("  [%c] %-8s  %s\n", shift ? 'B' : 'A', p.label,
               p.is_log && p.max_val > 100
                   ? (std::to_string(static_cast<int>(val)) + (p.max_val > 100 ? " Hz" : "")).c_str()
                   : (std::to_string(static_cast<int>(val * 100)) + "%").c_str());
    };

    cb.on_button = [](int id, bool pressed) {
        static const char *btn_name[] = {"Trigger", "Shift", "Preset"};

        // Track button states for 3-button AP mode combo
        switch (id) {
        case 0: g_btn_trigger.store(pressed); break;
        case 1: g_btn_shift.store(pressed); break;
        case 2: g_btn_preset.store(pressed); break;
        }

        // Log button state for AP combo debugging (visible in system log)
        slog("[btn] %s %s (T=%d S=%d P=%d)",
             btn_name[id], pressed ? "DOWN" : "UP",
             g_btn_trigger.load(), g_btn_shift.load(),
             g_btn_preset.load());

        // Trigger gate always works (including AP mode for preview)
        if (id == 0) {
            g_gate.store(pressed);
            printf("  %-9s %s\n", btn_name[0],
                   pressed ? "GATE ON" : "GATE OFF");
        }

        // Skip other button handling when in AP mode or 3-button combo active
        if (g_ap_mode.load()) return;
        if (g_combo_suppress.load()) {
            // Still track shift state so it's correct after combo ends
            if (id == 1) g_shift.store(pressed);
            return;
        }

        switch (id) {
        case 0:
            // Already handled above
            break;
        case 1: {
            // Shift → bank select
            //   Double-click: toggle standard ↔ user preset bank
            //   Triple-click: toggle experimental ↔ user preset bank
            g_shift.store(pressed);

            if (pressed) {
                auto now = std::chrono::steady_clock::now();

                // Reset if too long since first click
                if (g_shift_clicks > 0) {
                    auto since = std::chrono::duration_cast<
                        std::chrono::milliseconds>(
                            now - g_shift_first_click).count();
                    if (since > 600) g_shift_clicks = 0;
                }

                if (g_shift_clicks == 0) g_shift_first_click = now;
                g_shift_clicks++;

                if (g_shift_clicks >= 3) {
                    // Triple-click: switch to experimental bank
                    g_shift_dblclick_pending.store(false);
                    auto span = std::chrono::duration_cast<
                        std::chrono::milliseconds>(
                            now - g_shift_first_click).count();
                    g_shift_clicks = 0;
                    if (span < 500) {
                        toggle_bank(BankMode::EXPERIMENTAL);
                    }
                } else if (g_shift_clicks == 2) {
                    // Pending double-click (may become triple)
                    g_shift_dblclick_pending.store(true);
                    g_shift_dblclick_time = now;
                }
            }

            slog("BANK %s", pressed ? "B" : "A");
            break;
        }
        case 2: {
            // Preset button: cycle presets, save (long-press)
            //   Shift+press:  cycle LFO waveform (immediate)
            //   Short press:  cycle preset (immediate)
            //   Long-press (3s): save current state to user bank slot

            static auto btn2_press_time = std::chrono::steady_clock::time_point{};
            static bool btn2_shift_at_press = false;

            if (pressed) {
                btn2_shift_at_press = g_shift.load();
                if (btn2_shift_at_press) {
                    // Shift+Press: cycle LFO waveform (fires immediately)
                    int lw = (g_lfo_waveform.load() + 1)
                           % static_cast<int>(LfoWave::COUNT);
                    g_lfo_waveform.store(lw);
                    slog("LFO WAVE %s", lfo_wave_name(lw));
                } else {
                    btn2_press_time = std::chrono::steady_clock::now();
                }
            } else {
                // Released
                if (!btn2_shift_at_press) {
                    auto now = std::chrono::steady_clock::now();
                    auto hold_ms = std::chrono::duration_cast<
                        std::chrono::milliseconds>(
                            now - btn2_press_time).count();

                    if (hold_ms >= 3000) {
                        // ── Long press: save to user bank ──
                        save_current_to_user_bank();
                    } else {
                        // ── Short press: cycle preset immediately ──
                        cycle_preset();
                    }
                }
            }
            break;
        }
        }
    };

    cb.on_pitch_switch = [](int pos) {
        g_pitch_env.store(pos);
        static const char *lbl[] = {"FALL", "OFF", "RISE"};
        printf("  PITCH-ENV  %s\n", lbl[pos + 1]);

        // Secret modes on triple-tap to FALL:
        //   Shift held  → super drip reverb (heavy dub spring)
        //   Shift free  → LFO-pitch-envelope link
        if (pos == -1) {
            if (g_shift.load()) {
                static MultiClickDetector det{3, 700, 800};
                if (det.click()) {
                    bool sd = !g_super_drip.load();
                    g_super_drip.store(sd);
                    printf("  >>> SUPER DRIP REVERB %s\n",
                           sd ? "ON" : "OFF");
                }
            } else {
                static MultiClickDetector det{3, 700, 800};
                if (det.click()) {
                    bool lk = !g_lfo_pitch_link.load();
                    g_lfo_pitch_link.store(lk);
                    printf("  >>> LFO-PITCH LINK %s\n",
                           lk ? "ON" : "OFF");
                }
            }
        }
    };

    // ── Keyboard-only shortcuts (simulate mode) ─────────────────────

    cb.on_save_preset = []() { save_current_to_user_bank(); };

    cb.on_toggle_bank = []() {
        // Keyboard shortcut cycles: USER → STANDARD → EXPERIMENTAL → USER
        auto cur = static_cast<BankMode>(g_bank_mode.load());
        BankMode next;
        switch (cur) {
        case BankMode::USER:         next = BankMode::STANDARD;     break;
        case BankMode::STANDARD:     next = BankMode::EXPERIMENTAL; break;
        case BankMode::EXPERIMENTAL: next = BankMode::USER;         break;
        default:                     next = BankMode::USER;         break;
        }
        switch_bank(next);
    };

    // ── Initialise GPIO / simulate ──────────────────────────────────

    GpioHw hw;
    if (!hw.init(simulate, cb)) {
        fprintf(stderr, "Failed to initialise hardware.\n");
        audio.shutdown();
        return 1;
    }

    // Initialise LED driver (non-fatal if ws2811 not available)
    if (!g_led.init()) {
        slog("LED: init failed — continuing without LED");
    }

    // Load persistent siren configuration (reverb type, delay type, etc.)
    load_siren_config();

    // Initialise user presets (factory defaults + saved overrides)
    init_user_presets();
    init_bank_presets();

    // Apply the saved active bank/preset
    {
        auto mode = static_cast<BankMode>(g_bank_mode.load());
        switch_bank(mode);
    }

    // Start web server (always-on for poorhouse.local/config access)
    {
        auto cb = build_web_callbacks();
        if (!web_server::start(80, cb)) {
            slog("WEB: Port 80 failed — trying port 8080");
            if (!web_server::start(8080, cb)) {
                slog("WEB: Failed to start web server on port 8080");
                // Non-fatal — siren still works without web config
            } else {
                slog("WEB: Server running on port 8080");
            }
        }
    }

    // Boot into user bank (identical to standard on first boot)
    apply_user_preset(g_user_presets[0]);

    {
        int idx = g_preset.load();
        const UserPreset& u = g_user_presets[idx];
        printf("\nBank: USER  Preset %d  \"%s\"%s\n",
               idx + 1, u.name, u.saved ? "" : "  (factory copy)");
        printf("  Freq: %.0f Hz  Wave: %s\n",
               g_freq.load(), waveform_name(g_waveform.load()));
        printf("  LFO: %.1f Hz @ %.0f%% [%s]  Filter: %.0f Hz / %.0f%%\n",
               g_lfo_rate.load(), g_lfo_depth.load() * 100.0f,
               lfo_wave_name(g_lfo_waveform.load()),
               g_filter_cutoff.load(), g_filter_reso.load() * 100.0f);
        printf("  Delay: %.0f ms  FB %.0f%%  Mix %.0f%%\n",
               g_delay_time.load() * 1000.0f,
               g_delay_feedback.load() * 100.0f,
               g_delay_mix.load() * 100.0f);
        printf("  Release: %.0f ms  Reverb: Mix %.0f%%\n",
               g_release_time.load() * 1000.0f,
               g_reverb_mix.load() * 100.0f);
        static const char *pe_lbl[] = {"Fall", "Off", "Rise"};
        printf("  Pitch Env: %s\n", pe_lbl[g_pitch_env.load() + 1]);
    }
    printf("\nListening for events (Ctrl-C to quit) ...\n\n");

    // ── Main loop ───────────────────────────────────────────────────
    while (g_running) {
        hw.poll();

        // Check for web UI exit request
        if (g_ap_exit_requested.load() && g_ap_mode.load()) {
            exit_ap_mode();
        }

        // Check for 3-button AP mode combo
        check_ap_combo(audio);

        // LED update (~20 Hz — main loop runs at ~20 Hz from hw.poll())
        {
            float bright = g_led_brightness.load();
            if (g_led_night_mode.load()) bright = std::min(bright, 0.15f);
            g_led.setBrightness(bright);
        }
        g_led.update(
            static_cast<LfoWave>(g_lfo_waveform.load()),
            g_lfo_out.load(std::memory_order_relaxed),
            g_lfo_depth.load(), g_gate.load(),
            g_freq.load(), g_lfo_rate.load());

        // Resolve pending shift double-click (fires 350 ms after
        // 2nd press if no 3rd click arrives for triple-click)
        if (!g_ap_mode.load() && g_shift_dblclick_pending.load()) {
            auto elapsed = std::chrono::duration_cast<
                std::chrono::milliseconds>(
                    std::chrono::steady_clock::now()
                    - g_shift_dblclick_time).count();
            if (elapsed > 350) {
                g_shift_dblclick_pending.store(false);
                g_shift_clicks = 0;
                toggle_bank(BankMode::STANDARD);
            }
        }
    }

    // Clean up
    web_server::stop();
    if (g_ap_mode.load()) {
        ap_mode::stop_ap();
    }

    printf("\nShutting down.\n");
    g_led.shutdown();
    audio.shutdown();
    hw.shutdown();
    return 0;
}
