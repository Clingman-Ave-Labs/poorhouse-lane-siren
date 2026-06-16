# Poorhouse Lane Siren

A standalone dub siren synthesizer for the Raspberry Pi Zero 2 W, with a
built-in Wi-Fi config portal, OTA updates, and an I2S DAC audio path. This is
the **v1.0** release from Clingman Ave Labs.

## Fresh Install (one-liner)

Run on a fresh Raspberry Pi to install everything. Prompts for autostart and
kiosk mode:

```
curl -fsSL https://raw.githubusercontent.com/Clingman-Ave-Labs/poorhouse-lane-siren/main/setup.sh | sudo bash
```

See [CLAUDE.md](CLAUDE.md) for testing, health-check, and deployment notes.

## Documentation

- [Build Manual](docs/BUILD_MANUAL.md) — bill of materials, PCB/enclosure files,
  GPIO wiring reference, and hardware assembly.
- [User Manual](docs/USER_MANUAL.md) — operation, controls, presets, web
  interface, and first-install guide.

## Licensing

This project is **source-available**, not OSI "open source" — the
commercial-use restriction below is incompatible with the Open Source
Definition.

- **Non-commercial use** (personal, hobby, research, educational) is **free**
  under the [PolyForm Noncommercial License 1.0.0](LICENSE).
- **Commercial use** requires a separate **paid commercial license**. To obtain
  one, contact **parkredding@gmail.com**.

See the [LICENSE](LICENSE) file for the full terms.

## Credits

- The parametric enclosure is based on the
  [Parametric Junction Box — Modular, Scalable](https://makerworld.com/en/models/2219219-parametric-junction-box-modular-scalable)
  model on MakerWorld, adapted for the Poorhouse Siren. Please honor the
  original model's license terms.
