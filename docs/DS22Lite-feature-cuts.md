# DS22Lite 功能裁剪说明

> 固件：`firmware/build/DS22Lite_CH552T.hex`（30849B，ROM 12652B / 14336B，XRAM 286B / 758B）
> 框架：`lbtrobit/CH55x_QMK_Lite` + `ch55xduino 0.0.18`｜主控：CH552T (TSSOP20)｜2026-11-12

**一句话**：为了在 CH552 上做到「2 层 VIA 改键 + 4 套键位 + 22 颗 RGB」，主动砍掉了宏、复用键、鼠标宏映射三类功能，把 ROM 从「编不进去」压到余 1684B（含恢复出厂设置修复）。

## 硬约束（所有裁剪的根因）

| 资源 | 物理上限 | 说明 |
|---|---|---|
| ROM / 程序 Flash | **14336B (`0x3800`)** | `0x3800` 以上是出厂 bootloader，越界会覆盖刷机能力 |
| DataFlash / EEPROM | **128B** | `eeprom.c:15` 写死 `if (addr >= 128) return`，读地址也是 `uint8` |
| XRAM | 758B | USB 占 0..265，实际用 283B |

VIA 每存一层动态键表要 `5×5×2 = 50B`。4 层全可改键要 `200B + 6B 配置 = 206B`，超 78B；**砍到全裸、128B 全给键表也只够 2.56 层**。这是物理限制，不是配置问题。

---

## A. 主动砍掉的功能

| 功能 | 原来能干什么 | 现在 | 为什么砍 | 用户可见影响 |
|---|---|---|---|---|
| **VIA 录制宏** | 5 个宏 M0~M4，可录按键序列 / 发送字符串、设循环次数 1~254（255 无限）、再按一次停止 | **完全移除** | 双层键表吃掉 100B 后，录宏缓冲只剩 17B（约 8 个键码，按两三下就满）；而宏代码要吃 1372B ROM | VIA 宏页显示为空（上报 0 个宏 / 0 字节），不报错；M0~M4 按下去无效 |
| **Tap Dance 复用键** | 任意键设单击 / 双击 / 按住三种动作，最多 5 个 | **完全移除** | 上游实测 716B ROM + 每键 6B DataFlash；双层全功能 14358B 超 22B，砍它才编得过 | VIA 里没有 Tap Dance 菜单；5 个 TD 占位标 `DISABLED`，按下无效 |
| **F13~F21 转鼠标键** | 宏里输 F13~F21 代替鼠标左/右/中键、方向、滚轮，做鼠标宏 / 连点器 | **完全移除** | 约 94B ROM，且宏砍了它本来就没载体 | 鼠标宏、连点器做不了；普通鼠标键仍可用 |
| **VIA Macro Loop 菜单** | 设每个宏的循环次数 | **删除** | 宏循环配置区 5B 一并砍掉，RGB 配置前移到 addr 1..5 | 菜单消失属正常 |
| **VIA Tap Dance 菜单** | 配单击/双击/按住键 | **删除** | 同上 | 菜单消失属正常 |
| **录宏数据区** | 17B（19:26 版）/ 上游 78B | **归零** | 让出 5B 配置 + 17B 缓冲 | EEPROM 从 128B 用满降到 106B，空 22B |

**砍宏具体改了什么**（框架本来没有开关，自建 `DYNAMIC_MACRO_ENABLE`）：

| 文件 | 改动 |
|---|---|
| `config.h` | 注释掉 `DYNAMIC_MACRO_ENABLE`；20:24 实测开回来 = 13812B，能编过 |
| `via.h` | `MACRO_LOOP_SIZE 5→0`（条件编译），RGB 配置前移 5B；`VIA_MAGIC_CODE → 0x5C` |
| `dynamic_keymap.h` | `MACRO_COUNT 5→0`、`MACRO_EEPROM_SIZE → 0` |
| `dynamic_macro.c` | 整包 `#ifdef`，`#else` 留空实现兜住 `via.c` / `action.c` 调用点 |
| `send_string/send_string.c` | 整包 `#ifdef`；**20:24 补了 `config_common.h` 引入**，否则开回宏会报 `Undefined Global '_send_char'` |
| `via.c` | 初始化、QK_MACRO 分支、macro_loop 通道、macro get/set buffer 全条件编译；保留 `get_count` / `get_buffer_size` 返回 0 |
| `action.c` | `dynamic_macro_task()` 调用条件编译 |
| `DS22Lite_via.json` | 删 Macro Loop / Tap Dance 菜单；Lighting / System 保留 |

