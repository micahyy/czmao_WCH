#pragma once
/* ============================================================================
 * DS22Lite v2 自定义键码 —— 数值与 QMK/VIA 完全一致，保证旧 VIA 网页能直接改键
 * ==========================================================================*/
#include "keycodes.h"

/* QMK 自定义键区起点；VIA 的 customKeycodes 下标 = 0x7E00 + i */
#define QK_KB_0                 0x7E00

enum v2_user_keycodes {
    DIAL_CCW = QK_KB_0,
    DIAL_BUTTON,
    DIAL_CW,

    KC_TO_0,
    KC_TO_1,
    KC_MO_0,
    KC_MO_1,

    KC_TD_0,
    KC_TD_1,
    KC_TD_2,
    KC_TD_3,
    KC_TD_4,

    RGB_MODE_U,
    RGB_HUI_U,
    RGB_HUD_U,
    RGB_SAI_U,
    RGB_SAD_U,
    RGB_VAI_U,
    RGB_VAD_U,
    RGB_SPI_U,
    RGB_SPD_U,

    /* 只能追加在末尾：VIA 按下标取键码 */
    KC_TO_2,
    KC_TO_3,
    KC_MO_2,
    KC_MO_3,
};

/* 动态宏：QMK QK_MACRO 区 */
#define QK_MACRO                0x7700
#define KC_M0                   (QK_MACRO + 0)
#define KC_M1                   (QK_MACRO + 1)
#define KC_M2                   (QK_MACRO + 2)
#define KC_M3                   (QK_MACRO + 3)
#define KC_M4                   (QK_MACRO + 4)
#define KC_M5                   (QK_MACRO + 5)
#define KC_M6                   (QK_MACRO + 6)
#define KC_M7                   (QK_MACRO + 7)

/* 编码用：C(kc) = 左 Ctrl + kc（与 QMK LCTL 数值一致） */
#define C(kc)                   (0x0100 | (kc))

/* F13..F21 用 keycodes.h 里的标值（0x68..0x70）。
 * 这里曾经重定义成 0x46..0x4E —— 那是 PRINT/SCRLK/PAUSE/INSERT/HOME/PGUP/
 * DEL/END/PGDN 的地盘，press_code 的 F13..F21→鼠标劫持会把整个导航键区
 * （含 Col0 的 HOME/PGUP/PGDN）吞掉不发键报 —— Col0“无触发”的真因。*/
