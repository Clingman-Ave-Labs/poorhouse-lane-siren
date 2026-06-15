#pragma once

#include <functional>
#include <memory>

// ─── GPIO pin assignments (BCM numbering) — Pi Zero 2W ─────────────
namespace gpio {

struct EncoderPins { unsigned clk; unsigned dt; };

constexpr EncoderPins ENCODERS[] = {
    { 2, 17},   // Encoder 1
    {22, 27},   // Encoder 2
    {24, 23},   // Encoder 3
    {26, 20},   // Encoder 4
    {13, 14},   // Encoder 5
};
constexpr int NUM_ENCODERS = 5;

// Momentary buttons (active LOW with pull-up)
constexpr unsigned BUTTONS[] = {4, 15, 5};  // Trigger, Shift (dbl-click=bank), Preset
constexpr int NUM_BUTTONS = 3;

// 3-position pitch-envelope switch (active LOW)
//   Rise: SW_RISE pulled LOW      → pos = +1
//   Off:  both HIGH (center)      → pos =  0
//   Fall: SW_FALL pulled LOW      → pos = -1
constexpr unsigned SW_RISE = 9;
constexpr unsigned SW_FALL = 10;

} // namespace gpio

// ─── Callbacks ──────────────────────────────────────────────────────
struct HwCallbacks {
    std::function<void(int encoder_id, int direction)> on_encoder;      // +1 CW, -1 CCW
    std::function<void(int button_id, bool pressed)>   on_button;
    std::function<void(int position)>                  on_pitch_switch; // -1 / 0 / +1
    std::function<void()>                              on_save_preset;  // keyboard shortcut
    std::function<void()>                              on_toggle_bank;  // keyboard shortcut
};

// ─── Hardware abstraction ───────────────────────────────────────────
class GpioHw {
public:
    GpioHw();
    ~GpioHw();

    // simulate = true  →  keyboard input (no GPIO required)
    // simulate = false →  real libgpiod v2 on /dev/gpiochip0
    bool init(bool simulate, const HwCallbacks& cb);
    void poll();      // call in tight loop — blocks up to ~50 ms
    void shutdown();

private:
    struct Impl;
    std::unique_ptr<Impl> pimpl_;
};
