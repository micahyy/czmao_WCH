#pragma once
/* ============================================================================
 * DS22Lite_R1.0.0 / CH552T (TSSOP20) —— 22 键 numpad 固件配置
 *
 * 框架 : lbtrobit/CH55x_QMK_Lite (main 90cc1ad) + ch55xduino 0.0.18
 * 硬件 : Netlist_DS22Lite_R1.0.0_2026-11-08.net（CH552T 板，67 元件）
 * 档案 : 完整推理/纠错链在 ../../../pcb/ch552g_22key/config_ch552t.h，本文件只留结论
 *
 * 编译 : Arduino IDE + ch55xduino 0.0.18
 *          Board = CH552 / Clock = 24MHz internal(5V)
 *          USB Settings = USER CODE w/ 266B ram
 *        或在本目录直接跑 build.py（自带便携 SDCC，不用装 IDE）
 * ==========================================================================*/

/* ---- USB 身份 ------------------------------------------------------------
 * 原先用 QMK 通用 VID 0xFEED，但 VIA 3.0 (usevia.app) 的 the-via/reader
 * src/transform.ts 第 31 行硬编码拒绝：
 *     if (vendorId.toUpperCase() === '0XFEED')
 *         throw new Error(`'0xFEED' is not a valid vendorId.`);
 * 大小写都拒，报错里那个 0xFEED 是写死的字符串，不是回显文件内容。
 * 现沿用上游框架自带 5 个键盘的 VID 0x5262，PID 0x4443（原按 'DC'
 * 的 ASCII 取；已用 PID：0x5217 / 0x4E45 / 0x4B4B / 0x4E4B / 0x4E52）。
 * 2026-11-12 键盘改名 DS22Lite，VID/PID 故意不动：它只是设备身份，
 * 改名只动 USB 产品串 + VIA name + 插件名，避免又要重导 VIA 定义。
 * 三处必须同步：本文件、DS22Lite_via.json、DS22Lite_signalrgb.js，然后重编重烧。
 * 出货前若要换成自己的 VID（USB-IF 付费 / pid.codes 开源免费 PID），
 * 同样三处同步 + `python build.py --clean`。
 */
#define VENDOR_ID   0x5262
#define PRODUCT_ID  0x4443
#define MANUFACTURER 'M','a','i','k','u'
#define PRODUCT      'D','S','2','2','L','i','t','e'

/* ---- 矩阵 -----------------------------------------------------------------
 * 宏值 = 端口*10 + 位号（框架编码），不是 TSSOP20 封装脚号。
 *
 *   ROW（BAT54C 共阴极侧 / 输出拉低）= P3.3 P3.4 P3.5 P1.3 P1.2
 *   COL（开关直连芯片 / 输入上拉）  = P3.2 P1.4 P1.5 P1.6 P1.7
 *
 * BAT54C pin3 共阴极全接 ROW，两个阳极经开关接 COL → 电流只能 COL→ROW，
 * 故 COL 是被读的输入侧、ROW 是选通输出侧 = COL2ROW。
 *
 * 框架 matrix_init_pins() 把 10 个脚全设 INPUT_PULLUP（CH552 准双向模式
 * 自带上拉），仅选通瞬间切输出低 → 外部 10k 上拉不需要，网表里也没有。
 * RST(pin6) 彻底悬空；P1.2/P1.3 是 XI/XO，本板不接晶振，
 * 用内置 24MHz RC + USB SOF 校准。V33(pin20) 必须单独 100nF 去耦。
 */
#define MATRIX_ROWS     5
#define MATRIX_COLS     5
#define MATRIX_ROW_PINS { 33, 34, 35, 13, 12 }
#define MATRIX_COL_PINS { 32, 14, 15, 16, 17 }
#define DIODE_DIRECTION COL2ROW

