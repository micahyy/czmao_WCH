# czmao_WCH

CZMao DS22Lite — a 22-key macro/numpad firmware for **WCH CH552T** (TSSOP20), open-sourced.

CZMao DS22Lite —— 基于 **WCH CH552T**（TSSOP20）的 22 键宏键盘 / 小键盘固件，现已开源。

Built on `lbtrobit/CH55x_QMK_Lite` + `ch55xduino 0.0.18` + portable SDCC. Supports VIA, 4 layers, 22 WS2812B LEDs and a SignalRGB plugin.

基于 `lbtrobit/CH55x_QMK_Lite` + `ch55xduino 0.0.18` + 便携 SDCC 构建，支持 VIA、4 层键位、22 颗 WS2812B 灯，以及 SignalRGB 插件。

## USB identity / USB 身份

> VID/PID `0x4A17 / 0x4A50` are CZMao-owned IDs for the CH552T series (`0x4A51..0x4A5F` reserved). The VIA definition source is published at `micahyy.github.io/czm/definitions/czmao/CH552T/DS22Lite/DS22Lite_via.json` (vpid `1243040336`).

> VID/PID `0x4A17 / 0x4A50` 为 CZMao 自有 ID（CH552T 系列起始号，预留 `0x4A51..0x4A5F`）。VIA 定义源位于 `micahyy.github.io/czm/definitions/czmao/CH552T/DS22Lite/DS22Lite_via.json`（vpid `1243040336`）。

## Features / 功能

- **MCU**: WCH CH552T, no external crystal, USB device on a 5x5 diode matrix (BAT54C).
- **主控**：WCH CH552T，无外部晶振，5×5 二极管矩阵（BAT54C）USB 设备。
- **Layers / 层数**：4 total — Layer 0/1 are VIA-dynamic (stored in DataFlash); Layer 2/3 are compiled-in static layouts (return to Layer 0 via `TO(0)`).
- **层数 / 键位**：共 4 层 —— 层 0/1 为 VIA 动态层（存于 DataFlash），层 2/3 为编译内置静态层（按 `TO(0)` 返回层 0）。
- **VIA**: load `firmware/keyboards/DS22Lite/DS22Lite_via.json` in the VIA Design tab. Factory reset and "Enter bootloader" are one-shot buttons; after a factory reset the keyboard soft-resets (`bSW_RESET`) and USB re-enumerates, so VIA reloads the default keymap and lighting automatically.
- **VIA**：在 VIA Design 页导入 `firmware/keyboards/DS22Lite/DS22Lite_via.json`。"恢复出厂设置"与"进入 Bootloader"为一次性按钮；恢复出厂后键盘软复位（`bSW_RESET`）并 USB 重新枚举，VIA 自动重载默认键位与灯效。
- **RGB**: 22 WS2812B, 11 VIA lighting effects; VIA per-key RGB and `DS22Lite_signalrgb.js` plugin.
- **RGB**：22 颗 WS2812B，11 种 VIA 灯效；支持 VIA 单键 RGB 及 `DS22Lite_signalrgb.js` 插件。
- **Standard VIA commands / 标准命令**：`0x0A` EEPROM reset and `0x0B` bootloader jump are implemented; `EE_CLR` / `QK_BOOT` keycodes share the same path.
- **标准 VIA 命令**：`0x0A` 恢复出厂、`0x0B` 进 Bootloader 均已实现；`EE_CLR` / `QK_BOOT` 键值共用同一条路径。
- **6KRO only / 仅 6KRO**：NKRO was intentionally not enabled.
- **仅 6KRO**：未启用 NKRO（全键无冲）。
- **Media keys / mouse keys / 媒体键 / 鼠标键**：kept / 保留。

## QMK features removed (to fit 14 KB ROM) / 阉割的 QMK 功能（为压进 14 KB ROM）

| Feature | Status | Note |
|---|---|---|
| 宏 Macro (`DYNAMIC_MACRO_ENABLE`) | ❌ removed | `send_string` / `dynamic_macro` stubbed out; VIA macro menu removed |
| 复用键 / Tap Dance (TD) | ❌ removed | 5 TD enum slots kept as disabled placeholders; Tap Dance menu removed |
| 鼠标宏映射 FCODE_TO_MOUSE | ❌ removed | — |
| 编码器 Encoder | ❌ off | — |
| DIAL | ❌ off | — |
| NKRO 全键无冲 | ❌ not enabled | 6KRO only |
| Macro Loop menu | ❌ removed | — |

