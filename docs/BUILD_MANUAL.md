# Poorhouse Siren — Build Manual

**Raspberry Pi-Powered Dub Siren Synthesizer**

_Clingman Ave Labs_

This manual describes how to assemble a Poorhouse Siren from the bare PCB,
Raspberry Pi Zero 2W, and the off-the-shelf components listed below. For
flashing and software installation after the hardware is built, see
[USER_MANUAL.md → Appendix A](USER_MANUAL.md#appendix-a-first-install-on-a-clean-raspberry-pi).

---

## Table of Contents

1. [Overview](#1-overview)
2. [Bill of Materials](#2-bill-of-materials)
3. [PCB and Enclosure Files](#3-pcb-and-enclosure-files)
4. [Tools Required](#4-tools-required)
5. [GPIO Pin Reference](#5-gpio-pin-reference)
6. [Assembly](#6-assembly)
7. [Power Input (USB-C)](#7-power-input-usb-c)
8. [First Power-On and Test](#8-first-power-on-and-test)
9. [Troubleshooting the Build](#9-troubleshooting-the-build)

---

## 1. Overview

The Poorhouse Siren is built around a Raspberry Pi Zero 2W mounted on a custom
carrier PCB. The PCB carries the five rotary encoders, three momentary buttons,
the three-position pitch-envelope toggle, the addressable status LED, the
PCM5102 I2S DAC, and the 1/4″ mono output jack. All control wiring is routed
through the PCB, so assembly is primarily a soldering and mechanical-fit
exercise rather than point-to-point wiring.

Signal path overview:

**Pi Zero 2W (GPIO + I2S) → PCM5102 DAC → 1/4″ mono output**

---

## 2. Bill of Materials

| Qty | Component | Notes | Reference |
|-----|-----------|-------|-----------|
| 1 | Raspberry Pi Zero 2W | The compute module. Not included in the linked parts. | — |
| 1 | Poorhouse Siren carrier PCB | Fabricated from the gerbers in this repo. | [Section 3](#3-pcb-and-enclosure-files) |
| 5 | EC11 rotary encoder | Knobs 1–5. CLK/DT/GND pins used; the integral push switch is **not** used by the firmware. | [Amazon B09KNC1J6H](https://www.amazon.com/dp/B09KNC1J6H) |
| 3 | 16 mm momentary push button | **Trigger**, **Bank (Shift)**, and **Preset** buttons. Momentary, normally-open. | [Amazon B081JJBN4G](https://www.amazon.com/dp/B081JJBN4G) |
| 1 | ON-OFF-ON toggle switch (SPDT, center-off) | Pitch-envelope switch (Rise / Off / Fall). The center-off position is required. | [Amazon B07BD1NNCC](https://www.amazon.com/dp/B07BD1NNCC) |
| 1 | PCM5102 I2S DAC board | Stereo 24-bit I2S DAC. Mounted as the audio output stage. | [Amazon B08YNJGSN4](https://www.amazon.com/dp/B08YNJGSN4) |
| 1 | 40-pin female header ("Pi HAT" header) | Connects the carrier PCB to the Pi Zero 2W GPIO. | [Amazon B084Q4W1PW](https://www.amazon.com/dp/B084Q4W1PW) |
| 1 | 1/4″ (6.35 mm) mono panel jack | Audio output. | [Amazon B08MT66VPX](https://www.amazon.com/dp/B08MT66VPX) |
| 1 | USB-C female connector / breakout | Used to build a USB-C-female → micro-USB-male power lead for the Pi. | [Amazon B0DL5JJS5T](https://www.amazon.com/dp/B0DL5JJS5T) |
| 1 | APA106 addressable LED | **Optional.** Single status LED. Driven on a dedicated GPIO. | [Amazon B071FND4WK](https://www.amazon.com/dp/B071FND4WK) |
| 1 | LED mounting holder/bezel | **Optional.** Holds the APA106 in the enclosure face. Only needed if fitting the LED. | [Amazon B0974DL4QR](https://www.amazon.com/dp/B0974DL4QR) |
| 5 | Encoder knobs | Fit the EC11 6 mm D-shaft. Included in the enclosure 3MF. | [Section 3](#3-pcb-and-enclosure-files) |
| — | Hookup wire, M2.5 standoffs/screws, solder | As needed for mounting and the USB-C power lead. | — |

> **Note on the addressable LED:** The firmware drives a single APA106 LED (see
> `src/led_driver.cpp`). The LED is **optional** — the enclosure 3MF includes
> face plates both with and without the LED hole. If you omit the LED, the
> firmware still runs; only the visual status indicator is absent.

---

## 3. PCB and Enclosure Files

Both fabrication files ship in this repository:

| File | Purpose |
|------|---------|
| [`poorhouse-lane-siren-gerber.zip`](../poorhouse-lane-siren-gerber.zip) | Gerber package for the carrier PCB. Upload directly to any PCB fab house (JLCPCB, PCBWay, OSH Park, etc.). |
| [`poorhouse_lane_siren_enclosure_and_knobs.3mf`](../poorhouse_lane_siren_enclosure_and_knobs.3mf) | 3D-printable enclosure and encoder knobs. Slice and print in PLA/PETG. |

**Ordering the PCB:** Upload `poorhouse-lane-siren-gerber.zip` as-is. Default
settings (1.6 mm FR-4, HASL finish) are sufficient; no controlled impedance or
special stackup is required.

---

## 4. Tools Required

- Soldering iron and solder (fine tip recommended for the DAC and header)
- Flush cutters and wire strippers
- Small Phillips screwdriver
- Multimeter (for continuity checks before first power-on)
- 3D printer (or a print service) for the enclosure
- Heat-shrink tubing for the USB-C power lead

---

## 5. GPIO Pin Reference

All pin numbers below are **BCM (Broadcom) GPIO numbers**, taken directly from
the firmware (`include/gpio_hw.h` and `include/led_driver.h`). The carrier PCB
already routes these connections — this table is for verification, repair, and
hand-wiring.

### 5.1 Rotary Encoders (5×)

Each EC11 uses two GPIO lines (CLK + DT) plus a common ground. The encoder's
push-button switch is left unconnected.

| Encoder | CLK (BCM) | DT (BCM) |
|---------|-----------|----------|
| Knob 1 | GPIO2 | GPIO17 |
| Knob 2 | GPIO22 | GPIO27 |
| Knob 3 | GPIO24 | GPIO23 |
| Knob 4 | GPIO26 | GPIO20 |
| Knob 5 | GPIO13 | GPIO14 |

### 5.2 Momentary Buttons (3×)

Wired active-LOW with internal pull-ups: one terminal to the GPIO, the other to
GND. A press pulls the line LOW.

| Button | BCM | Function |
|--------|-----|----------|
| Trigger | GPIO4 | Hold to produce sound |
| Bank (Shift) | GPIO15 | Hold for Bank B; double/triple-click to switch banks |
| Preset | GPIO5 | Tap to cycle presets; hold 3 s to save |

### 5.3 Pitch-Envelope Toggle (ON-OFF-ON)

The center-off toggle uses two GPIO lines, both active-LOW. The center
(Off) position leaves both lines HIGH.

| Position | Line | BCM |
|----------|------|-----|
| Rise (up) | SW_RISE | GPIO9 |
| Off (center) | — (both HIGH) | — |
| Fall (down) | SW_FALL | GPIO10 |

Wire the toggle's common (center) terminal to GND, the "up" terminal to GPIO9,
and the "down" terminal to GPIO10.

### 5.4 Status LED

| Signal | BCM | Notes |
|--------|-----|-------|
| LED data | GPIO12 | APA106 / WS2811 data line (PWM0). Default in `led_driver.h`. |

The LED also needs 5 V and GND.

### 5.5 PCM5102 I2S DAC

The firmware's installer configures the `hifiberry-dac` overlay, which uses the
Pi's hardware I2S pins:

| DAC Signal | BCM | Pi physical pin |
|------------|-----|-----------------|
| BCK (bit clock) | GPIO18 | 12 |
| LRCK / WS (word select) | GPIO19 | 35 |
| DIN (data in) | GPIO21 | 40 |
| VIN | — | 5 V (pin 2 or 4) |
| GND | — | GND (e.g. pin 6) |

On the PCM5102 board, tie the control pins for I2S slave / standard operation
per the board silkscreen: **SCK → GND** (the board's internal PLL generates the
system clock), **FLT/DEMP/XSMT** to their default states (XSMT pulled high to
un-mute). Most ACEIRMC/PCM5102A breakouts have these jumpered correctly out of
the box.

---

## 6. Assembly

Work from the lowest-profile components to the tallest. Confirm orientation
before soldering — most of these parts are not easily removed once seated.

1. **40-pin header.** Solder the female 40-pin header to the carrier PCB on the
   side that mates with the Pi Zero 2W. Keep it square to the board.
2. **PCM5102 DAC.** Mount the DAC to its footprint. Double-check the BCK/LRCK/DIN
   and power/ground pin order against [Section 5.5](#55-pcm5102-i2s-dac) before
   soldering.
3. **Rotary encoders (×5).** Seat all five EC11 encoders flush against the PCB and
   solder. The mounting nut/washer secures them to the enclosure later.
4. **Momentary buttons (×3).** Install the three 16 mm buttons. Their physical
   positions map to Preset (left), Bank/Shift (center), and Trigger (right) per
   the enclosure layout.
5. **Pitch toggle.** Install the ON-OFF-ON toggle. Verify the center-off position
   mechanically before final mounting.
6. **1/4″ output jack.** Wire/mount the panel jack to the audio output pads
   (tip = signal, sleeve = GND for mono).
7. **Pi Zero 2W.** Seat the Pi onto the 40-pin header. Secure with M2.5 standoffs.
8. **Status LED (optional — last electrical step).** Solder the APA106 *after*
   everything else, fitted into its holder, so the LED ends up at the correct
   height for the enclosure face. See [Section 6.1](#61-fitting-the-optional-led)
   for the procedure. Skip this step if you are using the no-LED face plate.
9. **Enclosure.** Mount the populated PCB into the printed enclosure, fit the
   knobs onto the encoder shafts, and fasten the panel hardware (encoder nuts,
   button nuts, toggle nut, and jack nut).

### 6.1 Fitting the Optional LED

The APA106 **cannot be mounted flush** to the PCB — its legs must stay long
enough to reach through the LED holder/bezel in the enclosure face. Soldering it
flat against the board will leave it too short to seat in the holder.

Recommended procedure (do this as the last step before closing the enclosure):

1. Press-fit the APA106 into the LED holder/bezel
   ([B0974DL4QR](https://www.amazon.com/dp/B0974DL4QR)) installed in the
   enclosure face plate.
2. Pressure-fit the enclosure cover/face onto the assembled PCB so the LED is
   held at its final installed height, legs spanning the gap to the board.
3. With the LED held in position by the holder and cover, solder its legs to the
   PCB. This guarantees the leg length matches the real cover-to-board distance.
4. Observe polarity and the data-in direction (data, 5 V, GND) while soldering —
   reversed data-in is the most common cause of a dark LED.
5. Remove the cover if needed to finish/clean up, then proceed to final assembly.

> **Tip:** If you would rather not deal with the LED, the enclosure 3MF includes
> a face plate **without** the LED hole — use it and skip this section entirely.

---

## 7. Power Input (USB-C)

The Pi Zero 2W is powered through its **micro-USB power input** (the port nearest
the board edge/PWR label). To present a modern USB-C power connector on the
enclosure, build a short adapter lead:

1. Take the USB-C female connector/breakout
   ([B0DL5JJS5T](https://www.amazon.com/dp/B0DL5JJS5T)).
2. Wire **VBUS (5 V)** and **GND** from the USB-C female to a micro-USB male plug
   that feeds the Pi's PWR input.
3. For a basic 5 V supply, bridge the USB-C **CC1 and CC2 pins to GND through
   5.1 kΩ resistors** so the source enumerates and provides 5 V. Many USB-C
   breakout boards include these resistors already — check the silkscreen.
4. Insulate with heat-shrink and strain-relieve the lead inside the enclosure.

Use a 5 V supply rated for **at least 2 A**. Under-powering the Pi Zero 2W causes
brownouts and audio dropouts.

> **Tip:** If you prefer not to build the adapter lead, a commercial USB-C-female
> → micro-USB-male adapter can be used instead, as long as it carries the full
> 2 A.

---

## 8. First Power-On and Test

Before applying power, do a continuity check with a multimeter:

- No short between 5 V and GND on the USB-C lead or the Pi header.
- DAC VIN to 5 V and DAC GND to GND are continuous.
- LED 5 V/GND and data line are not shorted together.

Then:

1. Flash and install the firmware following
   [USER_MANUAL.md → Appendix A](USER_MANUAL.md#appendix-a-first-install-on-a-clean-raspberry-pi).
   The installer enables I2S, applies the `hifiberry-dac` overlay, and disables
   onboard audio.
2. Power on. After ~15–20 s the status LED should illuminate.
3. Connect a 1/4″ cable to an amp or powered speaker.
4. Hold **Trigger** — you should hear a siren tone. Turn the knobs to confirm
   each encoder responds.
5. Flip the pitch toggle to **Fall** and release Trigger to confirm the
   downward pitch slide.

Refer to the **Siren Health Check** one-liner in `CLAUDE.md` to confirm the
binary, autostart, and persistence are all in order.

---

## 9. Troubleshooting the Build

**No audio at all.** Verify the `hifiberry-dac` overlay applied (the installer
edits `/boot/firmware/config.txt`) and that onboard audio is disabled. Recheck
the DAC BCK/LRCK/DIN wiring against [Section 5.5](#55-pcm5102-i2s-dac). Confirm
the DAC's XSMT (mute) pin is pulled high.

**Distorted or very quiet output.** Confirm the output jack is wired for mono
(tip = signal, sleeve = ground) and that the destination input is line-level.

**An encoder turns the wrong parameter or backwards.** Swap that encoder's CLK
and DT connections — reversed CLK/DT inverts the direction. Confirm the encoder
maps to the expected knob per [Section 5.1](#51-rotary-encoders-5).

**A button does nothing / is always pressed.** A dead button usually means an
open or swapped connection; an always-on button means the GPIO is shorted to
GND. Verify against [Section 5.2](#52-momentary-buttons-3).

**Pitch toggle stuck in Rise or Fall.** The center-off position must leave both
GPIO9 and GPIO10 HIGH. If one line is permanently LOW, the common terminal is
likely miswired — it must go to GND with the two outer terminals to GPIO9/GPIO10.

**LED dark or wrong colors.** Confirm the data line is on GPIO12 and that 5 V/GND
reach the LED. A wrong-direction data line (DOUT vs DIN) is the most common
cause of a dark LED.

**Pi won't boot / brownouts.** Use a 5 V ≥ 2 A supply and verify the USB-C lead
delivers 5 V under load.
