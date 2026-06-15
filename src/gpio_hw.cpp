// gpio_hw.cpp — libgpiod v2 GPIO driver + desktop keyboard simulation
//
// All inputs use internal pull-ups (active LOW):
//   Physical HIGH (pull-up) = not pressed / contact open
//   Physical LOW  (ground)  = pressed / contact closed
//
// libgpiod active_low flag inverts this so the API reports:
//   ACTIVE  = pressed / contact closed   (physical LOW)
//   INACTIVE = released / contact open    (physical HIGH)

#include "gpio_hw.h"

#include <cstdio>
#include <array>

#ifdef HAS_GPIOD
#include <gpiod.h>
#endif

#include <termios.h>
#include <unistd.h>
#include <sys/select.h>

// ─── Quadrature encoder direction lookup ────────────────────────────
//
// State = (CLK << 1) | DT   (using logical active-low values)
// Index = (prev_state << 2) | cur_state
//
//  +1 = CW  (CLK leads DT)
//  -1 = CCW (DT leads CLK)
//   0 = no movement or invalid transition
//
static constexpr int8_t ENC_DIR[16] = {
     0, -1, +1,  0,
    +1,  0,  0, -1,
    -1,  0,  0, +1,
     0, +1, -1,  0
};

// ─── Private implementation ─────────────────────────────────────────

struct GpioHw::Impl {
    HwCallbacks cb;
    bool simulate = false;

    // Encoder previous (CLK,DT) as 2-bit value
    std::array<uint8_t, gpio::NUM_ENCODERS> enc_prev{};

    // Pitch-switch previous position
    int sw_pos = 0;

#ifdef HAS_GPIOD
    struct gpiod_chip         *chip    = nullptr;
    struct gpiod_line_request *request = nullptr;
    struct gpiod_edge_event_buffer *ev_buf = nullptr;
#endif

    // Simulate mode: saved terminal state
    struct termios orig_term{};
    bool term_saved = false;
};

// ─── Constructor / destructor ───────────────────────────────────────

GpioHw::GpioHw()  = default;
GpioHw::~GpioHw() { shutdown(); }

// ─── init ───────────────────────────────────────────────────────────

bool GpioHw::init(bool simulate, const HwCallbacks& cb)
{
    pimpl_ = std::make_unique<Impl>();
    pimpl_->cb = cb;
    pimpl_->simulate = simulate;

#ifndef HAS_GPIOD
    if (!simulate) {
        fprintf(stderr,
            "NOTE: built without libgpiod — forcing --simulate\n");
        pimpl_->simulate = true;
    }
#endif

    // ── Simulate mode (keyboard) ────────────────────────────────────
    if (pimpl_->simulate) {
        if (tcgetattr(STDIN_FILENO, &pimpl_->orig_term) == 0) {
            pimpl_->term_saved = true;
            struct termios raw = pimpl_->orig_term;
            raw.c_lflag &= ~static_cast<tcflag_t>(ECHO | ICANON);
            raw.c_cc[VMIN]  = 0;
            raw.c_cc[VTIME] = 0;
            tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw);
        }
        printf("SIMULATE mode — keyboard map:\n");
        printf("  1-5           Encoder 1-5 CW\n");
        printf("  ! @ # $ %%    Encoder 1-5 CCW\n");
        printf("  z / Z         Trigger   press / release\n");
        printf("  x / X         Shift     press / release\n");
        printf("  c / C         Preset    press / release\n");
        printf("  v             Save to user preset (long-press shortcut)\n");
        printf("  b             Toggle factory / user bank\n");
        printf("  a / s / d     Pitch envelope  Rise / Off / Fall\n\n");
        return true;
    }

    // ── Real GPIO (libgpiod v2) ─────────────────────────────────────
