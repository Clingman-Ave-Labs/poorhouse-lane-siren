# Poorhouse Lane Siren

A standalone dub siren synthesizer for the Raspberry Pi Zero 2 W, with a
built-in Wi-Fi config portal, OTA updates, and an I2S DAC audio path. This is
the **v1.0** release from Clingman Ave Labs.

<img width="4032" height="3024" alt="IMG_0279" src="https://github.com/user-attachments/assets/0f46bd5e-63da-4707-a0ce-b23d96914094" />


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

This project is **open source**, licensed under the
[Apache License 2.0](LICENSE). You are free to use, modify, and distribute it —
including for commercial purposes — subject to the terms of the license, which
include preserving attribution and the license notice. The Apache 2.0 license
also provides an express patent grant.

See the [LICENSE](LICENSE) file for the full terms.

## Enclosure

The 3D-printable enclosure is hosted on MakerWorld:
[Poorhouse Lane Siren Enclosure](https://makerworld.com/en/models/2940774-poorhouse-lane-siren-enclosure).

It is a derivative work of the
[Parametric Junction Box](https://makerworld.com/en/models/2219219-parametric-junction-box-modular-scalable)
by **iLab**. Per the original's MakerWorld Exclusive License, derivative works
must be hosted only on MakerWorld, so the model files are not included in this
repository — download them from the link above.