---

## B. 降级但没砍死

| 项目 | 现状 | 代价 / 限制 |
|---|---|---|
| **USB 报告层 / 无冲** | 标准 **Boot Keyboard 6KRO**：9B 报告 = report_id 1 + mods 1 + reserved 1 + `keys[6]` | 第 7 个及以后的普通键被 `add_key_to_report()` 静默丢弃；8 个修饰键是独立位图，**不占 6 个名额**。详见文末专题 |
| **层数** | `TOTAL_LAYER_COUNT = 4`，但 `DYNAMIC_KEYMAP_LAYER_COUNT = 2` | VIA **只显示 Layer 0 / 1 且可改键**；Layer 2（F1~F12 + 系统键）、Layer 3（RGB / 鼠标 / 媒体）是 `__code` Flash 静态层，**只读**，改键必须改 `keymap.h` 重编 |
| **静态层入口** | `TO(2)/TO(3)/FN(2)/FN(3)` 在 VIA 自定义键码末尾（共 25 项） | 绑到 Layer 0/1 任意键即可切过去；静态层里都放了 `TO(0)` 回层 |
| **自定义键码占位** | 3 个 Dial + 5 个 TD 占位仍留在枚举和 JSON 里，标 `DISABLED` | **不能删**：VIA 按下标取键码，删/插任一项后面全部错位 |
| **按住式切层 `MO(n)`** | 代码支持 `MO(0..3)`，默认键表全用切换式 `TO(n)` | 框架缺陷：松手时用当前层重新查键表，只在单层放 `MO` 会永久粘层；要用必须相关两层同一位置都放 `MO` |

---

## C. 上游默认就没开 / 板子本来就没有（不算这轮阉割）

| 项目 | 原因 |
|---|---|
| 编码器 EC11 | 板上没有；`P1.2/P1.3` 已被 ROW3/ROW4 占用，开 `ENCODER_ENABLE` 会抢脚 |
| Surface Dial | 上游 RP17 的 `DIAL_ENABLE` 本来就是注释掉的；DS22Lite 同样关闭 |
| Direct pin 直连 | 本板是 5×5 矩阵 + BAT54C 二极管（COL2ROW），不用直连 |
| 物理恢复出厂设置 | RST(pin6) 定案悬空 NC；恢复出厂走 VIA `System → Restore factory defaults`，救砖走 D+ 短接 |
| 外部晶振 | 用内置 24MHz RC + USB SOF 校准，`P1.2/P1.3` 腾给矩阵 |

---

## D. 框架精简遗留（不是这次砍的，但要知道）

| 位置 | 现象 | 影响 |
|---|---|---|
| `via.c` 0x0A / 0x0B | 原为 `id_eeprom_reset` / `id_bootloader_jump` 空 `break`，VIA Debug 页 / 新版系统菜单点了没反应 | **2026-11-12 已修**：标准命令与 channel 6 按钮共用 `via_factory_reset()` / `via_bootloader_jump()`，见文末 I 节 |
| `eeconfig_init_via()` | 只写 addr 0..105，addr 106..127 那 22B 不清 | 旧版残留的宏数据留着，但没代码读它，功能无害 |
| `id_dynamic_keymap_set_buffer` | offset 无上界检查 | `eeprom.c` 有 `addr>=128` 兜底，不会物理越界 |

---

## E. 保留的功能（别以为全砍了）

