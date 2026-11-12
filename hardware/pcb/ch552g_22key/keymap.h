#pragma once
/* ============================================================================
 * DS22Lite / 22 键 —— keymap.h  v0.1
 * 格式完全照抄 CH55x_QMK_Lite / keyboards/RP17/keymap.h，只把 5x4 改成 5x5。
 *
 * ★ 与 config.h 的联动（改之前先读）
 *   TWO_LAYER_ENABLE 开 → DYNAMIC_KEYMAP_LAYER_COUNT=2，KEY_MAPS 必须正好 2 层
 *   TWO_LAYER_ENABLE 关 → 必须正好 1 层，否则数组维度不匹配
 *   本文件用 #ifdef 自动适配两者，切开关时不用改这里。
 *
 * ★ 但默认配置把 TWO_LAYER_ENABLE 关掉了，原因是 128B DataFlash 装不下：
 *   按 src/qmk/quantum/dynamic_keymap.h 的分区公式
 *     G 版: config_end=6, keymap 2层=100B, tapdance=12B -> 宏只剩 10B
 *     G 版: config_end=6, keymap 1层= 50B, tapdance=12B -> 宏有   60B
 *     T 版(开 RGB+编码器): 2层 100B -> macro=-3B，直接越界写 EEPROM
 *     T 版(开 RGB+编码器): 1层  50B -> macro= 51B，安全
 *   VIA 改键本身不依赖双层，单层照样能改任意键，所以先保 VIA + 宏。
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
 * 共 22 个实体键、25 个矩阵位，3 个 KC_NO = (1,4) (3,4) (4,3)。
 *
 * ⚠ (r,c) 是**电气坐标**，不是物理 x/y：物理 row1 col4 的 2U 高 "+" 键
 *   电气上接 (2,4)，物理 row3 col4 的 2U 高 Enter 接 (4,4)，
 *   物理 row4 col3 的 "." 接 (4,2)。DS22Lite_via.json 的 layout 数组顺序和
 *   {"h":2}/{"w":2} 保留物理形状，只改这三个键的 "r,c" 标签。
 *   键码按网表开关名（丝印）填，不再是占位推测；VIA 里仍可任意改。
 * ==========================================================================*/

// clang-format off

/* ---- 基础层：按网表丝印填，DEL/NUM 与导航列在左；FN = TO(1) -------------- */
#define LAYER_0 {                                            \
   {KC_DEL,   KC_NUM,   KC_PSLS,  KC_PAST,  KC_PMNS},      \
   {KC_HOME,  KC_P7,    KC_P8,    KC_P9,    KC_NO},        \
   {KC_PGUP,  KC_P4,    KC_P5,    KC_P6,    KC_PPLS},      \
   {KC_PGDN,  KC_P1,    KC_P2,    KC_P3,    KC_NO},        \
   {KC_TO_1,  KC_P0,    KC_PDOT,  KC_NO,    KC_PENT},      \
}

/* ---- 第二层：仅当 TWO_LAYER_ENABLE 打开时参与编译 ----------------------- */
/* 导航 / 媒体层，键码跟着物理键迁移到新电气坐标：+ 键→(2,4)、
   Enter→(4,4)、. 键→(4,2)；TO(0) 切回基础层。默认单层配置下不编译。 */
#define LAYER_1 {                                            \
   {KC_TO_0,  KC_MUTE,  KC_VOLD,  KC_VOLU,  KC_DEL},       \
   {KC_HOME,  KC_UP,    KC_END,   KC_BSPC,  KC_NO},        \
   {KC_LEFT,  KC_DOWN,  KC_RIGHT, KC_ENT,   KC_PGUP},      \
   {C(KC_X),  C(KC_C),  C(KC_V),  C(KC_Z),  KC_NO},        \
   {KC_SPC,   KC_BSPC,  KC_DEL,   KC_NO,    KC_SPC},       \
}

#ifdef TWO_LAYER_ENABLE
#define KEY_MAPS { LAYER_0, LAYER_1, };
#else
#define KEY_MAPS { LAYER_0, };
#endif

/* ---- 编码器：20:07 T 版已关 ENCODER_ENABLE（板上无 EC11）----------------- */
/* ENCODER_MAP 无条件定义是沿用 RP17 写法；dynamic_keymap.c 里由
   #ifdef ENCODER_ENABLE 包住，关掉时不生效、不占 DataFlash，不用删。 */
#define ENCODER_MAP {                                        \
    {ENCODER_CCW_CW(KC_MS_WH_UP, KC_MS_WH_DOWN)},           \
    {ENCODER_CCW_CW(KC_VOLU, KC_VOLD)},                     \
};

/* ---- RGB 灯位映射：22 颗灯 + 3 个 NO_LED（仅 T 版 / 出货全贴版用） ------ */
/* 第一部分 [row][col] -> LED 编号，NO_LED 必须落在 3 个 KC_NO 位。
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
