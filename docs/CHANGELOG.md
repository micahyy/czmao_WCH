# Changelog

## 2026-11-13 — v1.0.1

- Updated the USB identity to `0x4A17 / 0x4A50` and the corresponding VIA definition.
- Reorganised the VIA definition sources into a per-chip series folder.
- Refreshed documentation (bilingual README, feature scope notes).
- Rebuilt the release hex and refreshed the source archive.

## 2026-11-12 — v1.0.0

- Initial open-source release: DS22Lite CH552T firmware, VIA definition, SignalRGB plugin and portable toolchain.
- Implemented VIA factory reset and bootloader controls (`0x0A` / `0x0B`, `EE_CLR` / `QK_BOOT`).