| 功能 | 状态 |
|---|---|
| VIA 实时改键（Layer 0 / 1，掉电保持） | 保留，双层动态 |
| 4 套键位 + 层切换 `TO(0..3)` / `MO(0..3)` | 保留 |
| 矩阵防鬼键（同按任意键不误判） | 保留，`COL2ROW` + BAT54C 二极管；**只防鬼影，不等于 USB 层 NKRO**（见文末专题） |
| 22 颗 WS2812B RGB、6 种本地色彩循环、按键控灯效 | 保留，`RGB_MATRIX_ENABLE` / `RGB_EFFECTS_PLUS` / `RGB_KEY_CONTROL` 全开 |
| VIA Lighting 菜单、SignalRGB 神光同步 | 保留 |
| 多媒体 / 系统键、鼠标键 | 保留，`EXTRAKEY_ENABLE` / `MOUSE_ENABLE` 开 |
| VIA System：恢复出厂设置、进 bootloader | 保留，已修（按钮式 + 回包后软复位，见 I 节） |
| USB 免晶振、D+ 短接救砖、VIA 协议 0x000C | 保留 |

---

## F. 两本账

### ROM 账（全部为实测）

| 配置 | ROM | 结果 |
|---|---:|---|
| 单层 全功能 | 14126B | 编得过 |
| 双层 全功能（TD + FCODE + 宏） | 14358B | **超 22B，链接失败** ← 唯一编不进去的 |
| 双层 −Tap Dance | 13642B | 砍 TD 省 716B |
| 双层 −TD −FCODE（19:26 交付） | 13548B | 砍 FCODE 省 94B，宏仍在 |
| 双层 + 宏 + 静态 4 层（20:24 试验） | 13812B | 余 524B，能编过 |
| **砍宏 + 静态 4 层 + 恢复出厂修复（当前交付）** | **12652B** | **余 1684B** |

拆解：宏净吃 **1372B**（`send_string` 四张 ASCII LUT + 宏回放 + VIA 宏命令）；静态 2 层吃 **264B**（100B 键表 + 164B `pgm_read_word` 分支）。砍宏省 1372B、静态层吃回 264B → 净省 **1108B**；本轮恢复出厂修复（延迟动作 + 软复位 + 系统键值 + 标准命令）再吃 **212B**。

> 撑爆 ROM 的其实是 Tap Dance 和 FCODE，不是宏。砍宏的理由不是「编不进去」，而是「17B 录不了东西，却要养 1372B 代码」。

### DataFlash 账

| 版本 | 分区 | 合计 |
|---|---|---|
| 上游单层 | magic 1 + macro_loop 5 + RGB 5 + 键表 50 + TD 12 + 宏 55 | 128B |
| 19:26 双层 + 宏 | magic 1 + macro_loop 5 + RGB 5 + 键表 100 + 宏 17 | 128B 用满 |
| **当前 砍宏 + 静态 4 层** | magic 1 + RGB 5 (addr 1..5) + 键表 100 (addr 6..105) | **106B，空 22B** |

---

## G. 想开回来要做什么

| 想开 | 动作 | 代价 |
|---|---|---|
| 宏 | 取消 `DYNAMIC_MACRO_ENABLE` 注释 | ROM 13812B（余 524B）；宏缓冲仍只有 17B；**必须同步下面四件套** |
| Tap Dance | 定义 `TAP_DANCE_ENABLE` | ROM +716B、DataFlash +12B；双层下大概率要再砍别的功能 |
| FCODE 转鼠标 | 定义 `FCODE_TO_MOUSE_ENABLE` | 单独开没意义，宏砍了没载体 |
| 编码器 / Dial | 不建议 | 板上没脚 / 无实际用途 |

**开回任意一项都要重编重烧**，且涉及 DataFlash 分区变化时必须同步：

1. `via.h` 的 `VIA_MAGIC_CODE` **+1**（当前 `0x5C`）
2. `DS22Lite_via.json` 加回对应菜单
3. `verify.py` 断言翻口径
4. `config.h` / `keymap.h` 分区注释更新
5. VIA Design 页删旧定义、重导 JSON