## Supported features / 支持的功能

| Feature | Status |
|---|---|
| VIA 2-layer dynamic + 4 physical layouts | ✅ |
| 22 WS2812B RGB + 11 effects + per-key RGB | ✅ |
| SignalRGB plugin | ✅ |
| Factory reset / bootloader buttons | ✅ |
| Standard VIA 0x0A / 0x0B + EE_CLR / QK_BOOT | ✅ |
| Media keys / mouse keys | ✅ |
| 25 custom keycodes (3 Dial + 4 layer + 5 TD placeholders + RGB) | ✅ |
| 6KRO | ✅ |

## Build (Windows) / 编译

The portable toolchain is included under `firmware/toolchain/`, so no global SDCC install is needed.

便携工具链已包含在 `firmware/toolchain/`，无需另行安装 SDCC。

```powershell
cd firmware
python build.py --clean
python verify.py
```

Output: `firmware/build/DS22Lite_CH552T.hex` (a prebuilt release hex is also included in this repo). Current build: ROM 12652 / 14336 B, XRAM 286 / 758 B, `verify.py` -> `RESULT: ALL PASS`.

产物：`firmware/build/DS22Lite_CH552T.hex`（仓库内也附预编译 hex）。当前固件：ROM 12652 / 14336 B，XRAM 286 / 758 B，`verify.py` 输出 `RESULT: ALL PASS`。

## Flash / 烧录

1. Hold the bootloader entry condition (short D+ to the specified pad / use the onboard key) while plugging in USB.
2. Use WCHISPTool or `vnproch55x` to flash `firmware/build/DS22Lite_CH552T.hex`.
3. Unplug and replug. On first boot after a firmware layout change, `VIA_MAGIC_CODE = 0x5C` rebuilds the DataFlash defaults.

1. 按住进入 Bootloader 的条件（短接 D+ 到指定焊盘 / 使用板载按键）后插入 USB。
2. 用 WCHISPTool 或 `vnproch55x` 烧录 `firmware/build/DS22Lite_CH552T.hex`。
3. 拔出再插入。固件布局变更后首次上电，`VIA_MAGIC_CODE = 0x5C` 会重建 DataFlash 默认值。

## Layout of this repository / 目录结构

```
firmware/src/         QMK-Lite / VIA / RGB / USB HID sources
firmware/keyboards/   DS22Lite config, VIA JSON, SignalRGB plugin, upstream example keyboards
firmware/toolchain/   portable SDCC + ch55xduino 0.0.18 (headers, libs, examples, Windows binaries)
firmware/build/       prebuilt release hex + .mem + patched core source; obj/ is generated and excluded
firmware/image/       upstream documentation screenshots
hardware/             OpenSCAD / PCB / netlist sources (binary STEP, STL and renders are excluded)
docs/                 feature-cut notes and changelog
```

`firmware/README.md` is the upstream framework README (kept for reference). `docs/DS22Lite-feature-cuts.md` explains in detail why macros, reusable keys and NKRO were cut to fit the CH552.

`firmware/README.md` 为上游框架说明（保留参考）。`docs/DS22Lite-feature-cuts.md` 详细解释了为适配 CH552 而裁剪宏、复用键与 NKRO 的原因。

## License / attribution / 许可证

This repository redistributes third-party GPL/LGPL code; see `LICENSE` (LGPL-2.1 full text) and `NOTICE.md` for per-component attribution. Upstream file headers remain authoritative.

本仓库再分发第三方 GPL/LGPL 代码；组件归属见 `LICENSE`（完整 LGPL-2.1 文本）与 `NOTICE.md`，上游文件头以原始为准。

## Status / 状态

The firmware builds and passes all static/structural checks. The deferred soft-reset path after VIA factory reset has not yet been validated on physical hardware; test that path first after flashing.

固件可编译并通过全部静态/结构检查。VIA 恢复出厂后的延迟软复位路径尚未在真机验证，烧录后请优先测试该路径。
