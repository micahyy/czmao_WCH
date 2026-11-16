#pragma once
/* ============================================================================
 * DS22Lite v2 —— 4 层出厂默认键表 + 灯位映射
 *
 * 这些 __code 常量只是出厂默认值；VIA 改键后的副本在 DataFlash 压缩区，
 * 恢复出厂 / magic 不匹配时从这里重建。
 * 空位 (1,4) (3,4) (4,3) 必须填 KC_NO。
 * ==========================================================================*/
#include "src/v2_keycodes.h"

#define LAYER_0 {                                            \
   {KC_HOME,  KC_NUM,   KC_PSLS,  KC_PAST,  KC_PMNS},       \
   {KC_END,   KC_P7,    KC_P8,    KC_P9,    KC_NO},         \
   {KC_PGUP,  KC_P4,    KC_P5,    KC_P6,    KC_PPLS},       \
   {KC_PGDN,  KC_P1,    KC_P2,    KC_P3,    KC_NO},         \
   {KC_MO_1,  KC_P0,    KC_PDOT,  KC_NO,    KC_PENT},       \
}

#define LAYER_1 {                                            \
   {KC_TO_0,  KC_MUTE,  KC_VOLD,  KC_VOLU,  KC_DEL},        \
   {KC_HOME,  KC_UP,    KC_END,   KC_BSPC,  KC_NO},         \
   {KC_LEFT,  KC_DOWN,  KC_RIGHT, KC_ENT,   KC_PGUP},       \
   {C(KC_X),  C(KC_C),  C(KC_V),  C(KC_Z),  KC_NO},         \
   {KC_MO_1,  KC_BSPC,  KC_DEL,   KC_NO,    KC_SPC},        \
}

/* L2/L3 按用户要求留空（VIA 里仍是 4 层，自行填键）；TD/宏/鼠标入口
 * 已从 L3 移除，需要时直接在 VIA 里摆键。 */
#define LAYER_2 {                                            \
   {KC_NO,    KC_NO,    KC_NO,    KC_NO,    KC_NO},         \
   {KC_NO,    KC_NO,    KC_NO,    KC_NO,    KC_NO},         \
   {KC_NO,    KC_NO,    KC_NO,    KC_NO,    KC_NO},         \
   {KC_NO,    KC_NO,    KC_NO,    KC_NO,    KC_NO},         \
   {KC_NO,    KC_NO,    KC_NO,    KC_NO,    KC_NO},         \
}

#define LAYER_3 {                                            \
   {KC_NO,    KC_NO,    KC_NO,    KC_NO,    KC_NO},         \
   {KC_NO,    KC_NO,    KC_NO,    KC_NO,    KC_NO},         \
   {KC_NO,    KC_NO,    KC_NO,    KC_NO,    KC_NO},         \
   {KC_NO,    KC_NO,    KC_NO,    KC_NO,    KC_NO},         \
   {KC_NO,    KC_NO,    KC_NO,    KC_NO,    KC_NO},         \
}

#define KEY_MAPS { LAYER_0, LAYER_1, LAYER_2, LAYER_3, }

/* 灯位映射：[row][col] -> LED 编号；NO_LED 对应 3 个物理空位 */
#define NO_LED  0xFF
#define RGB_CONFIG { {                                      \
    {21, 20, 19, 18, 17},                                   \
    {16, 15, 14, 13, NO_LED},                               \
    {11, 10,  9,  8, 12},                                   \
    { 7,  6,  5,  4, NO_LED},                               \
    { 2,  1,  0, NO_LED, 3},                                \
}, {                                                        \
    {224,0}, {168,0}, {112,0}, {56,0}, {0,0},              \
    {224,24},{168,24},{112,24},{56,24},{0,24},             \
    {224,48},{168,48},{112,48},{56,48},                    \
    {224,72},{168,72},{112,72},{56,72},{0,72},             \
    {224,96},{168,96},{0,96},                              \
}};

#define RGB_MODE_DEFAULT    3       /* CYCLE_UP_DOWN */
#define RGB_HUE_DEFAULT     120
#define RGB_SAT_DEFAULT     255
#define RGB_VAL_DEFAULT     100
#define RGB_SPD_DEFAULT     50