> ⚠ **magic 铁律**：`via_init()` 只在上电时检查一次 magic，不匹配才 `eeconfig_init_via()` 重建 EEPROM。分区改了却不 +1，烧过旧固件的板会拿旧数据按新地址读 → 键位 / RGB 全乱。已经踩过三次：`0x5A → 0x5B → 0x5C`。
> 烧完新固件**必须断电重插**才触发重建；自动重建会把 Layer 0/1 改过的键位冲回默认。

---

## H. 已知限制 / 未验证

- **未上板验证**：重点测 `pgm_read_word(&keymaps[layer][...])` 对 `__code` 多维数组变址的运行时正确性、Layer 2/3 逐键、VIA 改键掉电保持、22 颗灯链，以及恢复出厂后的 `bSW_RESET` 热复位 / USB 重枚举。
- VID/PID `0x4A17 / 0x4A50` 为 CZMao 自有 ID（CH552T 系列起始号，预留 `0x4A51..0x4A5F`）；已核对 VIA 定义库 55 组 VID/PID，`0x4A5x` 零命中、vpid `1243040336` 未占用。
- V33(pin20) 100nF 去耦需实物确认。
- Layer 2 / 3 键位是默认方案，要换改 `keymap.h` 重编。

## 附：全键无冲（NKRO）能不能开

**结论：现在不是 NKRO，是 6KRO；能开，但不是加个宏，是一次 USB 报告层重构，且不建议开。**

「无冲」分三层，卡点不在板上：

| 层 | 现状 | 判定 |
|---|---|---|
| ① 矩阵硬件 | 5×5 + 每键 BAT54C 二极管，`COL2ROW` | **已具备防鬼键**：任意键同按不会产生虚假按键，硬件前提没问题 |
| ② 固件报告结构 | `report.h:112` `KEYBOARD_REPORT_SIZE 9` / `KEYBOARD_REPORT_KEYS 6`；`report.c` 的 `add_key_to_report()` 遍历 6 个槽，满了直接 `return` 丢弃 | **卡在这里**：第 7 个普通键发不出去 |
| ③ USB 描述符 | `USBconstant.c:89` `REPORT_COUNT(6)` + `REPORT_SIZE(8)` + `INPUT (Data,Ary,Abs)`；接口声明 `BootSubclass` / `KeyboardBootProtocol`；`host.c:57` 固定 `USB_EP1_send(..., 9)` | **主机按 6 键数组解析**，改结构必须同步改描述符 |

> `report.h:126` 那句「extended to 16 bytes when NKRO mode」是 QMK 上游注释，**本框架没实现**；`keycodes.h:337-339` 的 `NK_ON` / `NK_OFF` / `NK_TOGG` 只是枚举常量，没有任何处理代码。

**要开真 NKRO 得改 6 处**：

| 文件 | 改动 |
|---|---|
| `USBconstant.c` `ReportDescriptor[]` | 6×8bit 键码数组 → 位图（22 键只需 3B，或按 QMK 标准 16B 覆盖 120 键）；`Ary` 改 `Var`，补 `LOGICAL_MIN/MAX` 与 `USAGE_MIN/MAX` |
| `report.h` | `report_keyboard_t` 改位图结构，`KEYBOARD_REPORT_SIZE 9 → 新长度` |
| `report.c` | `add_key_to_report` / `del_key_from_report` / `is_key_pressed` 三个改 bit set / clear / test |
| `host.c:57` | `USB_EP1_send(..., 9)` 改新长度 |
| `action_util.c` / `action.c` | 所有遍历 `keys[]` 的地方（`clear_keys`、层切换、报告比对）跟着改 |
| USB 接口 | **Boot Protocol 兼容**：BIOS / UEFI、Windows 登录界面、部分 KVM 只认 boot report。QMK 的做法是保留 6KRO 接口 + 另加 NKRO 接口，用 `NK_TOGG` 运行时切换，等于双份描述符 + 切换逻辑 |