#ifdef HAS_GPIOD
    auto *d = pimpl_.get();

    d->chip = gpiod_chip_open("/dev/gpiochip0");
    if (!d->chip) {
        perror("gpiod_chip_open(/dev/gpiochip0)");
        return false;
    }

    // Encoder line settings: pull-up, active-low, both edges, no debounce
    // (Encoders produce clean quadrature signals; the ENC_DIR table discards
    //  invalid transitions. Debounce silently drops events from marginal
    //  connections — a common cause of encoders only working when pressed in.)
    auto *enc_set = gpiod_line_settings_new();
    gpiod_line_settings_set_direction(enc_set, GPIOD_LINE_DIRECTION_INPUT);
    gpiod_line_settings_set_bias(enc_set, GPIOD_LINE_BIAS_PULL_UP);
    gpiod_line_settings_set_active_low(enc_set, true);
    gpiod_line_settings_set_edge_detection(enc_set, GPIOD_LINE_EDGE_BOTH);

    // Button / switch settings: same but 5 ms debounce
    auto *btn_set = gpiod_line_settings_new();
    gpiod_line_settings_set_direction(btn_set, GPIOD_LINE_DIRECTION_INPUT);
    gpiod_line_settings_set_bias(btn_set, GPIOD_LINE_BIAS_PULL_UP);
    gpiod_line_settings_set_active_low(btn_set, true);
    gpiod_line_settings_set_edge_detection(btn_set, GPIOD_LINE_EDGE_BOTH);
    gpiod_line_settings_set_debounce_period_us(btn_set, 5000);

    auto *lcfg = gpiod_line_config_new();

    // Encoder lines (10 pins: 5 encoders x CLK+DT)
    for (int i = 0; i < gpio::NUM_ENCODERS; i++) {
        unsigned pins[] = { gpio::ENCODERS[i].clk, gpio::ENCODERS[i].dt };
        gpiod_line_config_add_line_settings(lcfg, pins, 2, enc_set);
    }

    // Button lines
    unsigned btns[] = {
        gpio::BUTTONS[0], gpio::BUTTONS[1], gpio::BUTTONS[2]
    };
    gpiod_line_config_add_line_settings(lcfg, btns, gpio::NUM_BUTTONS,
                                        btn_set);

    // Pitch-switch lines
    unsigned sw[] = { gpio::SW_RISE, gpio::SW_FALL };
    gpiod_line_config_add_line_settings(lcfg, sw, 2, btn_set);

    // Request all lines
    auto *rcfg = gpiod_request_config_new();
    gpiod_request_config_set_consumer(rcfg, "siren-v4");

    d->request = gpiod_chip_request_lines(d->chip, rcfg, lcfg);

    gpiod_request_config_free(rcfg);
    gpiod_line_config_free(lcfg);
    gpiod_line_settings_free(btn_set);
    gpiod_line_settings_free(enc_set);

    if (!d->request) {
        perror("gpiod_chip_request_lines");
        fprintf(stderr,
            "HINT: make sure GPIO 14/15 are free (disable serial console\n"
            "      with raspi-config) and GPIO 20 isn't claimed by I2S.\n");
        return false;
    }

    d->ev_buf = gpiod_edge_event_buffer_new(64);
    if (!d->ev_buf) {
        perror("gpiod_edge_event_buffer_new");
        return false;
    }

    // Snapshot initial encoder states
    for (int i = 0; i < gpio::NUM_ENCODERS; i++) {
        int a = gpiod_line_request_get_value(d->request,
                                             gpio::ENCODERS[i].clk);
        int b = gpiod_line_request_get_value(d->request,
                                             gpio::ENCODERS[i].dt);
        d->enc_prev[i] =
            static_cast<uint8_t>(
                ((a == GPIOD_LINE_VALUE_ACTIVE) << 1) |
                 (b == GPIOD_LINE_VALUE_ACTIVE));
    }

    // Read initial switch position
    {
        int r = gpiod_line_request_get_value(d->request, gpio::SW_RISE);
        int f = gpiod_line_request_get_value(d->request, gpio::SW_FALL);
        d->sw_pos = (r == GPIOD_LINE_VALUE_ACTIVE)  ?  1
                  : (f == GPIOD_LINE_VALUE_ACTIVE)   ? -1
                  :                                     0;
    }

    printf("GPIO ready  (%d encoders, %d buttons, 1 pitch-switch)\n",
           gpio::NUM_ENCODERS, gpio::NUM_BUTTONS);

    printf("  Encoder pins:");
    for (int i = 0; i < gpio::NUM_ENCODERS; i++)
        printf("  [%d] %u/%u", i + 1,
               gpio::ENCODERS[i].clk, gpio::ENCODERS[i].dt);

    printf("\n  Button  pins: Trigger=%u  Shift=%u  Waveform=%u\n",
           gpio::BUTTONS[0], gpio::BUTTONS[1], gpio::BUTTONS[2]);
    printf("  Switch  pins: Rise=%u  Fall=%u\n\n",
           gpio::SW_RISE, gpio::SW_FALL);

    return true;
#else
    return false;
#endif
}

// ─── poll ───────────────────────────────────────────────────────────

