# czmao_WCH

CZMao DS22Lite — a 22-key macro/numpad firmware for **WCH CH552T** (TSSOP20), open-sourced.

CZMao DS22Lite —— 基于 **WCH CH552T**（TSSOP20）的 22 键宏键盘 / 小键盘固件，现已开源。

Built on `lbtrobit/CH55x_QMK_Lite` + `ch55xduino 0.0.18` + portable SDCC. Supports VIA, 4 layers, 22 WS2812B LEDs and a SignalRGB plugin.

基于 `lbtrobit/CH55x_QMK_Lite` + `ch55xduino 0.0.18` + 便携 SDCC 构建，支持 VIA、4 层键位、22 颗 WS2812B 灯，以及 SignalRGB 插件。

## USB identity / USB 身份

VID `0x4A17`, PID `0x4A50`. The VIA definition is available at `micahyy.github.io/czm/definitions/czmao/CH552T/DS22Lite/DS22Lite_via.json`.

VID `0x4A17`，PID `0x4A50`。VIA 定义见 `micahyy.github.io/czm/definitions/czmao/CH552T/DS22Lite/DS22Lite_via.json`。

## Features / 功能

- **MCU**: WCH CH552T, no external crystal, USB device on a 5x5 diode matrix (BAT54C).
- **主控**：WCH CH552T，无外部晶振，5×5 二极管矩阵（BAT54C）USB 设备。
- **Layers / 层数**：4 total — Layer 0/1 are VIA-dynamic (stored in DataFlash); Layer 2/3 are compiled-in layouts (return to Layer 0 via `TO(0)`).
- **层数 / 键位**：共 4 层 —— 层 0/1 为 VIA 动态层（存于 DataFlash），层 2/3 为编译内置键位（按 `TO(0)` 返回层 0）。
- **VIA**: load `firmware/keyboards/DS22Lite/DS22Lite_via.json` in the VIA Design tab. Factory reset and "Enter bootloader" are available as one-shot buttons.
- **VIA**：在 VIA Design 页导入 `firmware/keyboards/DS22Lite/DS22Lite_via.json`；「恢复出厂设置」与「进入 Bootloader」为一次性按钮。
- **RGB**: 22 WS2812B, VIA lighting effects, per-key RGB and the `DS22Lite_signalrgb.js` plugin.
- **RGB**：22 颗 WS2812B，支持 VIA 灯效、单键 RGB 及 `DS22Lite_signalrgb.js` 插件。
- **Standard VIA commands / 标准命令**：`0x0A` EEPROM reset, `0x0B` bootloader jump, and the `EE_CLR` / `QK_BOOT` keycodes are supported.
- **标准 VIA 命令**：支持 `0x0A` 恢复出厂、`0x0B` 进 Bootloader，以及 `EE_CLR` / `QK_BOOT` 键值。
- **6KRO**: standard boot keyboard report.
- **6KRO**：标准引导键盘报告。
- **Media keys / mouse keys / 媒体键 / 鼠标键**：supported / 支持。

## Feature scope / 功能范围

The CH552T has limited on-chip ROM and DataFlash, so this firmware implements a focused subset of QMK/VIA. The following desktop-QMK features are not included:

CH552T 片内 ROM 与 DataFlash 有限，固件实现的是 QMK/VIA 的精简子集。以下桌面版 QMK 功能未包含：

| Feature | Status | Note |
|---|---|---|
| 宏 Macro (`DYNAMIC_MACRO_ENABLE`) | 未包含 / Not included | 宏数据与回放代码超出片内容量 |
| 复用键 / Tap Dance | 未包含 / Not included | 同上 |
| 鼠标宏映射 / FCODE_TO_MOUSE | 未包含 / Not included | 依赖宏功能 |
| 编码器 / Encoder | 未启用 / Not enabled | 板上未配备 |
| DIAL / Surface Dial | 未启用 / Not enabled | 板上未配备 |
| NKRO / 全键无冲 | 未启用 / Not enabled | 使用标准 6KRO 报告 |
| VIA Macro Loop / Tap Dance 菜单 | 未包含 / Not included | 随对应功能一并省略 |

