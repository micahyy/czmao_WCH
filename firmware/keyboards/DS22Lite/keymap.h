#pragma once
/* ============================================================================
 * DS22Lite / 22 键 —— keymap.h  v0.1
 * 格式完全照抄 CH55x_QMK_Lite / keyboards/RP17/keymap.h，只把 5x4 改成 5x5。
 *
 * ★ 与 config.h 的联动（改之前先读）
 *   TOTAL_LAYER_COUNT = 4 -> KEY_MAPS 必须正好 4 层，否则数组维度不匹配
 *   DYNAMIC_KEYMAP_LAYER_COUNT = 2 -> 前 2 层走 EEPROM（VIA 可改键）
 *   Layer 2/3 是静态层：键表在 __code Flash，VIA 看不到也改不了，
 *   要改只能改本文件重编；切过去靠 TO(2)/TO(3) / FN(2)/FN(3)。
 *
 * ★ 19:44 砍宏 + 静态 4 层，DataFlash 实算：
 *   config_end = 1(magic) + 5(RGB) = 6（macro_loop 5B 已砍）
 *   keymap 2 x 5x5x2 = 100B -> addr 6..105；macro 0B；合计 106B
 *   CH552 物理 128B，4 层动态键表要 200B 装不下，故后 2 层放 Flash。
 *   19:26 版（双层 + 17B 宏）是 11 + 100 + 17 = 128B，见 log.md。
 *   分区变了 -> via.h 的 VIA_MAGIC_CODE 已改 0x5C，旧板首次上电自动重建。
 *
 * ---------------------------------------------------------------------------
 * 电气矩阵（20:07 由 Netlist_DS22Lite_R1.0.0 逐脚反推，已取代 19:08 的
 * board_data.scad 坐标推测）
 *
 *   ROW 网络（BAT54C 共阴极侧）= {P3.3,P3.4,P3.5,P1.3,P1.2}，下标 row0..4
 *   COL 网络（芯片直连侧）    = {P3.2,P1.4,P1.5,P1.6,P1.7}，下标 col0..4
 *
 *        col0    col1    col2    col3    col4     网表开关名
 * row0   DEL     NUM     P/      P*      P-
 * row1   HOME    P7      P8      P9      --       (1,4) 空位
 * row2   PGUP    P4      P5      P6      P+       2U 高 + 键接在这里
 * row3   PGDN    P1      P2      P3      --       (3,4) 空位
 * row4   FN      P0      PDOT    --      PENT     2U 高 Enter 接在 (4,4)
 *
 * 共 22 个实体键、25 个矩阵位，3 个物理空位 = (1,4) (3,4) (4,3)。
 * KC_NO 共 3 个（仅物理空位）；(4,0) 的 FN 现在填 KC_TO_1 切到层 1，
 * 层 1 的 (0,0) 是 KC_TO_0 切回来，见 config.h 末尾的按住式粘层提醒。
 * ⚠ RGB_CONFIG 的 NO_LED 仍是 3 个、只对应物理空位：灯位映射和键码无关，
 *   FN 键有灯，(4,0) 保留 LED 编号 2。
 *
 * ⚠ (r,c) 是**电气坐标**，不是物理 x/y：物理 row1 col4 的 2U 高 "+" 键
 *   电气上接 (2,4)，物理 row3 col4 的 2U 高 Enter 接 (4,4)，
 *   物理 row4 col3 的 "." 接 (4,2)。DS22Lite_via.json 的 layout 数组顺序和
 *   {"h":2}/{"w":2} 保留物理形状，只改这三个键的 "r,c" 标签。
 *   键码按网表开关名（丝印）填，不再是占位推测；VIA 里仍可任意改。
 * ==========================================================================*/

// clang-format off

/* ---- 基础层：按网表丝印填，DEL/NUM 与导航列在左 --------------------------
 * (4,0) FN = KC_TO_1（切换式）。层 1 的 (0,0)=KC_TO_0 负责切回来。
 * 想改按住式 FN(1)/KC_MO_1，两层 (4,0) 必须都放 MO_1，否则松手粘层。
 * VIA 里这个位置随便改（媒体键 / RGB 控制 / 静态层入口都行）。 */
#define LAYER_0 {                                            \
   {KC_DEL,   KC_NUM,   KC_PSLS,  KC_PAST,  KC_PMNS},      \
   {KC_HOME,  KC_P7,    KC_P8,    KC_P9,    KC_NO},        \
   {KC_PGUP,  KC_P4,    KC_P5,    KC_P6,    KC_PPLS},      \
   {KC_PGDN,  KC_P1,    KC_P2,    KC_P3,    KC_NO},        \
   {KC_TO_1,  KC_P0,    KC_PDOT,  KC_NO,    KC_PENT},      \
}

/* ---- 第二层：19:26 起 TWO_LAYER_ENABLE 已开，参与编译 ----------------- */
/* 导航 / 媒体层，键码跟着物理键迁移到新电气坐标：+ 键→(2,4)、
   Enter→(4,4)、. 键→(4,2)；(0,0)=TO(0) 切回基础层。 */