/* ---- 功能开关：ROM 14336B / DataFlash 128B --------------------------------
 * 【19:44 定案】老板拍板：砍掉宏（17B 本来也干不了啥），层做到 4 套。
 *
 * 为什么 VIA 里仍然只显示 2 层：
 *   CH552 的 DataFlash 物理只有 128B（eeprom.c:15 `if (addr>=128) return`，
 *   读也用 uint8 地址），每层动态键表 5x5x2 = 50B，4 层要 200B，
 *   加配置 206B —— 超 78B。砍到全裸、128B 全给键表也只够 2.56 层。
 *   要 VIA 显示 4 层且都能改键，只能换更大 EEPROM 的 MCU / 外挂 EEPROM，
 *   或把键码压成 1 字节（VIA 协议不兼容 = 放弃 VIA 改键）。都不划算。
 *
 * 本配置的做法 = 静态层：
 *   DYNAMIC_KEYMAP_LAYER_COUNT = 2 -> VIA 显示并可改 Layer 0 / Layer 1
 *   TOTAL_LAYER_COUNT          = 4 -> 固件实际有 4 套键位
 *   Layer 2/3 的键表放 __code Flash 常量 keymaps[]，
 *   dynamic_keymap_get_keycode() 对 layer>=2 直接读常量、set_keycode 忽略。
 *   代价：Layer 2/3 改键要改 keymap.h 重编，VIA 里看不到也不能改。
 *   切过去：VIA 里给 Layer 0/1 任意键绑 TO(2)/TO(3)，或编译期直接放。
 *
 * DataFlash 分区（19:44，按 dynamic_keymap.h + via.h 实算）：
 *   magic 1B -> addr 0
 *   macro_loop 0B -> 已砍（原 5B）
 *   RGB config 5B -> addr 1..5
 *   VIA_EEPROM_CONFIG_END = 6
 *   keymap 2 x 5x5x2 = 100B -> addr 6..105
 *   tapdance 0B / encoder 0B / macro 0B
 *   合计 106B，物理 128B 里空 22B（eeprom 地址上限仍是 128，不动它）。
 *
 * 砍宏具体做了什么（框架本来没有开关，自己加 DYNAMIC_MACRO_ENABLE）：
 *   via.h   MACRO_LOOP_SIZE 5->0（RGB 配置前移 5B）
 *   dynamic_keymap.h  MACRO_COUNT 5->0 / MACRO_EEPROM_SIZE -> 0
 *   dynamic_macro.c、send_string.c 整包 #ifdef 编出去，空实现兜住调用点
 *   via.c / action.c 调用点同步 #ifdef；VIA json 删 Macro Loop 菜单
 *   VIA 仍上报「0 个宏 / 0 字节缓冲」，网页宏页显示为空而不是报错
 *   开回宏：下面定义 DYNAMIC_MACRO_ENABLE，但 ROM 必须重核 <= 14336
 *
 * ⚠ 分区变了：via.h 的 VIA_MAGIC_CODE 已 0x5B -> 0x5C，烧过旧固件的板
 *   首次上电自动重建 EEPROM。以后再改分区必须同步 +1。
 *
 * ROM 历史实测（诊断时临时放大 --code-size 取真实占用，编完立刻还原）：
 *   A 单层 全功能           14126B / macro 55B
 *   B 双层 全功能           14358B（超 22B，链接失败）
 *   D 双层 -TAP_DANCE       13642B / macro 17B
 *   D+F 双层 -TD -FCODE     13548B / macro 17B（19:26 交付版）
 *   19:44 砍宏 + 静态 4 层 -> 见 build/DS22Lite_CH552T.mem
 */
#define TWO_LAYER_ENABLE
#define TOTAL_LAYER_COUNT   4   /* 2 层动态(VIA 可改) + 2 层静态(Flash 常量) */
// #define DYNAMIC_MACRO_ENABLE   /* 砍宏；20:24 实测开回来 = 13812B/14336B，能编过 */
// #define TAP_DANCE_ENABLE
#define TAP_DANCE_COUNT  2      /* 关 TAP_DANCE_ENABLE 时无效，留着方便开回 */
#define TAPPING_TERM     200
#define EXTRAKEY_ENABLE
#define MOUSE_ENABLE
// #define FCODE_TO_MOUSE_ENABLE  /* 宏里的 F13~F21→鼠标映射；宏砍了自然无效 */
// #define DIAL_ENABLE

/* ---- RGB：22 颗 WS2812B，DIN = P1.0 / TSSOP20 pin7 ------------------------
 * 网表网络名 2812_PWM。框架 WS2812 是软件 bit-bang（按脚号选
 * neopixel_show_Px_y()），换脚只改这个宏，不用写驱动。
 * 板内走线 22mm 不串电阻；P3.6/P3.7(USB) 和 10 个矩阵脚不可占用。
 * 出货版若不贴灯珠，注释掉下面 5 个宏即可，其余不用动。
 */
#define RGB_MATRIX_ENABLE
#define WS2812_DI_PIN        10
#define RGB_MATRIX_LED_COUNT 22
#define RGB_EFFECTS_PLUS
#define RGB_KEY_CONTROL

/* ---- 编码器：板上没有 EC11 ------------------------------------------------
 * P1.2/P1.3 已被 ROW4/ROW3 占用，开着会抢脚。关掉后 dynamic_keymap.c 的
 * encoder_map 由 #ifdef ENCODER_ENABLE 包住，不编译也不占 DataFlash。
 */
// #define ENCODER_ENABLE
// #define NUM_ENCODERS    1
// #define ENCODERS_PAD_A  { 12 }
// #define ENCODERS_PAD_B  { 13 }

/* ---- 层切换键（19:44） ---------------------------------------------------
 * Layer 0 (4,0) FN = KC_TO_1；Layer 1 (0,0) = KC_TO_0；
 * Layer 2/3 的 (4,0)/(0,0) 也放了 KC_TO_0，切过去出得来。
 * VIA 只看到 2 层；TO(2)/TO(3)/FN(2)/FN(3) 在自定义键码里，
 * 绑到 Layer 0/1 任意键即可切到静态层。static 层不能在 VIA 里改键。
 *
 * ⚠ 按住式 MO(n) 的粘层坑（框架缺陷，对所有层都成立）：
 *   process_record_quantum() 每次按键事件都用 current_layer 重新查键表，
 *   不缓存按下时的键码。若只在层 0 放 MO_1，松开时读到层 1 的 (4,0)
 *   （KC_SPC），layer_switch() 释放分支命中不了 → 永久粘层。要用按住式，
 *   相关两层同一位置都得放 MO_n。默认全部用切换式 TO(n)。
 *
 * DS22Lite_via.json 里 25 项 customKeycodes 都是 quantum.h 枚举占位，
 * 顺序必须逐项一致，删/插任何一项后面全部错位。末尾 4 项是 19:44
 * 为静态层追加的 TO(2)/TO(3)/FN(2)/FN(3)。
 */
