# czmao_WCH

CZMao DS22Lite — a 22-key macro/numpad firmware for **WCH CH552T** (TSSOP20).

CZMao DS22Lite —— 基于 **WCH CH552T**（TSSOP20）的 22 键宏键盘 / 小键盘固件。

Current firmware is **v2**: a from-scratch lightweight keyboard firmware that no
longer depends on the QMK framework. It reuses only the chip SDK (ch55xduino
core + portable SDCC + WS2812 asm) under `firmware/toolchain/`. The older
QMK-Lite based v1 has been removed.

当前固件为 **v2**：从零实现、不依赖 QMK 的轻量固件，只复用 `firmware/toolchain/`
下的芯片 SDK（ch55xduino core + 便携 SDCC + WS2812 汇编库）。旧的 QMK-Lite v1 已移除。

## USB identity / USB 身份

VID `0x4A17`, PID `0x4A51` (Manufacturer `Maiku`, Product `DS22Lite v2`).
The VIA definition is `firmware/v2/DS22Lite_v2_via.json` (import via the VIA
Design tab).

VID `0x4A17`，PID `0x4A51`。VIA 定义为 `firmware/v2/DS22Lite_v2_via.json`。

## Features / 功能

- 4 layers, all VIA-editable; keymap + RGB + macros persist in the 128 B DataFlash.
- 22 WS2812B LEDs, VIA lighting effects and per-layer colors.
- Dynamic macros (8 slots), VIA commands `0x0C`~`0x10`, power-off persistent.
- Media / consumer keys, mouse keys and `F13`~`F21`→mouse, dial HID report.
- Standard VIA commands `0x0A` (EEPROM reset) and `0x0B` (bootloader jump); `6KRO`.
- 4 层全部可在 VIA 改；键表 + 灯效 + 宏掉电保持于 128 B DataFlash。
- 22 颗 WS2812B，VIA 灯效 + 分层配色；动态宏 8 条；媒体键 / 鼠标键 / 拨盘。

Not included (by design): Tap Dance, NKRO, encoder / Surface Dial hardware.

未包含（设计如此）：Tap Dance、NKRO、编码器 / 拨盘硬件。

## Build (Windows) / 编译

The portable toolchain is included under `firmware/toolchain/`, so no global SDCC
install is needed.

```powershell
cd firmware/v2
python build.py --clean
python verify.py
```

Output: `firmware/v2/build/DS22Lite_v2.hex` (a prebuilt release hex + `.mem` are
included). See `firmware/v2/README.md` for build variants and resource usage.

产物：`firmware/v2/build/DS22Lite_v2.hex`（仓库内附预编译 hex 与 `.mem`）。
构建档位与资源占用详见 `firmware/v2/README.md`。

## Flash / 烧录

Flash `firmware/v2/build/DS22Lite_v2.hex` with WCHISPTool (or `vnproch55x`).

The firmware can enter the CH552 on-chip USB ISP with VIA command `0x0B`
("进入升级模式"), which does a software `lcall #0x3800` into the ROM bootloader —
no case opening or pin shorting. Unlike CH582-based boards, CH552 has no BLE /
dual-bank, so silent drag-and-drop self-flash over WebHID is not supported.

用 WCHISPTool（或 `vnproch55x`）烧录 `firmware/v2/build/DS22Lite_v2.hex`。固件支持
VIA 命令 `0x0B`「进入升级模式」软件跳转进 CH552 片内 ROM USB ISP，无需开盖 / 短接；
但 CH552 无 BLE / 双分区，不支持 CH582 那种拖拽文件静默自刷。

## Layout of this repository / 目录结构

```
firmware/v2/          v2 固件源码、构建/校验脚本、VIA 定义、预编译 hex
firmware/toolchain/   portable SDCC + ch55xduino + WS2812 (shared build toolchain)
hardware/             OpenSCAD / PCB / netlist sources (binary STEP/STL excluded)
docs/                 feature-scope notes and changelog
```

## License / attribution / 许可证

This repository redistributes third-party GPL/LGPL code; see `LICENSE` (LGPL-2.1
full text) and `NOTICE.md`. Upstream file headers remain authoritative.

本仓库再分发第三方 GPL/LGPL 代码；见 `LICENSE`（LGPL-2.1 全文）与 `NOTICE.md`。
