# czmao_WCH

CZMao DS22Lite 22-key macro/numpad firmware for **WCH CH552T** (TSSOP20), open-sourced.

Built on `lbtrobit/CH55x_QMK_Lite` + `ch55xduino 0.0.18` + portable SDCC. Supports VIA, 4 layers, 22 WS2812B LEDs and a SignalRGB plugin.

## Features

- **MCU**: WCH CH552T, no external crystal, USB device on a 5x5 diode matrix (BAT54C).
- **Layers**: 4 total. Layer 0/1 are VIA-dynamic (stored in DataFlash), Layer 2/3 are compiled-in static layouts.
- **VIA**: load `firmware/keyboards/DS22Lite/DS22Lite_via.json` in the VIA Design tab. Factory reset and "Enter bootloader" are one-shot buttons; after a factory reset the keyboard soft-resets (`bSW_RESET`) and USB re-enumerates, so VIA reloads the default keymap and lighting automatically.
- **RGB**: 22 WS2812B, 11 VIA lighting effects; VIA per-key RGB and `DS22Lite_signalrgb.js` plugin.
- **Standard VIA commands**: `0x0A` EEPROM reset and `0x0B` bootloader jump are implemented; `EE_CLR` / `QK_BOOT` keycodes share the same path.
- **6KRO only**: NKRO / mouse-key macro mappings were intentionally removed to fit 14 KB ROM. Two VIA macro slots remain (4 actions each).

> VID/PID `0x5262 / 0x4443` are temporary values borrowed from the upstream framework; replace them before mass production.

## Build (Windows)

The portable toolchain is included under `firmware/toolchain/`, so no global SDCC install is needed.

```powershell
cd firmware
python build.py --clean
python verify.py
```

Output: `firmware/build/DS22Lite_CH552T.hex` (a prebuilt release hex is also included in this repo). Current build: ROM 12652 / 14336 B, XRAM 286 / 758 B, `verify.py` -> `RESULT: ALL PASS`.

## Flash

1. Hold the bootloader entry condition (short D+ to the specified pad / use the onboard key) while plugging in USB.
2. Use WCHISPTool or `vnproch55x` to flash `firmware/build/DS22Lite_CH552T.hex`.
3. Unplug and replug. On first boot after a firmware layout change, `VIA_MAGIC_CODE = 0x5C` rebuilds the DataFlash defaults.

## Layout of this repository

```
firmware/src/         QMK-Lite / VIA / RGB / USB HID sources
firmware/keyboards/   DS22Lite config, VIA JSON, SignalRGB plugin, upstream example keyboards
firmware/toolchain/   portable SDCC + ch55xduino 0.0.18 (headers, libs, examples, Windows binaries)
firmware/build/       prebuilt release hex + .mem + patched core source; obj/ is generated and excluded
firmware/image/       upstream documentation screenshots
hardware/             OpenSCAD / PCB / netlist sources (binary STEP, STL and renders are excluded)
docs/                 feature-cut notes and changelog
```

`firmware/README.md` is the upstream framework README (kept for reference). `docs/DS22Lite-feature-cuts.md` explains why macros, reusable keys and NKRO were cut to fit the CH552.

## License / attribution

This repository redistributes third-party GPL/LGPL code; see `LICENSE` (LGPL-2.1 full text) and `NOTICE.md` for per-component attribution. Upstream file headers remain authoritative.

## Status

The firmware builds and passes all static/structural checks. The deferred soft-reset path after VIA factory reset has not yet been validated on physical hardware; test that path first after flashing.