The VIA macro page is empty and the related menus are absent by design — this is expected, not an error.

VIA 宏页面为空、相关菜单不出现属于预期行为，并非故障。

## Supported features / 支持的功能

| Feature | Status |
|---|---|
| VIA 双层动态 + 4 套键位 / VIA dynamic layers + 4 layouts | ✅ |
| 22 WS2812B RGB + VIA 灯效 + 单键 RGB / 22 WS2812B RGB + VIA lighting + per-key RGB | ✅ |
| SignalRGB plugin / SignalRGB 插件 | ✅ |
| 恢复出厂 / 进 Bootloader 按钮 / Factory reset & bootloader buttons | ✅ |
| 标准 VIA 0x0A / 0x0B + EE_CLR / QK_BOOT | ✅ |
| 媒体键 / 鼠标键 / Media & mouse keys | ✅ |
| 层切换与灯效自定义键码 / Custom layer & lighting keycodes | ✅ |
| 6KRO | ✅ |

## Build (Windows) / 编译

The portable toolchain is included under `firmware/toolchain/`, so no global SDCC install is needed.

便携工具链已包含在 `firmware/toolchain/`，无需另行安装 SDCC。

```powershell
cd firmware
python build.py --clean
python verify.py
```

Output: `firmware/build/DS22Lite_CH552T.hex` (a prebuilt release hex is also included). Current build: ROM 12652 / 14336 B, XRAM 286 / 758 B.

产物：`firmware/build/DS22Lite_CH552T.hex`（仓库内也附预编译 hex）。当前固件：ROM 12652 / 14336 B，XRAM 286 / 758 B。

## Flash / 烧录

1. Hold the bootloader entry condition (short D+ to the specified pad / use the onboard key) while plugging in USB.
2. Use WCHISPTool or `vnproch55x` to flash `firmware/build/DS22Lite_CH552T.hex`.
3. Unplug and replug.

1. 按住进入 Bootloader 的条件（短接 D+ 到指定焊盘 / 使用板载按键）后插入 USB。
2. 用 WCHISPTool 或 `vnproch55x` 烧录 `firmware/build/DS22Lite_CH552T.hex`。
3. 拔出再插入。

## Layout of this repository / 目录结构

```
firmware/src/         QMK-Lite / VIA / RGB / USB HID sources
firmware/keyboards/   DS22Lite config, VIA JSON, SignalRGB plugin, upstream example keyboards
firmware/toolchain/   portable SDCC + ch55xduino 0.0.18 (headers, libs, examples, Windows binaries)
firmware/build/       prebuilt release hex + .mem; obj/ is generated and excluded
firmware/image/       upstream documentation screenshots
hardware/             OpenSCAD / PCB / netlist sources (binary STEP, STL and renders are excluded)
docs/                 feature scope notes and changelog
```

`firmware/README.md` is the upstream framework README (kept for reference). `docs/DS22Lite-feature-scope.md` documents the feature scope and hardware limits.

`firmware/README.md` 为上游框架说明（保留参考）。`docs/DS22Lite-feature-scope.md` 说明功能范围与硬件限制。

## License / attribution / 许可证

This repository redistributes third-party GPL/LGPL code; see `LICENSE` (LGPL-2.1 full text) and `NOTICE.md` for per-component attribution. Upstream file headers remain authoritative.

本仓库再分发第三方 GPL/LGPL 代码；组件归属见 `LICENSE`（完整 LGPL-2.1 文本）与 `NOTICE.md`，上游文件头以原始为准。

## Status / 状态

The firmware builds and passes the bundled static checks. Please open an issue for any problems you encounter.

固件可编译并通过随附的静态检查；使用中遇到问题欢迎提 issue。
