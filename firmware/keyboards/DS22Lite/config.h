#pragma once
/* ============================================================================
 * DS22Lite_R1.0.0 / CH552T (TSSOP20) —— 22 键 numpad 固件配置
 *
 * 框架 : lbtrobit/CH55x_QMK_Lite + ch55xduino 0.0.18
 * 硬件 : Netlist_DS22Lite_R1.0.0_2026-11-08.net（CH552T 板）
 *
 * 编译 : Arduino IDE + ch55xduino 0.0.18
 *          Board = CH552 / Clock = 24MHz internal(5V)
 *          USB Settings = USER CODE w/ 266B ram
 *        或在本目录直接跑 build.py（自带便携 SDCC，不用装 IDE）
 * ==========================================================================*/

/* ---- USB 身份 ------------------------------------------------------------
 * VIA 3.0 不接受 vendorId 0xFEED。
 * USB 身份统一登记在 firmware/boards.json。修改 VID/PID / 产品串时须同步
 * 本文件、DS22Lite_via.json、DS22Lite_signalrgb.js，然后重新编译烧录。
 */
#define VENDOR_ID   0x4A17
#define PRODUCT_ID  0x4A50
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
 * CH552T 的 DataFlash 物理只有 128B，而每层 VIA 动态键表需要
 * 5x5x2 = 50B。因此固件采用「2 层动态 + 2 层静态」的结构：
 *
 *   DYNAMIC_KEYMAP_LAYER_COUNT = 2 -> VIA 显示并可改 Layer 0 / Layer 1
 *   TOTAL_LAYER_COUNT          = 4 -> 固件实际有 4 套键位
 *   Layer 2/3 的键表放在 __code Flash 常量 keymaps[] 中，
 *   dynamic_keymap_get_keycode() 对 layer>=2 直接读常量、set_keycode 忽略。
 *   Layer 2/3 需修改 keymap.h 重新编译；VIA 里不可编辑。
 *   切换到静态层：在 Layer 0/1 上绑定 TO(2)/TO(3) 即可。
 *
 * DataFlash 分区：
 *   magic 1B            -> addr 0
 *   RGB config 5B       -> addr 1..5
 *   VIA_EEPROM_CONFIG_END = 6
 *   keymap 2 x 5x5x2 = 100B -> addr 6..105
 *   tapdance / encoder / macro: 0B
 *   合计 106B，128B 中余 22B。
 *
 * 受片内资源限制，动态宏 / Tap Dance / FCODE_TO_MOUSE / 编码器 / Dial
 * 未启用。VIA 宏页面为空、相关菜单不出现属于预期行为。
 *
 * 修改 DataFlash 分区后须同步递增 via.h 的 VIA_MAGIC_CODE，
 * 使旧配置在上电时自动重建 EEPROM。
 */
#define TWO_LAYER_ENABLE
#define TOTAL_LAYER_COUNT   4   /* 2 层动态(VIA 可改) + 2 层静态(Flash 常量) */
// #define DYNAMIC_MACRO_ENABLE   /* 未启用 */
// #define TAP_DANCE_ENABLE
#define TAP_DANCE_COUNT  2      /* 关 TAP_DANCE_ENABLE 时无效，留着方便开回 */
#define TAPPING_TERM     200
#define EXTRAKEY_ENABLE
#define MOUSE_ENABLE
// #define FCODE_TO_MOUSE_ENABLE  /* 宏里的 F13~F21→鼠标映射；未启用 */
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

/* ---- 层切换键 ------------------------------------------------------------
 * Layer 0 (4,0) FN = KC_TO_1；Layer 1 (0,0) = KC_TO_0；
 * Layer 2/3 的 (4,0)/(0,0) 也放了 KC_TO_0，切过去出得来。
 * VIA 只看到 2 层；TO(2)/TO(3)/FN(2)/FN(3) 在自定义键码里，
 * 绑到 Layer 0/1 任意键即可切到静态层。static 层不能在 VIA 里改键。
 *
 * 按住式 MO(n) 的注意事项：process_record_quantum() 每次按键事件都用
 * current_layer 重新查键表，不缓存按下时的键码。若只在层 0 放 MO_1，
 * 松开时可能读到其它层的键码，导致层切换未释放。要使用按住式切层，
 * 相关两层同一位置都应放 MO_n。默认键表全部使用切换式 TO(n)。
 *
 * DS22Lite_via.json 里 25 项 customKeycodes 都是 quantum.h 枚举占位，
 * 顺序必须逐项一致，删/插任何一项后面全部错位。末尾 4 项为静态层的
 * TO(2)/TO(3)/FN(2)/FN(3)。
 */