void GpioHw::poll()
{
    if (!pimpl_) return;
    auto& cb = pimpl_->cb;

    // ── Simulate: keyboard input ────────────────────────────────────
    if (pimpl_->simulate) {
        fd_set fds;
        FD_ZERO(&fds);
        FD_SET(STDIN_FILENO, &fds);
        struct timeval tv = {0, 50000};   // 50 ms
        if (select(STDIN_FILENO + 1, &fds, nullptr, nullptr, &tv) <= 0)
            return;

        char ch = 0;
        if (read(STDIN_FILENO, &ch, 1) != 1) return;

        // Encoder CW: keys 1-5
        if (ch >= '1' && ch <= '5') {
            if (cb.on_encoder) cb.on_encoder(ch - '1', +1);
            return;
        }
        // Encoder CCW: Shift+1-5  →  ! @ # $ %
        {
            static const char ccw[] = "!@#$%";
            for (int i = 0; i < 5; i++) {
                if (ch == ccw[i]) {
                    if (cb.on_encoder) cb.on_encoder(i, -1);
                    return;
                }
            }
        }
        // Buttons: lowercase = press, UPPERCASE = release
        switch (ch) {
            case 'z': if (cb.on_button) cb.on_button(0, true);  return;
            case 'Z': if (cb.on_button) cb.on_button(0, false); return;
            case 'x': if (cb.on_button) cb.on_button(1, true);  return;
            case 'X': if (cb.on_button) cb.on_button(1, false); return;
            case 'c': if (cb.on_button) cb.on_button(2, true);  return;
            case 'C': if (cb.on_button) cb.on_button(2, false); return;
            default: break;
        }
        // Preset shortcuts (simulate long-press / double-click)
        if (ch == 'v') { if (cb.on_save_preset) cb.on_save_preset(); return; }
        if (ch == 'b') { if (cb.on_toggle_bank) cb.on_toggle_bank(); return; }

        // Pitch switch
        switch (ch) {
            case 'a': if (cb.on_pitch_switch) cb.on_pitch_switch(+1); return;
            case 's': if (cb.on_pitch_switch) cb.on_pitch_switch( 0); return;
            case 'd': if (cb.on_pitch_switch) cb.on_pitch_switch(-1); return;
            default: break;
        }
        return;
    }

    // ── Real GPIO: edge-event driven ────────────────────────────────
#ifdef HAS_GPIOD
    auto *d = pimpl_.get();

    // Block up to 16 ms (~60 Hz loop rate) for smoother LED animation
    int ret = gpiod_line_request_wait_edge_events(d->request,
                                                  16000000LL);  // ns
    if (ret <= 0) return;   // timeout (0) or error (<0)

    int n = gpiod_line_request_read_edge_events(d->request, d->ev_buf, 64);
    if (n <= 0) return;

    for (int i = 0; i < n; i++) {
        auto *ev  = gpiod_edge_event_buffer_get_event(d->ev_buf,
                        static_cast<unsigned long>(i));
        unsigned off = gpiod_edge_event_get_line_offset(ev);
        bool rising  = (gpiod_edge_event_get_event_type(ev)
                        == GPIOD_EDGE_EVENT_RISING_EDGE);

        // ── Encoder? ────────────────────────────────────────────────
        bool handled = false;
        for (int e = 0; e < gpio::NUM_ENCODERS && !handled; e++) {
            if (off != gpio::ENCODERS[e].clk &&
                off != gpio::ENCODERS[e].dt)
                continue;

            int a = gpiod_line_request_get_value(d->request,
                                                 gpio::ENCODERS[e].clk);
            int b = gpiod_line_request_get_value(d->request,
                                                 gpio::ENCODERS[e].dt);
            uint8_t cur = static_cast<uint8_t>(
                ((a == GPIOD_LINE_VALUE_ACTIVE) << 1) |
                 (b == GPIOD_LINE_VALUE_ACTIVE));

            int8_t dir = ENC_DIR[(d->enc_prev[e] << 2) | cur];
            d->enc_prev[e] = cur;

            // Fire only on CLK falling edge — one trigger per detent
            if (dir != 0 && !rising &&
                off == gpio::ENCODERS[e].clk && cb.on_encoder)
                cb.on_encoder(e, dir);

            handled = true;
        }
        if (handled) continue;

        // ── Button? ─────────────────────────────────────────────────
        for (int b = 0; b < gpio::NUM_BUTTONS && !handled; b++) {
            if (off == gpio::BUTTONS[b]) {
                // rising edge (logical) = INACTIVE→ACTIVE = pressed
                if (cb.on_button) cb.on_button(b, rising);
                handled = true;
            }
        }
        if (handled) continue;

        // ── Pitch switch? ───────────────────────────────────────────
        if (off == gpio::SW_RISE || off == gpio::SW_FALL) {
            int r = gpiod_line_request_get_value(d->request,
                                                 gpio::SW_RISE);
            int f = gpiod_line_request_get_value(d->request,
                                                 gpio::SW_FALL);
            int pos = (r == GPIOD_LINE_VALUE_ACTIVE)  ?  1
                    : (f == GPIOD_LINE_VALUE_ACTIVE)   ? -1
                    :                                     0;
            if (pos != d->sw_pos) {
                d->sw_pos = pos;
                if (cb.on_pitch_switch) cb.on_pitch_switch(pos);
            }
        }
    }
#endif
}

// ─── shutdown ───────────────────────────────────────────────────────

void GpioHw::shutdown()
{
    if (!pimpl_) return;

    if (pimpl_->simulate) {
        if (pimpl_->term_saved)
            tcsetattr(STDIN_FILENO, TCSAFLUSH, &pimpl_->orig_term);
    }
#ifdef HAS_GPIOD
    else {
        if (pimpl_->ev_buf) {
            gpiod_edge_event_buffer_free(pimpl_->ev_buf);
            pimpl_->ev_buf = nullptr;
        }
        if (pimpl_->request) {
            gpiod_line_request_release(pimpl_->request);
            pimpl_->request = nullptr;
        }
        if (pimpl_->chip) {
            gpiod_chip_close(pimpl_->chip);
            pimpl_->chip = nullptr;
        }
    }
#endif

    pimpl_.reset();
}