**为什么不建议开**：

- 22 键数字键盘，同时按 6 个以上**普通键**的场景几乎没有；`Ctrl / Shift / Alt / GUI` 是独立 8bit 位图，不占那 6 个名额，所以 `Ctrl+Shift+Alt+5 个数字键` 仍能正常上报。
- 老板最早就否了 BIOS 场景（「谁会用数字键盘去进 bios」），而 boot protocol 恰恰是保 BIOS / KVM / 登录界面兼容的东西；为 NKRO 动它可能得不偿失。
- ROM 余 1684B、XRAM 余 472B，写是写得下，但要重编重烧 + 全平台实测（Windows / macOS / Linux / BIOS / KVM），跟「只有价格才是竞争力」的定位相冲。

> 顺带回答最早那个问题（「CH552G 做 17 键能不能全键无冲」）：**矩阵层能，USB 报告层是 6KRO**。二极管解决的是鬼影，不是 rollover 上限。

## 文件索引

| 文件 | 作用 |
|---|---|
| `firmware/keyboards/DS22Lite/config.h` | 所有功能开关、ROM / DataFlash 实算、分区注释 |
| `firmware/keyboards/DS22Lite/keymap.h` | 4 层键表、电气矩阵 |
| `firmware/keyboards/DS22Lite/DS22Lite_via.json` | VIA 定义（全 ASCII，中文会乱码） |
| `firmware/src/qmk/quantum/via.h` | DataFlash 分区、magic |
| `firmware/src/qmk/quantum/dynamic_keymap.c` | 静态层分流（layer ≥ 2 读 Flash） |
| `firmware/verify.py` | 一键自检（`RESULT: ALL PASS`） |
| `firmware/build.py` | 便携 SDCC 编译（`python build.py --clean`） |
| `../log.md` | 完整推理 / 纠错链 |

---

## I. 恢复出厂设置修复（参考 EC87 Qoder 修复）

| 项 | 修复前 | 修复后 |
|---|---|---|
| VIA 标准命令 | `id_eeprom_reset`(0x0A) / `id_bootloader_jump`(0x0B) 是空 `break` | 两条都接通，VIA Debug 页 / 新版系统菜单可用 |
| 恢复动作 | HID 回调里就地 `eeconfig_init_via()`（105 次 DataFlash 写），**不复位**：VIA 只在连上时读一遍键表，界面永远停在旧值 | 回调只回包；`via_task()` 延迟 120ms 重建 EEPROM + `bSW_RESET` 软复位 → USB 重新枚举，VIA 重新拉默认键表 / 灯效 |
| 系统键值 | `EE_CLR`(`QK_CLEAR_EEPROM`) / `QK_BOOT` 枚举存在但无处理（与 EC87 同一个坑） | `process_record_via()` 拦截，和 VIA 按钮共用一条路径，不再各写一套自定义逻辑 |
| 菜单控件 | `toggle`：GET 恒回 0 会回弹、关回 0 又触发一次；`id_custom_save` 还会重复执行 | `button` + `options:[1]`；GET 回 0、`id_custom_save` 显式吞掉、set 仅 value=1 触发 |

`CH55x_QMK_Lite.c` 主循环在 `keyboard_task()` 后调用 `via_task()`；`via.h` 补 4 个原型。
DataFlash 分区没变 → `VIA_MAGIC_CODE` 仍 `0x5C`，不用改；`eeconfig_init_via()` 仍只写 addr 0..105。
软复位用 CH552 的 `GLOBAL_CFG |= bSW_RESET`（热复位 `bBOOT_LOAD=0`，从 0 地址跑 app，不会误进 `0x3800` bootloader）。

> 上板：重烧本版 hex，VIA Design 页删旧定义、重导 `DS22Lite_via.json`。点「Restore factory defaults」后键盘断开约 0.5s 自动重连，VIA 界面刷新为默认键位 / 灯效，不需要手动断电重插。
