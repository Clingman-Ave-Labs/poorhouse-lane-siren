# Poorhouse Siren — User Manual

**Raspberry Pi-Powered Dub Siren Synthesizer**

_Clingman Ave Labs_

---

## Table of Contents

1. [Overview](#1-overview)
2. [Getting Started](#2-getting-started)
3. [WiFi Configuration](#3-wifi-configuration)
4. [Accessing the Web Interface](#4-accessing-the-web-interface)
5. [Physical Controls](#5-physical-controls)
6. [Preset Banks](#6-preset-banks)
7. [Encoder Reference](#7-encoder-reference)
8. [LFO Waveforms](#8-lfo-waveforms)
9. [Pitch Envelope Toggle](#9-pitch-envelope-toggle)
10. [Advanced Features](#10-advanced-features)
11. [Oscillator Waveforms](#11-oscillator-waveforms)
12. [Web Interface Guide](#12-web-interface-guide)
13. [Firmware Updates](#13-firmware-updates)
14. [Backup and Restore](#14-backup-and-restore)
15. [LED Indicator Reference](#15-led-indicator-reference)
16. [Signal Flow](#16-signal-flow)
17. [Techniques and Applications](#17-techniques-and-applications)
18. [Troubleshooting](#18-troubleshooting)
19. [Default Parameters](#19-default-parameters)

---

## 1. Overview

The Poorhouse Siren is a single-voice synthesizer designed for live dub and
reggae sound-system performance. It produces a range of effects including siren
wails, laser shots, machine-gun stutters, and deep sub-bass tones through a
configurable signal chain.

The signal chain is structured as follows:

**Oscillator → Filter → Delay → Reverb → Modulation FX → Limiter → Output**

The unit is powered by a Raspberry Pi Zero 2W and operates at 48 kHz / 24-bit
audio resolution. A built-in web interface provides access to all parameters,
preset management, firmware updates, and WiFi configuration from any
browser-equipped device.

---

## 2. Getting Started

### 2.1 Requirements

- The Poorhouse Siren unit (Raspberry Pi Zero 2W enclosed)
- USB-C power supply (5 V, 2 A minimum)
- Amplifier, mixer, or powered speaker with a 1/4″ (6.35 mm) input

### 2.2 Powering On

1. Connect a USB-C cable to the power port on the Pi.
2. Allow approximately 15–20 seconds for the device to boot. The LED indicator
   will illuminate when the unit is ready.
3. The Poorhouse Siren initializes in the User Bank with default settings.

### 2.3 Producing Sound

1. Press and hold the **Trigger** button (rightmost button). A siren tone will
   be audible through the connected speaker or amplifier.
2. Release the Trigger button. The tone will decay over the configured release
   time.
3. Adjust any knob while holding Trigger to shape the sound in real time.

> **Operating principle:** Hold Trigger to produce sound; release to stop. All
> other controls shape the character of the output between press and release.

### 2.4 Audio Output

Connect a standard 1/4″ (6.35 mm) mono cable from the audio output jack to the
destination amplifier, mixer, or powered speaker. The output is line-level
stereo at 48 kHz / 24-bit.

---

## 3. WiFi Configuration

The Poorhouse Siren includes a built-in web interface for comprehensive
control. Initial WiFi configuration is required to enable this interface.

### 3.1 Entering Access Point (AP) Mode

1. Hold all three buttons simultaneously (**Trigger + Shift + Preset**) for 3
   seconds.
2. A white Doppler animation on the LED confirms that AP mode is active.
3. The device creates a WiFi network named **Poorhouse-Siren-Config-XXXX**,
   where XXXX is a unique identifier derived from the device's MAC address.

### 3.2 Connecting to the Device Network

1. Open WiFi settings on a phone or laptop.
2. Connect to the `Poorhouse-Siren-Config-XXXX` network.
3. A captive portal should open automatically. If it does not, open a browser
   and navigate to `http://192.168.4.1`.

### 3.3 Configuring the Network

1. In the web interface, navigate to the **WiFi** tab.
2. Select **Scan** to discover available networks.
3. Select the desired network and enter its password.
4. Select **Test & Save** to verify the connection.

Once configured, the Poorhouse Siren will retain the network credentials and
reconnect automatically on subsequent boots.

### 3.4 Exiting AP Mode

Hold all three buttons (**Trigger + Shift + Preset**) for 3 seconds again. A
reverse LED animation confirms exit from AP mode. The device will connect to the
saved WiFi network.

> **Note:** While in AP mode, the standard WiFi connection may be temporarily
> unavailable depending on hardware capabilities. The device will attempt to
> maintain both connections when possible.

---

## 4. Accessing the Web Interface

Once the Poorhouse Siren is connected to a WiFi network, the web interface is
accessible from any device on the same network.

### 4.1 Primary URL

Open a browser and navigate to: **http://poorhouse.local**

This address uses mDNS (Bonjour) and is compatible with most modern devices
without additional configuration.

### 4.2 Fallback Options

If the primary URL is not accessible:

- Use the device hostname: `http://<hostname>.local`
- Use the IP address directly: Locate the Poorhouse Siren's IP in the router's
  device list, then navigate to `http://<ip-address>`

> **Note:** The web interface is served on port 80. No port number is required
> in the URL.

---

## 5. Physical Controls

The device features three buttons, five rotary encoders, and a three-position
pitch envelope toggle switch.

### 5.1 Control Layout

The top row contains Knobs 1 through 3 (left to right). The middle row contains
Knobs 4 and 5, with the Pitch Envelope toggle switch positioned to the right.
The bottom row contains three buttons: Preset (left), Shift (center), and
Trigger (right).

```
┌──────────────────────────────────────────┐
│                                            │
│   [Knob 1]  [Knob 2]  [Knob 3]             │
│                                            │
│   [Knob 4]  [Knob 5]  ┌─ Rise ─┐           │
│                       │  Off   │           │
│                       └─ Fall ─┘           │
│                                            │
│   (PRESET)   (SHIFT)   (TRIGGER)           │
│                                            │
└──────────────────────────────────────────┘
```

### 5.2 Button Functions

| Control | Function |
|---------|----------|
| **Preset** (left) | Tap to cycle through presets. Hold Shift + tap to change LFO waveform. Hold for 3 seconds to save the current state (User Bank only). |
| **Shift** (center) | Hold to access Bank B encoder parameters. Double-click to switch to the Standard Bank. Triple-click to switch to the Experimental Bank. |
| **Trigger** (right) | Hold to produce sound. Release to initiate the decay envelope. |
| **Pitch Toggle** | Three positions: Rise (up), Off (center), Fall (down). Controls pitch behavior on trigger release. |
| **Knobs 1–5** | Rotary encoders controlling different parameters depending on Shift state. |

---

## 6. Preset Banks

The Poorhouse Siren provides three banks of presets, each containing four preset
slots.

### 6.1 User Bank

The default bank on boot. Contains four editable slots initialized with copies
of the factory presets. To return to this bank after switching, double-click
Shift.

### 6.2 Standard Bank (Factory Presets)

Four preset dub siren configurations. These presets are read-only and cannot be
overwritten. Access by double-clicking the Shift button.

| # | Name | Description |
|---|------|-------------|
| 0 | **Slow Wail** | A 10-second sweeping siren. Sawtooth oscillator with sine LFO, deep delay, and long reverb tail. |
| 1 | **Machine Gun** | Rapid-fire stutter bursts. Square LFO chopping a square wave at 14 Hz. Percussive and aggressive character. |
| 2 | **Lickshot** | Laser-type effects. Triangle LFO on a square wave with a falling pitch envelope. Fast and punchy response. |
| 3 | **Droppa** | Descending wail with ramp-down LFO and heavy delay feedback. Dramatic, impactful sound. |

### 6.3 Experimental Bank

Four presets that explore the extremes of the synthesizer's capabilities. Access
by triple-clicking the Shift button.

| # | Name | Description |
|---|------|-------------|
| 0 | **Insect Swarm** | Chaotic random pitch modulation with near-self-oscillating filter. Produces metallic, buzzing textures. |
| 1 | **Depth Charge** | Sub-bass detonations. 120 Hz sine with near-infinite delay feedback. Deep, underwater rumble. |
| 2 | **Glitch Storm** | Rapid upward pitch sweeps with stuttering digital artifacts. Tight and aggressive. |
| 3 | **Foghorn From Hell** | 55 Hz drone with cathedral reverb and 4-second release. Massive, room-filling low end. |

### 6.4 Loading and Saving Presets

**Loading:** Tap the Preset button to cycle through presets in the active bank
(0 → 1 → 2 → 3 → 0).

**Saving (User Bank only):**

1. Confirm the User Bank is active.
2. Configure the desired sound parameters.
3. Hold the Preset button for 3 seconds, then release.

A triple white LED blink confirms the save. All parameters are stored, including
frequency, LFO configuration, filter settings, delay, reverb, pitch envelope
position, modulation FX, and Super Drip state.

### 6.5 Preset Library

In addition to the 12 hardware presets, the web interface provides a library of
48 additional presets organized by category: Classic Dub, NJD Style, Sci-Fi,
Modern Dub, Experimental, and Dub FX. Any library preset may be loaded into a
User Bank slot via the Presets tab.

---

## 7. Encoder Reference

Each of the five rotary encoders controls two parameters: one when Shift is not
held (Bank A) and one when Shift is held (Bank B). Slow rotation provides fine
adjustment; fast rotation engages up to 4× acceleration for rapid sweeps.

### 7.1 Bank A — Shift Not Held

| Knob | Parameter | Range | Description |
|------|-----------|-------|-------------|
| 1 | **Base Frequency** | 30 – 8,000 Hz | Oscillator pitch. Moves in semitone increments. LFO rate auto-scales with pitch. |
| 2 | **LFO Rate** | 0.1 – 20 Hz | LFO modulation speed. Lower values produce sweeping sirens; higher values produce stutter and trill effects. |
| 3 | **Filter Cutoff** | 20 Hz – 20 kHz | Low-pass filter frequency. Clockwise to brighten; counterclockwise to darken. |
| 4 | **Delay Time** | 1 ms – 1 s | Interval between delay repeats. Short values produce slapback; longer values produce spacious echoes. |
| 5 | **Delay Feedback** | 0 – 95% | Number of echo repetitions. Values above 90% will cause significant buildup. |

### 7.2 Bank B — Shift Held

| Knob | Parameter | Range | Description |
|------|-----------|-------|-------------|
| 1 | **LFO Depth** | 0 – 100% | Amplitude of LFO pitch modulation. 0% disables modulation; 100% applies full sweep. |
| 2 | **Release Time** | 10 ms – 5 s | Decay duration after Trigger release. Short values produce tight stabs; longer values produce lingering tails. |
| 3 | **Filter Resonance** | 0 – 95% | Emphasis at the cutoff frequency. Above approximately 85%, the filter begins to self-oscillate. |
| 4 | **Delay Mix** | 0 – 100% | Wet/dry balance for delay. 0% bypasses the delay; 100% outputs only the delayed signal. |
| 5 | **Reverb Mix** | 0 – 100% | Wet/dry balance for reverb. 0% is fully dry; 100% is fully wet. |

> **Note:** Encoder assignments may be remapped through the Encoders tab in the
> web interface. Any available parameter, including modulation FX controls, may
> be assigned to any encoder.

---

## 8. LFO Waveforms

The LFO (Low Frequency Oscillator) determines the modulation pattern applied to
oscillator pitch. Cycle through available waveforms by holding Shift and tapping
the Preset button. The cycle wraps from Exp Fall back to Sine.

| # | Shape | Character |
|---|-------|-----------|
| 1 | **Sine** | Smooth, musical siren sweep. |
| 2 | **Triangle** | Linear ascending and descending ramps. Slightly more defined than sine. |
| 3 | **Square** | Abrupt alternation between two pitch levels. Produces stutter effects. |
| 4 | **Ramp Up** | Pitch rises steadily, then resets. Ascending siren character. |
| 5 | **Ramp Down** | Pitch falls steadily, then resets. Descending siren character. |
| 6 | **Sample & Hold** | Random stepped pitches at the LFO rate. Produces unpredictable, chaotic tones. |
| 7 | **Exp Rise** | Exponential upward sweep: gradual start, rapid finish. |
| 8 | **Exp Fall** | Exponential downward sweep: gradual start, rapid finish. |

---

## 9. Pitch Envelope Toggle

The three-position toggle switch (located below Knob 3) determines the pitch
behavior when the Trigger button is released.

| Position | Effect |
|----------|--------|
| **Rise** (up) | Pitch slides upward by 3 octaves during the release phase. |
| **Off** (center) | No pitch change on release. The LFO continues to modulate pitch independently. |
| **Fall** (down) | Pitch slides downward by 3 octaves during the release phase. |

The low-pass filter also sweeps during the release phase (typically closing),
contributing to the characteristic dub siren decay effect.

---

## 10. Advanced Features

### 10.1 LFO-Pitch Link Toggle

By default, the LFO rate is linked to the pitch envelope. When the pitch slides
during release, the LFO speed follows. This link can be disabled.

**Activation:** Flip the pitch switch to the **Fall** position three times
rapidly (within 700 ms). Do not hold Shift.

- **Linked (default):** LFO rate scales with the pitch envelope during release,
  producing complex intertwined modulation.
- **Unlinked:** LFO rate remains constant during release. Only the pitch is
  affected.

### 10.2 Super Drip Reverb Toggle

A high-feedback spring-tank reverb mode inspired by classic dub mixing consoles.
Enabled by default at boot.

**Activation:** Hold Shift and flip the pitch switch to the **Fall** position
three times rapidly (within 700 ms).

- **On (default):** Enhanced reverb with additional feedback and compression.
  Produces a thick, characteristic spring-tank sound.
- **Off:** Standard reverb processing. Cleaner and more restrained output.

### 10.3 Quick Reference

| Feature | Activation | Default |
|---------|------------|---------|
| LFO-Pitch Link | Triple-tap Fall (without Shift) | On |
| Super Drip Reverb | Shift + Triple-tap Fall | On |

> **Note:** Both toggles are also accessible through the web interface under
> Options → Behavior.

---

## 11. Oscillator Waveforms

The oscillator waveform is configured per preset and may be changed via the Live
tab in the web interface.

| Waveform | Character |
|----------|-----------|
| **Sine** | Pure, smooth tone. Suitable for clean sub-bass applications. |
| **Square** | Hollow and crisp with rich odd harmonics. The most characteristic siren waveform. |
| **Sawtooth** | Bright and buzzy with a full harmonic spectrum. Aggressive tonal quality. |
| **Triangle** | Soft and mellow with gentle harmonic rolloff. Understated presence. |

---

## 12. Web Interface Guide

The web interface provides six tabs for comprehensive control. Access it at
**http://poorhouse.local** (refer to Section 4).

### 12.1 Live Tab

Provides real-time control of all DSP parameters. Adjustments take effect
immediately and can be heard while the Trigger button is held.

| Section | Parameters |
|---------|------------|
| Oscillator | Waveform (Sine, Square, Saw, Triangle), Frequency (30–8,000 Hz) |
| LFO | Shape (8 waveforms), Rate (0.1–20 Hz), Depth (0–100%) |
| Filter | Cutoff (20 Hz–20 kHz), Resonance (0–95%) |
| Delay | Time (1 ms–1 s), Feedback (0–95%), Mix (0–100%) |
| Reverb | Mix (0–100%) |
| Envelope | Release (10 ms–5 s), Pitch Envelope (Fall / Off / Rise) |

### 12.2 Presets Tab

Provides tools for browsing, loading, saving, and managing presets across all
banks and the extended preset library. User Bank presets can be renamed and
rearranged.

### 12.3 Encoders Tab

Allows remapping of encoder assignments. Any of the 17 available parameters
(including Phaser Mix, Chorus Mix, Flanger Mix, Saturator Mix/Drive, and Tape
Wobble/Flutter) may be assigned to any encoder in Bank A or Bank B. Sensitivity
settings control response speed, and a visual device layout displays current
assignments.

### 12.4 Options Tab

Configures effects processing, behavioral toggles, and visual appearance.

**Signal Chain (FX Order):**

| Option | Chain Order |
|--------|-------------|
| 0 (Default) | Filter → Delay → Reverb |
| 1 | Filter → Reverb → Delay |
| 2 | Delay → Filter → Reverb |
| 3 | Delay → Reverb → Filter |
| 4 | Reverb → Filter → Delay |
| 5 | Reverb → Delay → Filter |

**Reverb Types:**

| Type | Character |
|------|-----------|
| Spring | Dual-spring tank model with metallic chirp. Classic dub reverb. |
| Plate | Steel plate model (Dattorro algorithm). Smooth and dense. |
| Hall | 8-channel feedback delay network. Large, diffuse spatial character. |
| Schroeder | Classic parallel combs and allpasses. Vintage digital character. |

**Delay Types:**

| Type | Character |
|------|-----------|
| Tape | Analog-modeled with configurable wobble (~0.5 Hz) and flutter (~3.5 Hz). Warm, imperfect repeats. |
| Digital | Clean, precise repeats with no coloration. |

**Modulation Effects:** Each effect provides a 0–100% wet/dry mix control:
Phaser (sweeping comb filtering), Chorus (pitch-modulated doubling), and Flanger
(jet-sweep modulation).

**Tape Saturator:** Provides Mix (0–100%) and Drive (0–100%) controls for
harmonic saturation.

**Behavioral Toggles:** LFO-Pitch Link, Super Drip Reverb, and Filter Sweep
direction.

**LED Settings:** Master Brightness (5–100%) and Night Mode (caps brightness at
15%).

**Themes:** 16 visual themes for the web interface, divided between dark themes
(Midnight, Deep Dub, Reggae, Steppers, Roots, Irie, Kingston, Dubplate,
Soundsystem, Heavyweight) and light themes (Silver, Concrete, Driftwood,
Overcast, Bleached, Studio).

### 12.5 WiFi Tab

Manages network connectivity. Provides scanning, connection, credential testing,
and signal strength monitoring.

### 12.6 System Tab

Displays system information (firmware version, CPU/RAM usage, uptime), provides
access to firmware updates, system logs, backup and restore functions, and
device reboot controls.

---

## 13. Firmware Updates

The Poorhouse Siren supports over-the-air firmware updates via the System tab.
An active internet connection is required.

### 13.1 Update Procedure

1. Navigate to **System → Updates**.
2. Select a branch (`main` is recommended for stable releases).
3. Select **Check for Updates** to determine if a newer version is available.
4. If an update is available, select **Install**.
5. The update progresses through five stages: Backup, Fetch, Pull, Build, and
   Deploy. A progress bar and log display the current status.
6. When complete, select **Restart to Apply** to activate the new firmware.

### 13.2 Update Behavior

- User presets and settings are automatically backed up before each update.
- The device fetches the latest code from the repository, rebuilds the binary
  using CMake, and deploys it.
- On kiosk-mode systems (overlay filesystem), the new binary is deployed to
  persistent storage to survive reboots.
- The previous firmware remains active until the device is restarted.

> **Note:** Updates may take several minutes on the Pi Zero 2W.

---

## 14. Backup and Restore

### 14.1 Creating a Backup

1. Navigate to **System → Backup & Restore**.
2. Select **Download Backup**.
3. A JSON file (`dubsiren-backup.json`) is saved to the local device.

### 14.2 Restoring from Backup

1. Navigate to **System → Backup & Restore**.
2. Select **Restore** and choose a previously downloaded backup file.
3. Presets and settings are restored immediately.

### 14.3 Backup Contents

A backup file includes:

- All 4 User preset slots (names and complete parameter sets)
- Reverb type, delay type, and tape parameters (wobble, flutter)
- FX chain order and modulation FX levels (phaser, chorus, flanger, saturator)
- LFO-Pitch Link and Super Drip Reverb toggle states
- Theme selection, LED brightness, and Night Mode setting

> **Recommendation:** Create a manual backup before performing firmware updates.
> While the update process creates an automatic backup, a manual backup provides
> an additional safety measure.

---

## 15. LED Indicator Reference

The device includes a single addressable LED that communicates operating state
through color and brightness patterns.

### 15.1 Waveform Colors

The LED color corresponds to the active LFO waveform:

| LFO Waveform | LED Color |
|--------------|-----------|
| Sine | Amber |
| Triangle | Teal |
| Square | Electric Blue |
| Ramp Up | Deep Orange |
| Ramp Down | Cool Cyan |
| Sample & Hold | Bright Yellow |
| Exp Rise | Magenta |
| Exp Fall | Soft Pink |

### 15.2 Brightness Behavior

| State | Behavior |
|-------|----------|
| Active (Trigger held) | Full brightness at LFO peak, with depth-modulated dipping (15–100%). |
| Idle (Trigger released) | Base 40% brightness with gentle LFO modulation (40–75%). |
| AP Mode | White breathing pulse at approximately 0.3 Hz. |
| Preset Saved | Triple white blink confirmation. |

Master Brightness (5–100%) and Night Mode (capped at 15%) are configurable in
the Options tab.

---

## 16. Signal Flow

The complete signal path is as follows:

1. **LFO and Pitch Envelope** modulate the oscillator pitch. LFO rate optionally
   links to the pitch envelope.
2. **Oscillator** generates the waveform (Sine, Square, Sawtooth, or Triangle).
3. **Gate Envelope** applies amplitude shaping with 5 ms attack and variable
   release time.
4. **FX Chain** processes the signal through Filter (Moog 4-pole low-pass with
   envelope), Delay (Tape or Digital), and Reverb (Spring, Plate, Hall, or
   Schroeder) in one of six configurable orderings.
5. **Modulation FX** apply optional Phaser, Chorus, and/or Flanger processing.
6. **Tape Saturator** adds optional harmonic saturation.
7. **Soft Limiter** applies a warm saturation ceiling to prevent clipping.
8. **Stereo Output** at 48 kHz, 24-bit resolution.

---

## 17. Techniques and Applications

### 17.1 Classic Dub Siren Wail

Load Standard Bank Preset 0 (Slow Wail). Set the pitch toggle to Fall. Hold
Trigger and allow the LFO to sweep, then release for the descending wail.
Increase Delay Feedback (Knob 5) for extended echo tails.

### 17.2 Laser Effects

Load Standard Bank Preset 2 (Lickshot). Apply brief taps to Trigger for short
bursts. Set the pitch toggle to Fall for downward trajectories or Rise for
upward trajectories.

### 17.3 Rhythmic Stutter

Set the LFO to the Square waveform (Shift + tap Preset). Increase LFO Rate
(Knob 2) to 8–14 Hz. Set LFO Depth (Shift + Knob 1) to 80% or higher. Hold
Trigger for machine-gun burst patterns.

### 17.4 Sub-Bass Drops

Set Base Frequency (Knob 1) to 50–120 Hz. Set the pitch toggle to Fall. Set
Release Time (Shift + Knob 2) to 2–3 seconds. Hold and release Trigger for deep
frequency drops.

### 17.5 Self-Oscillating Filter

Increase Filter Resonance (Shift + Knob 3) above 85%. Sweep Filter Cutoff
(Knob 3) slowly. At high resonance, the filter produces its own pitched tones
independent of the oscillator.

### 17.6 Echo Buildup

Set Delay Feedback (Knob 5) to 90% or higher. Apply short Trigger taps. Each echo
feeds back into subsequent repetitions, building layered textures. Reduce
feedback to regain control.

### 17.7 Dub Mixdown Technique

Enable Super Drip reverb for heavy spring-tank character. Use short Trigger taps
with long release time and high reverb mix. Modulate the delay feedback knob in
real time for classic dub echo throws.

### 17.8 Modulation FX Layering

In the Options tab, add Chorus at 20–30% for stereo width. Layer Phaser at
15–25% for sweeping movement. Apply the Tape Saturator with low Drive (10–20%)
for warmth. These effects are applied after the main signal chain.

---

## 18. Troubleshooting

### 18.1 No Audio Output

- Verify the audio cable connection. Confirm that a 1/4″ cable is connected to
  the amplifier or speaker and that the destination volume is adequate.
- Confirm the Trigger button is held. Sound is produced only while Trigger is
  active.
- Check the release time. If Release Time is very short and the button is
  released quickly, the output duration may be imperceptible.
- Check the filter. If Filter Cutoff is set very low, the output may be
  inaudible. Rotate Knob 3 clockwise to open the filter.

### 18.2 Unable to Access poorhouse.local

- Confirm that the accessing device is on the same WiFi network as the Poorhouse
  Siren.
- Locate the Poorhouse Siren's IP address in the router's device list and
  navigate directly to that address.
- Some older Android devices do not support mDNS (`.local`) addresses. Use the
  IP address as a fallback.

### 18.3 AP Mode Does Not Activate

- Ensure all three buttons are held simultaneously for a full 3 seconds.
- Confirm activation by watching for the white Doppler fly-by LED animation.

### 18.4 Presets Lost on Reboot

- On kiosk-mode systems (overlay filesystem), presets are stored on persistent
  storage via a bind mount. If the mount is missing, presets will not persist.
- Use the web interface Backup & Restore feature to maintain an external copy of
  presets.

### 18.5 Web Interface Not Loading

- Verify the WiFi connection between the Poorhouse Siren and the accessing
  device.
- Attempt access from a different browser (Chrome, Firefox, Safari, or Edge
  recommended).
- Clear the browser cache. After firmware updates, cached assets may cause
  display issues. Use a hard refresh (Ctrl+Shift+R or Cmd+Shift+R).

### 18.6 Update Failure

- Confirm that the Poorhouse Siren has an active internet connection.
- Review the update log in the System tab for diagnostic information.
- Restart the Poorhouse Siren service from the System tab and attempt the update
  again.

---

## 19. Default Parameters

The following values are applied at boot:

| Parameter | Default Value |
|-----------|---------------|
| Frequency | 440 Hz (A4) |
| LFO Rate | 0.35 Hz |
| LFO Depth | 35% |
| LFO Shape | Sine |
| Filter Cutoff | 8,000 Hz |
| Filter Resonance | 0% |
| Delay Time | 375 ms |
| Delay Feedback | 55% |
| Delay Mix | 30% |
| Reverb Mix | 35% |
| Release Time | 50 ms |
| Pitch Envelope | Fall |
| Super Drip | On |
| LFO-Pitch Link | On |
| Phaser Mix | 0% (off) |
| Chorus Mix | 0% (off) |
| Flanger Mix | 0% (off) |
| Saturator Mix | 0% (off) |
| Reverb Type | Spring |
| Delay Type | Tape |
| FX Chain | Filter → Delay → Reverb |
| Bank | User |
