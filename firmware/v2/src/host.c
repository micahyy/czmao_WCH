#include <stdint.h>
#include <string.h>
#include "report.h"
#include "config.h"
#include "usb/USBHID.h"

/* 六键无冲键盘报告 + 独立鼠标/系统/媒体/拨盘报告 */
static __idata report_keyboard_t keyboard_report;
static __idata report_mouse_t mouse_report;
__idata uint8_t host_leds = 0;

void host_init(void) {
    memset(&keyboard_report, 0, sizeof(keyboard_report));
    memset(&mouse_report, 0, sizeof(mouse_report));
    keyboard_report.report_id = REPORT_ID_KEYBOARD;
    mouse_report.report_id = REPORT_ID_MOUSE;
    mouse_report.buttons = 0; mouse_report.x = mouse_report.y = mouse_report.v = 0;
}

static void kb_send(void) {
    USB_EP1_send(keyboard_report.raw, KEYBOARD_REPORT_SIZE);
}

void host_keyboard_set(uint8_t code, bool pressed) {
    uint8_t i;
    if (code >= 0xE0 && code <= 0xE7) {
        uint8_t bit = 1 << (code - 0xE0);
        if (pressed) keyboard_report.mods |= bit;
        else keyboard_report.mods &= ~bit;
    } else if (code == 0) {
        /* KC_NO：忽略 */
    } else {
        if (pressed) {
            for (i = 0; i < KEYBOARD_REPORT_KEYS; i++) {
                if (keyboard_report.keys[i] == code) break;
                if (keyboard_report.keys[i] == 0) { keyboard_report.keys[i] = code; break; }
            }
        } else {
            for (i = 0; i < KEYBOARD_REPORT_KEYS; i++) {
                if (keyboard_report.keys[i] == code) keyboard_report.keys[i] = 0;
            }
        }
    }
    kb_send();
}

void host_keyboard_clear(void) {
    keyboard_report.mods = 0;
    memset(keyboard_report.keys, 0, KEYBOARD_REPORT_KEYS);
    kb_send();
}

void host_mouse_button(uint8_t idx, bool pressed) {
    if (pressed) mouse_report.buttons |= (1 << idx);
    else mouse_report.buttons &= ~(1 << idx);
    USB_EP1_send((const __idata uint8_t *)&mouse_report, sizeof(report_mouse_t));
}

void host_mouse_move(int8_t x, int8_t y, int8_t v) {
    mouse_report.x = x; mouse_report.y = y; mouse_report.v = v;
    USB_EP1_send((const __idata uint8_t *)&mouse_report, sizeof(report_mouse_t));
    mouse_report.x = mouse_report.y = mouse_report.v = 0;
}

void host_consumer_send(uint16_t usage) {
    report_extra_t r = { REPORT_ID_CONSUMER, usage };
    USB_EP1_send((const __idata uint8_t *)&r, sizeof(r));
}

void host_system_send(uint16_t usage) {
    report_extra_t r = { REPORT_ID_SYSTEM, usage };
    USB_EP1_send((const __idata uint8_t *)&r, sizeof(r));
}

void host_dial_send(int16_t value) {
    report_dial_t r = { REPORT_ID_DIAL, (uint16_t)value };
    USB_EP1_send((const __idata uint8_t *)&r, sizeof(r));
}

uint8_t host_keyboard_leds(void) { return keyboard_leds(); }

/* ---- 键码 -> 消费电子 / 系统 usage --------------------------------------- */
/* Consumer 映射表：键码(0xA8..0xC2,均<0x100) -> HID Consumer usage，数值对齐
 * QMK tmk_core/protocol/report.{c,h} 的 usageKeymap（权威表），ROM 紧张故用表+循环
 * 替代 switch（旧 switch 还把 KC_CALCULATOR 错映射成 0x0183=AL_CC_CONFIG，
 * 正确应为 0x0192）。 */
static const __code uint8_t  ck_kc[] = {
    0xA8, 0xA9, 0xAA, 0xAB, 0xAC, 0xAD, 0xAE, 0xAF, 0xB0,
    0xB1, 0xB2, 0xB3, 0xB4, 0xB5, 0xB6, 0xB7, 0xB8, 0xB9,
    0xBA, 0xBB, 0xBC, 0xBD, 0xBE, 0xBF, 0xC0, 0xC1, 0xC2,
};
static const __code uint16_t ck_use[] = {
    0x00E2, 0x00E9, 0x00EA, 0x00B5, 0x00B6, 0x00B7, 0x00CD, 0x0183, 0x00B8,
    0x018A, 0x0192, 0x0194, 0x0221, 0x0223, 0x0224, 0x0225, 0x0226, 0x0227,
    0x022A, 0x00B3, 0x00B4, 0x006F, 0x0070, 0x019F, 0x01CB, 0x029F, 0x02A0,
};

uint16_t keycode_to_consumer(uint16_t kc) {
    uint8_t i;
    if (kc > 0xFF) return 0;                    /* consumer 全在 basic 区 */
    for (i = 0; i < (uint8_t)sizeof(ck_kc); i++)
        if (ck_kc[i] == (uint8_t)kc) return ck_use[i];
    return 0;
}

uint16_t keycode_to_system(uint16_t kc) {
    switch (kc) {
        case 0x00A5: return 0x0081;  /* KC_SYSTEM_POWER */
        case 0x00A6: return 0x0082;  /* KC_SYSTEM_SLEEP */
        case 0x00A7: return 0x0083;  /* KC_SYSTEM_WAKE */
        default: return 0;
    }
}
