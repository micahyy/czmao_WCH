# Changelog

## 2026-11-12 - initial open-source release

- Factory reset fix: VIA standard commands `0x0A` / `0x0B` are now implemented; `EE_CLR` (`QK_CLEAR_EEPROM`) and `QK_BOOT` share the same system-keycode path.
- Factory reset no longer runs 105 DataFlash writes inside the HID callback: the reply is sent first, then `via_task()` rebuilds EEPROM and soft-resets with `bSW_RESET` so USB re-enumerates and VIA reloads defaults.
- VIA System controls changed from `toggle` to one-shot `button`; GET returns 0 and `CUSTOM_MENU_SAVE` is swallowed.
- Build: `DS22Lite_CH552T.hex` 30849 B, ROM 12652 / 14336 B, XRAM 286 / 758 B, `verify.py` ALL PASS.

## 2026-11-12 - USB ID allocation

- Replaced borrowed `0x5262/0x4443` with CZMao-owned `0x4A17/0x4A50` (vpid `1243040336`).
- Checked all 55 VID/PID pairs in the VIA definitions source: `0x4A5x` is unused and the vpid is absent from `supported_kbs.json` and v3 outputs.
- Added the CH552T series folder in the VIA definitions repo; reserved `0x4A51..0x4A5F` for future boards.
- Rebuilt hex: ROM 12652 / 14336 B, XRAM 286 / 758 B; `verify.py` ALL PASS.
