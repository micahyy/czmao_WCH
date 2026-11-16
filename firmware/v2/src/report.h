#pragma once
/* v2 HID 报告结构（与 QMK 数值保持一致，VIA/主机无需改驱动） */
#include <stdint.h>
#include <stdbool.h>
#include "modifiers.h"

#define KEYBOARD_REPORT_KEYS    6
#define KEYBOARD_REPORT_SIZE    (KEYBOARD_REPORT_KEYS + 3)

enum hid_report_ids {
    REPORT_ID_KEYBOARD = 1,
    REPORT_ID_SYSTEM,
    REPORT_ID_CONSUMER,
    REPORT_ID_MOUSE,
    REPORT_ID_DIAL,
};

typedef union {
    uint8_t raw[KEYBOARD_REPORT_SIZE];
    struct {
        uint8_t report_id;
        uint8_t mods;
        uint8_t reserved;
        uint8_t keys[KEYBOARD_REPORT_KEYS];
    };
} report_keyboard_t;

typedef struct {
    uint8_t report_id;
    uint16_t usage;
} report_extra_t;

typedef struct {
    uint8_t report_id;
    uint8_t buttons;
    int8_t x, y, v;
} report_mouse_t;

typedef struct {
    uint8_t report_id;
    uint16_t dial;
} report_dial_t;

/* keycode -> 各报告 usage 的映射在 keymap.c 里实现 */
uint16_t keycode_to_consumer(uint16_t keycode);
uint16_t keycode_to_system(uint16_t keycode);
