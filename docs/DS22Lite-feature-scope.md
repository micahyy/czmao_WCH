# DS22Lite 功能范围 / Feature scope

DS22Lite 基于 WCH CH552T (TSSOP20)。受片内资源限制，固件实现的是 QMK/VIA 的精简子集，与桌面版 QMK 的功能集合并不完全一致。本文说明支持与未包含的功能，便于使用前了解。

DS22Lite runs on a WCH CH552T (TSSOP20). Due to the on-chip resources, the firmware implements a focused subset of QMK/VIA rather than the full desktop-QMK feature set. This document lists what is and is not included.

## 硬件资源 / Hardware resources

| 资源 / Resource | 上限 / Limit | 说明 / Note |
|---|---|---|
| 程序 ROM / Program ROM | 14336 B (`0x3800`) | 以上地址为出厂 bootloader |
| DataFlash / EEPROM | 128 B | 存放 VIA 动态键表与配置 |
| XRAM | 758 B | 前 266 B 由 USB 缓冲区占用 |

VIA 每层动态键表占 `5×5×2 = 50 B`。因此 4 层键位中，Layer 0/1 存于 DataFlash 并可由 VIA 实时编辑；Layer 2/3 编译进 ROM，为只读键位。

Each VIA dynamic layer takes `5×5×2 = 50 B`. Of the four layers, Layer 0/1 live in DataFlash and can be edited live in VIA; Layer 2/3 are compiled into ROM and are read-only.

## 支持的功能 / Supported

| 功能 / Feature | 说明 / Note |
|---|---|
| 4 层键位 / 4 layers | `TO(0..3)` / `MO(0..3)` 层切换 |
| VIA 实时改键 / VIA live keymap | Layer 0/1，掉电保持 |
| 22 颗 WS2812B / 22 WS2812B LEDs | VIA 灯效、单键 RGB、SignalRGB 插件 |
| 矩阵防鬼键 / Ghost-key protection | 5×5 `COL2ROW` + BAT54C 二极管 |
| 媒体键 / 系统键 / 鼠标键 | Media, system and mouse keys |
| VIA 恢复出厂设置 / 进入 Bootloader | 按钮式，含标准 `0x0A` / `0x0B` 命令与 `EE_CLR` / `QK_BOOT` 键值 |
| USB 免外部晶振 / Crystal-less USB | 内置 RC + USB SOF 校准 |

## 未包含的功能 / Not included

| 功能 / Feature | 原因 / Note |
|---|---|
| 动态宏 / Dynamic macros | 宏数据与回放代码超出片内 ROM / DataFlash 容量 |
| Tap Dance | 同上 |
| 鼠标宏映射 / FCODE_TO_MOUSE | 依赖宏功能 |
| 编码器 / Encoder | 板上未配备 |
| Surface Dial | 板上未配备 |
| NKRO / 全键无冲 | 使用标准 6KRO 引导键盘报告 |
| VIA Macro Loop / Tap Dance 菜单 | 随对应功能一并省略 |

VIA 宏页面为空、相关菜单不出现属于预期行为，并非故障。
An empty VIA macro page and the absence of these menus are expected.

## 已知限制 / Known limitations

- **6KRO**：USB 使用标准 6KRO 引导键盘报告；8 个修饰键为独立位图，不占用 6 个普通键名额。
  **6KRO**: a standard boot keyboard report is used; the eight modifiers are a separate bitmap and do not consume the six key slots.
- **Layer 2/3 只读**：如需自定义这两层键位，请修改 `firmware/keyboards/DS22Lite/keymap.h` 后重新编译。
  **Read-only layers**: to customise Layer 2/3, edit `firmware/keyboards/DS22Lite/keymap.h` and rebuild.
- 矩阵二极管解决的是鬼影（ghosting），与 USB 报告层的 rollover 上限是两件事。
  The matrix diodes prevent ghosting; they do not change the USB report rollover limit.

## 构建 / Build

编译与烧录方法见仓库根目录 `README.md`。
See the repository `README.md` for build and flashing instructions.