#define LAYER_1 {                                            \
   {KC_TO_0,  KC_MUTE,  KC_VOLD,  KC_VOLU,  KC_DEL},       \
   {KC_HOME,  KC_UP,    KC_END,   KC_BSPC,  KC_NO},        \
   {KC_LEFT,  KC_DOWN,  KC_RIGHT, KC_ENT,   KC_PGUP},      \
   {C(KC_X),  C(KC_C),  C(KC_V),  C(KC_Z),  KC_NO},        \
   {KC_SPC,   KC_BSPC,  KC_DEL,   KC_NO,    KC_SPC},       \
}

/* ---- 静态层：19:44 起参与编译，放 __code Flash，不占 DataFlash -----------
 * 这两层 VIA 改不了，默认内容只是给老板一个能用的起点：
 *   Layer 2 = F1~F12 + 系统/应用键；Layer 3 = RGB 控制 + 鼠标 + 媒体。
 * 要换成别的键位直接改下面两个宏，再 `python build.py --clean` 重编。
 * 每层的 (4,0) 与 (0,0) 都放了 KC_TO_0，保证切过去出得来。
 * 3 个物理空位 (1,4) (3,4) (4,3) 仍必须填 KC_NO。 */
#define LAYER_2 {                                            \
   {KC_F1,   KC_F2,   KC_F3,   KC_F4,   KC_F5},            \
   {KC_F6,   KC_F7,   KC_F8,   KC_F9,   KC_NO},            \
   {KC_F10,  KC_F11,  KC_F12,  KC_PSCR, KC_PAUS},          \
   {KC_INS,  KC_CAPS, KC_APP,  KC_RGUI, KC_NO},            \
   {KC_TO_0, KC_CALC, KC_MAIL, KC_NO,   KC_ENT},           \
}

#define LAYER_3 {                                            \
   {KC_TO_0,   RGB_MODE_U, RGB_HUI_U,  RGB_HUD_U, RGB_SAI_U}, \
   {RGB_SAD_U, RGB_VAI_U,  RGB_VAD_U,  RGB_SPI_U, KC_NO},  \
   {RGB_SPD_U, KC_MS_BTN1, KC_MS_BTN2, KC_MS_BTN3, KC_MS_UP}, \
   {KC_MS_DOWN,KC_MS_LEFT, KC_MS_RIGHT,KC_MS_WH_UP,KC_NO},  \
   {KC_TO_0,   KC_MS_WH_DOWN, KC_MUTE, KC_NO,    KC_MPLY}, \
}

/* 必须与 config.h 的 TOTAL_LAYER_COUNT 一致（dynamic_keymap.c 用该宏定维度） */
#define KEY_MAPS { LAYER_0, LAYER_1, LAYER_2, LAYER_3, };

/* ---- 编码器：20:07 T 版已关 ENCODER_ENABLE（板上无 EC11）----------------- */
/* ENCODER_MAP 无条件定义是沿用 RP17 写法；dynamic_keymap.c 里由
   #ifdef ENCODER_ENABLE 包住，关掉时不生效、不占 DataFlash，不用删。 */
#define ENCODER_MAP {                                        \
    {ENCODER_CCW_CW(KC_MS_WH_UP, KC_MS_WH_DOWN)},           \
    {ENCODER_CCW_CW(KC_VOLU, KC_VOLD)},                     \
};

/* ---- RGB 灯位映射：22 颗灯 + 3 个 NO_LED（仅 T 版 / 出货全贴版用） ------ */
/* 第一部分 [row][col] -> LED 编号，NO_LED 落在 3 个物理空位（与 KC_NO 无关）。
   20:07 跟着电气矩阵挪了 3 个编号：. 键 0 从 (4,3)→(4,2)、
   Enter 3 从 (3,4)→(4,4)、+ 键 12 从 (1,4)→(2,4)，其余不动。
   第二部分是 SignalRGB 近似物理坐标，与电气坐标无关，不用改。 */
#define RGB_CONFIG { {                                      \
    {21,      20,      19,      18,      17},              \
    {16,      15,      14,      13,      NO_LED},          \
    {11,      10,      9,       8,       12},              \
    {7,       6,       5,       4,       NO_LED},          \
    {2,       1,       0,       NO_LED,  3},               \
}, {                                                       \
    {224, 0}, {168, 0}, {112, 0}, {56, 0}, {0, 0},         \
    {224, 24}, {168, 24}, {112, 24}, {56, 24}, {0, 24},    \
    {224, 48}, {168, 48}, {112, 48}, {56, 48},             \
    {224, 72}, {168, 72}, {112, 72}, {56, 72}, {0, 72},    \
    {224, 96}, {168, 96}, {0, 96},                         \
}};

#define RGB_MODE_RESET {                                    \
    .mode = RGB_MATRIX_CYCLE_UP_DOWN,                       \
    .hsv.h = 120, .hsv.s = 255, .hsv.v = 100,               \
    .speed = 50,                                            \
};

// clang-format on
