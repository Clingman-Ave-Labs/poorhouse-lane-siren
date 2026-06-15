# Dub Siren — User Manual

A Raspberry Pi-powered dub siren synthesizer.

---

## Table of Contents

1. [Overview](#overview)
2. [Controls at a Glance](#controls-at-a-glance)
3. [Getting Sound Out](#getting-sound-out)
4. [The Three Banks](#the-three-banks)
5. [Presets](#presets)
6. [Knob Reference](#knob-reference)
7. [LFO Waveforms](#lfo-waveforms)
8. [Pitch Envelope Toggle](#pitch-envelope-toggle-position-6)
9. [Secret Combinations](#secret-combinations)
10. [Signal Flow](#signal-flow)
11. [Tips & Techniques](#tips--techniques)

---

## Overview

The dub siren is a single-voice synthesizer built for live dub and reggae
sound-system use. It produces classic siren wails, laser shots, machine-gun
stutters, and deep sub-bass effects through a chain of:

**Oscillator → Filter → Tape Delay → Spring Reverb → Output**

Everything runs on a Raspberry Pi Zero 2W at 48 kHz / 24-bit.

---

## Controls at a Glance

The device has **three buttons**, **five rotary encoders** (knobs), and a
**three-position pitch envelope toggle** (position 6, located under Knob 3).

```
┌──────────────────────────────────────────┐
│                                          │
│   [Knob 1]  [Knob 2]  [Knob 3]          │
│                                          │
│   [Knob 4]  [Knob 5]  ┌─ Rise ─┐        │
│                        │  Off   │ (6)    │
│                        └─ Fall ─┘        │
│                                          │
│   (PRESET)   (BANK)   (TRIGGER)          │
│                                          │
└──────────────────────────────────────────┘
```

| Control | What It Does |
|---------|-------------|
| **Preset** button (left) | Tap to change presets. Hold Bank + tap to change LFO shape. |
| **Bank** button (center) | Hold to access the second layer of knob parameters. Double/triple-click to switch banks. |
| **Trigger** button (right) | Hold to make sound. Release to let it ring out. |
| **Pitch toggle** (pos. 6) | Three positions: Rise / Off / Fall — controls how pitch bends on release. |
| **Knobs 1–5** | Rotary encoders that control different parameters depending on whether Bank is held. |

---

## Getting Sound Out

1. **Power on** the device. It boots into the User bank with default settings.
2. **Press and hold the Trigger button.** You should hear a siren tone.
3. **Release Trigger.** The tone fades out over the release time.
4. **Turn knobs** to shape the sound while holding Trigger.

That's it — hold Trigger to play, release to stop. Everything else is about
shaping what happens between press and release.

---

## The Three Banks

The siren has three banks of presets, each containing four presets.

### User Bank (default on boot)

Your own saved sounds. Starts with copies of the factory presets that you can
tweak and overwrite.

**How to access:** This is where you start. If you've switched away,
double-click Bank to return.

### Standard Bank (factory presets)

Four classic dub siren sounds. These cannot be overwritten.

**How to access:** Double-click the Bank button (two quick clicks within
half a second).

| # | Name | Character |
|---|------|-----------|
| 0 | **Slow Wail** | The classic — a 10-second sweeping siren. Sawtooth oscillator with sine LFO. Deep delay and long reverb tail. |
| 1 | **Machine Gun** | Rapid-fire stutter bursts. Square LFO chopping a square wave at 14 Hz. Percussive and aggressive. |
| 2 | **Lickshot** | Laser-gun zaps. Triangle LFO on a square wave with a falling pitch. Fast and punchy. |
| 3 | **Droppa** | Dramatic descending wail. Ramp-down LFO with heavy delay feedback. Drops hard. |

### Experimental Bank (extreme sounds)

Four wild presets that push the synth to its limits.

**How to access:** Triple-click the Bank button (three quick clicks within
half a second).

| # | Name | Character |
|---|------|-----------|
| 0 | **Insect Swarm** | Chaotic random pitch jumps with near-self-oscillating filter. Metallic buzzing insect cloud. |
| 1 | **Depth Charge** | Sub-bass explosions. 120 Hz sine with near-infinite delay feedback. Underwater thunder. |
| 2 | **Glitch Storm** | Frantic upward pitch sweeps with stuttering digital chaos. Tight and aggressive. |
| 3 | **Foghorn From Hell** | Monstrous 55 Hz drone with cathedral reverb and 4-second release. Fills the room. |

---

## Presets

### Loading a Preset

**Tap the Preset button** to cycle through presets in the current bank:
0 → 1 → 2 → 3 → 0 → ...

### Saving a Preset (User Bank only)

1. Make sure you are in the **User Bank**.
2. Dial in the sound you want.
3. **Hold the Preset button for 3 full seconds**, then release.
4. The current state is saved to the active preset slot.

Everything is saved: frequency, LFO shape and rate, filter settings, delay,
reverb, pitch envelope position, and Super Drip on/off.

> **Note:** On systems running in kiosk/overlay mode, saved presets may be lost
> on power cycle unless persistent storage is mounted.

---

## Knob Reference

Each of the five rotary encoders controls two parameters — one when Bank is
**not held** (Layer A), and one when Bank **is held** (Layer B).

Spin a knob slowly for fine control. Spin fast for rapid sweeps (up to 4×
acceleration).

### Layer A — Bank NOT held

| Knob | Parameter | Range | What It Does |
|------|-----------|-------|-------------|
| 1 | **Base Frequency** | 30 – 8,000 Hz | The pitch of the oscillator. Moves in semitone steps. LFO rate auto-scales with pitch. |
| 2 | **LFO Rate** | 0.1 – 20 Hz | How fast the LFO wobbles the pitch. Slow = sweeping siren. Fast = stutter/trill. |
| 3 | **Filter Cutoff** | 20 Hz – 20 kHz | Opens/closes the low-pass filter. Turn right to brighten, left to darken. |
| 4 | **Delay Time** | 1 ms – 1 second | Time between delay repeats. Short = slapback. Long = spacious echoes. |
| 5 | **Delay Feedback** | 0 – 95% | How many times the echo repeats. High values build up — careful near 90%+. |

### Layer B — Bank HELD

| Knob | Parameter | Range | What It Does |
|------|-----------|-------|-------------|
| 1 | **LFO Depth** | 0 – 100% | How much the LFO moves the pitch. 0% = no wobble. 100% = full sweep. |
| 2 | **Release Time** | 10 ms – 5 seconds | How long the sound rings after you release Trigger. Short = tight stabs. Long = lingering wails. |
| 3 | **Filter Resonance** | 0 – 95% | Emphasizes frequencies at the cutoff point. Above ~85%, the filter starts to self-oscillate and sing. |
| 4 | **Delay Mix** | 0 – 100% | Wet/dry balance for the delay. 0% = no delay heard. 100% = all echo. |
| 5 | **Reverb Mix** | 0 – 100% | Wet/dry balance for the reverb. 0% = dry. 100% = fully drenched. |

---

## LFO Waveforms

The LFO (Low Frequency Oscillator) shapes how the pitch wobbles over time.
Cycle through waveforms by holding **Bank + tapping Preset**.

| # | Shape | Character |
|---|-------|-----------|
| 1 | **Sine** | Smooth, musical siren sweep. The classic. |
| 2 | **Triangle** | Linear ramps up and down. Slightly more edgy than sine. |
| 3 | **Square** | Hard chop between two pitches. On/off stutter. |
| 4 | **Ramp Up** | Pitch rises steadily, then snaps back down. Ascending siren. |
| 5 | **Ramp Down** | Pitch falls steadily, then snaps back up. Descending siren. |
| 6 | **Sample & Hold** | Random stepped pitches at LFO rate. Chaotic robot sounds. |
| 7 | **Exp Rise** | Slow start, fast finish upward. Dramatic builds. |
| 8 | **Exp Fall** | Slow start, fast finish downward. Dramatic drops. |

The cycle wraps: after Exp Fall, it returns to Sine.

---

## Pitch Envelope Toggle (Position 6)

The three-position toggle (located under Knob 3) controls what happens to the
pitch when you **release** the Trigger button:

| Position | Effect |
|----------|--------|
| **Rise** (up) | Pitch slides UP 3 octaves during release. Tone soars away. |
| **Off** (center) | No pitch change on release. Let the LFO do the work. |
| **Fall** (down) | Pitch slides DOWN 3 octaves during release. Classic descending siren. |

The filter also sweeps during release (usually darkening the sound), creating
the characteristic dub siren "dive" effect.

---

## Secret Combinations

### 1. LFO-Pitch Link Toggle

By default, the LFO rate is linked to the pitch envelope — when the pitch
slides on release, the LFO speed follows along. You can unlink them.

**How to activate:**
> Flip the pitch switch to the **Fall** position three times quickly
> (triple-tap within 700 ms). Do **not** hold Bank.

- **Linked (default):** LFO rate scales with the pitch envelope during release,
  creating complex intertwined modulation.
- **Unlinked:** LFO rate stays constant during release. Only the pitch moves.

### 2. Super Drip Reverb Toggle

A heavy spring-tank reverb mode inspired by classic dub mixing desks. Enabled
by default on boot.

**How to activate:**
> **Hold Bank** and flip the pitch switch to the **Fall** position three
> times quickly (triple-tap within 700 ms).

- **On (default):** Enhanced reverb with extra feedback and compression.
  Thick, splashy spring sound.
- **Off:** Standard reverb. Cleaner and more restrained.

### Quick Reference

| Secret | How | Default |
|--------|-----|---------|
| LFO-Pitch Link | Triple-tap Fall (no Bank) | ON |
| Super Drip Reverb | Bank + Triple-tap Fall | ON |

---

## Signal Flow

```
 Trigger Button (gate)
       │
       ▼
 ┌───────────┐     ┌───────────┐
 │    LFO    │────▶│   Base    │
 │ (8 shapes)│     │ Frequency │
 └───────────┘     └─────┬─────┘
                         │
                   ┌─────▼─────┐
                   │   Pitch   │
                   │ Envelope  │
                   └─────┬─────┘
                         │
                   ┌─────▼─────┐
                   │ Oscillator│  (Sine / Square / Saw / Triangle)
                   └─────┬─────┘
                         │
                   ┌─────▼─────┐
                   │  Filter   │  (Moog 4-pole low-pass)
                   │ + Envelope│  (sweeps on release)
                   └─────┬─────┘
                         │
                   ┌─────▼─────┐
                   │  Volume   │  (5 ms attack, variable release)
                   │ Envelope  │
                   └─────┬─────┘
                         │
                   ┌─────▼─────┐
                   │   Delay   │  (Tape-style with wobble)
                   └─────┬─────┘
                         │
                   ┌─────▼─────┐
                   │  Reverb   │  (Dual spring tank)
                   │± Super Drip│
                   └─────┬─────┘
                         │
                   ┌─────▼─────┐
                   │   Soft    │  (Warm saturation limiter)
                   │  Limiter  │
                   └─────┬─────┘
                         │
                         ▼
                   Stereo Output
                  (48 kHz, 24-bit)
```

---

## Tips & Techniques

### Classic Dub Siren Wail
- Start with Standard Bank, Preset 0 (Slow Wail).
- Pitch switch to **Fall**.
- Hold Trigger, let it sweep, release for the descending wail.
- Turn up delay feedback (Knob 5) for spacious echoes.

### Laser Shots
- Load Preset 2 (Lickshot) from Standard Bank.
- Tap Trigger quickly for short zaps.
- Pitch switch to **Fall** for downward lasers or **Rise** for upward ones.

### Rhythmic Stutter
- Set LFO to **Square** (Bank + tap Preset until Square).
- Crank LFO Rate (Knob 2) to 8–14 Hz.
- Set LFO Depth (Bank + Knob 1) to 80%+.
- Hold Trigger for machine-gun bursts.

### Sub-Bass Drops
- Turn Base Frequency (Knob 1) down to 50–120 Hz.
- Pitch switch to **Fall**.
- Set Release Time (Bank + Knob 2) to 2–3 seconds.
- Hold and release Trigger for massive drops.

### Self-Oscillating Filter
- Turn Filter Resonance (Bank + Knob 3) above 85%.
- Sweep Filter Cutoff (Knob 3) slowly.
- The filter becomes its own oscillator — pure singing tones.

### Infinite Echo Buildup
- Set Delay Feedback (Knob 5) to 90%+.
- Play short Trigger taps.
- Echoes build on each other, creating walls of sound.
- Pull feedback back down to rein it in.

### Dub Mixdown Style
- Enable Super Drip reverb (Bank + triple-tap Fall) for heavy spring tank.
- Use short Trigger taps with long release and high reverb mix.
- Ride the delay feedback knob in real time for dub-style echo throws.

### Chaos Mode
- Switch to Experimental Bank (triple-click Bank).
- Load Insect Swarm or Glitch Storm.
- Everything is extreme — small knob changes make big differences.

---

## Oscillator Waveforms

The oscillator waveform is set per-preset and not directly changeable via a
knob. Load different presets to get different oscillator characters:

| Waveform | Character |
|----------|-----------|
| **Sine** | Pure, smooth tone. Clean sub-bass. |
| **Square** | Hollow and crisp. Rich in odd harmonics. The most "siren-like." |
| **Sawtooth** | Buzzy and bright. Full harmonic spectrum. Aggressive. |
| **Triangle** | Soft and mellow. Harmonics roll off gently. Understated. |

---

## Defaults on Boot

| Parameter | Value |
|-----------|-------|
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
| Super Drip | ON |
| LFO-Pitch Link | ON |
| Bank | User |
