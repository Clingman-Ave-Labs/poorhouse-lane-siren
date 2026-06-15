#pragma once

#include "lfo.h"
#include <memory>

// ─── APA106 / WS2811 LED driver ─────────────────────────────────────
//
// Drives a single addressable LED to communicate device state:
//   - Waveform shape → pure color (R/G/B/O/C/Y/M/W, diffusion-friendly)
//   - LFO output → brightness pulse (depth=1 → full on/off blink)
//   - Frequency → saturation (low=vivid, high=slightly washed, smoothed)
//   - Gate → brightness boost (LFO frozen during release tail)
//   - AP mode → white Doppler fly-by animation
//   - Preset save → triple white blink
//
// Uses rpi_ws281x on Pi (HAS_WS2811), empty stubs on desktop.

class LedDriver {
public:
    LedDriver();
    ~LedDriver();

    // Initialise the LED hardware. Returns false on failure (non-fatal).
    bool init(int gpio_pin = 12);

    // Set master brightness multiplier (0.0–1.0). Applied to all output.
    void setBrightness(float level);

    // Call from main loop at ~20 Hz with current siren state.
    // lfo_out: raw LFO output in [-1, +1]
    void update(LfoWave wave, float lfo_out, float lfo_depth,
                bool gate, float freq, float lfo_rate);

    // Visual feedback: 3× white flash on preset save (~400 ms, blocking)
    void blinkSave();

    // AP mode Doppler fly-by in white (replaces audio wind-up cue)
    // Blocking ~1.5s animation, then returns. Caller should set AP idle after.
    void playApEnter();

    // Reverse Doppler fly-by for exiting AP mode (~1.5s blocking)
    void playApExit();

    // Set AP mode idle state (gentle white breathing while AP is active)
    void setApIdle(bool active);

    // Clean shutdown — LED off
    void shutdown();

private:
    struct Impl;
    std::unique_ptr<Impl> pimpl_;
};
