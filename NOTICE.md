# Third-party notices

The v2 firmware (`firmware/v2/`) is a from-scratch keyboard implementation by
this project. It does **not** ship QMK / CH55x_QMK_Lite sources; it links against
the chip SDK below and reimplements the keyboard/VIA/RGB logic itself.

- **ch55xduino 0.0.18** (Deqing Sun / ch55xduino contributors): LGPL-2.1, the
  core and headers under `firmware/toolchain/ch55x/`.
- **WS2812 ch55xduino library**: `firmware/toolchain/ch55x/libraries/WS2812/`.
- **SDCC** (Small Device C Compiler): GPL-2.0-or-later, portable Windows binaries
  and libraries under `firmware/toolchain/sdcc/`.

CZMao-specific firmware (board config, 4-layer keymap, 22-LED mapping, macro
persistence, VIA protocol, build/verify scripts) is released under LGPL-2.1
(see `LICENSE`). Upstream per-file headers of the toolchain remain authoritative.
