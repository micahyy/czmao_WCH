# Changelog

## 2026-11-12 - initial open-source release

- Factory reset fix: VIA standard commands `0x0A` / `0x0B` are now implemented; `EE_CLR` (`QK_CLEAR_EEPROM`) and `QK_BOOT` share the same system-keycode path.
- Factory reset no longer runs 105 DataFlash writes inside the HID callback: the reply is sent first, then `via_task()` rebuilds EEPROM and soft-resets with `bSW_RESET` so USB re-enumerates and VIA reloads defaults.
- VIA System controls changed from `toggle` to one-shot `button`; GET returns 0 and `CUSTOM_MENU_SAVE` is swallowed.
- Build: `DS22Lite_CH552T.hex` 30849 B, ROM 12652 / 14336 B, XRAM 286 / 758 B, `verify.py` ALL PASS.