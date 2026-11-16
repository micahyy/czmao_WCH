#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include "config.h"
#include "keymap.h"
#include "matrix.h"
#include "host.h"
#include "action.h"
#include "rgb.h"
#ifndef ROLLBACK
#include "persist.h"      /* macro_t / td_t 定义在此，--no-persist 档也用 */
#endif
#include "v2_keycodes.h"
#include <Arduino.h>

/* ============================================================================
 * 动作引擎：v1 被砍的功能这里全捡回来。
 *   层：TO(n) 切换 / MO(n) 按住（框架缺陷修复：按下的键按当前层解释）
 *   宏：8 条 x 12 步，VIA 可录制（KC_M0..KC_M7），掉电保持（CodeFlash IAP）
 *   Tap Dance：5 组（单击/双击/长按）
 *   鼠标：按键/移动/滚轮 + F13..F21 映射
 *   拨盘：DIAL_CW/CCW/BUTTON
 * ==========================================================================*/

static __idata uint8_t base_layer = 0;       /* TO 切换后的默认层 */
static __idata uint8_t momentary_mask = 0;   /* 当前按住的 MO 层 bit */
static __idata uint8_t active_layer = 0;
static __idata bool    layer_changed = true;

/* 每个物理位置当前按下的键码（16bit），用于正确松键；25 个槽 */
static __xdata uint16_t pressed_kc[MATRIX_ROWS * MATRIX_COLS];

/* ---- 宏存储（RAM；掉电由 CodeFlash IAP 恢复，VIA 可下发/录制）------------ */
#ifdef ROLLBACK
/* ROLLBACK 档：完全回到「加持久化之前」的原状态（结构体本地定义 + static） */
typedef struct {
    uint8_t len;
    uint8_t keys[MACRO_LEN];
} macro_t;
static __xdata macro_t macros[MACRO_COUNT];
#else
/* macro_t / td_t 定义在 persist.h，与持久化层共用同一布局 */
__xdata macro_t macros[MACRO_COUNT];
#endif

/* 宏回放状态 */
static __idata uint8_t  play_slot = 0xFF, play_pos = 0;
static __idata uint32_t play_next_ms = 0;
/* play_phase: 0=待按下当前键 1=待松开刚按的普通键（连同其修饰键一并 clear） */
static __xdata uint8_t  play_phase;

/* ---- Tap Dance：已整链路移除（v1 同样裁掉，VIA json 标 DISABLED），
 * TD0..TD4 键码落在 press/release 末尾无动作分支，不占 ROM。 */

/* ---- 拨盘/鼠标累积 -------------------------------------------------------- */
static __idata int8_t dial_accum = 0;

/* 恢复出厂时给宏填默认（可从 VIA/服务器改） */
void action_init(void) {
    memset(pressed_kc, 0, sizeof(pressed_kc));
    memset(macros, 0, sizeof(macros));
    /* 给 M1 一个演示宏：Ctrl + X（剪切） */
    macros[1].len = 2;
    macros[1].keys[0] = 0xE0;   /* KC_LCTL */
    macros[1].keys[1] = 0x1B;   /* KC_X */
    /* 宏/TD 的掉电保持由 main.c 在 USBInit() 之后调 persist_load() 加载，
     * 保证即使读 Code 区出意外，USB 也已经枚举，设备不会"插上没反应"。 */
}

static uint8_t current_layer(void) {
    if (momentary_mask & (1 << 0)) return 0;
    if (momentary_mask & (1 << 1)) return 1;
    if (momentary_mask & (1 << 2)) return 2;
    if (momentary_mask & (1 << 3)) return 3;
    return base_layer;
}

/* ---- 单个键码按下/抬起 ---------------------------------------------------- */
/* QK_MODS 0x0100..0x1FFF：mods5=(kc>>8)&0x1F，bit4 选左/右手，低 4 位 = Ctrl/Shift/Alt/GUI 位掩码；
 * 基础键码 = kc&0xFF。press/release 共用一份，省 ROM（旧版把 C(kc) 单独写死且 &0x1F 砍坏 >=0x20 的键）*/
static void handle_mods(uint16_t kc, uint8_t down) {
    uint8_t mods5 = (uint8_t)((kc >> 8) & 0x1F), off = (mods5 & 0x10) ? 4 : 0, m;
    for (m = 0; m < 4; m++) if (mods5 & (1 << m)) host_keyboard_set((uint8_t)(0xE0 + off + m), down);
    host_keyboard_set((uint8_t)kc, down);
}

static void press_code(uint16_t kc) {
    uint8_t hi = (uint8_t)(kc >> 8), lo = (uint8_t)kc;
    if (kc >= QK_KB_0 && kc < QK_KB_0 + CUSTOM_KEYCODE_COUNT) {
        uint8_t u = (uint8_t)(kc - QK_KB_0);
        if (u >= 3 && u <= 6) {                 /* TO0/TO1/MO0/MO1 */
            if (u <= 4) base_layer = (uint8_t)(u - 3);
            else { momentary_mask |= (1 << (u - 5)); rgb_fn_flag = 1; }
            layer_changed = true;
            return;
        }
        if (u >= 21 && u <= 24) {               /* TO2/TO3/MO2/MO3（enum 尾 4 项，
                                                 * 曾误写 19..22：把 Spd+/Spd- 劫持成切层） */
            if (u <= 22) base_layer = (uint8_t)(u - 19);
            else { momentary_mask |= (1 << (u - 21)); rgb_fn_flag = 1; }
            layer_changed = true;
            return;
        }
        if (u >= 7 && u <= 11) {                /* TD0..TD4：已移除，无动作 */
            return;
        }
        if (u == 0) { host_dial_send(-1); return; }        /* DIAL_CCW */
        if (u == 2) { host_dial_send(1);  return; }        /* DIAL_CW */
        if (u == 1) { host_mouse_button(3, true); return; }/* DIAL_BUTTON */
        /* RGB 控制 u=12..20（Mode/Hue±/Sat±/Val±/Spd±，曾误写 12..18 漏掉 Spd） */
        if (u >= 12 && u <= 20) { rgb_key_action((uint8_t)(u - 12)); return; }
        return;
    }
    if (kc >= KC_M0 && kc <= KC_M7) {
        play_slot = (uint8_t)(kc - KC_M0);
        play_pos = 0;
        play_phase = 0;
        play_next_ms = millis();
        return;
    }
    if (hi == 0x52) { /* 层切换预留 0x52xx：TO(n)/MO(n) */
        if (lo <= 3) { base_layer = lo; layer_changed = true; }
        return;
    }
    if (kc >= QK_MODS && kc <= QK_MODS_MAX) { handle_mods(kc, 1); return; }
    if (kc >= 0x00D1 && kc <= 0x00D8) { host_mouse_button((uint8_t)(kc - 0x00D1), true); return; }
    if (kc >= 0x00CD && kc <= 0x00D0) {
        host_mouse_move(kc == 0x00CF ? -1 : (kc == 0x00D0 ? 1 : 0),
                        kc == 0x00CD ? -1 : (kc == 0x00CE ? 1 : 0), 0);
        return;
    }
    if (kc == 0x00D9) { host_mouse_move(0, 0, 1); return; }  /* WH_UP */
    if (kc == 0x00DA) { host_mouse_move(0, 0, -1); return; } /* WH_DOWN */
    if (kc == 0x00DB) { host_mouse_move(0, 0, -1); return; } /* WH_LEFT 并入垂直轮 */
    if (kc == 0x00DC) { host_mouse_move(0, 0, 1); return; }  /* WH_RIGHT */
    /* F13..F21 -> 鼠标（把 v1 的 FCODE_TO_MOUSE 真正启用） */
    if (kc >= KC_F13 && kc <= KC_F21) {
        uint8_t u = (uint8_t)(kc - KC_F13);
        if (u < 3) host_mouse_button(u, true);
        else if (u < 7) host_mouse_move(u == 4 ? -1 : (u == 5 ? 1 : 0), u == 3 ? -1 : (u == 6 ? 1 : 0), 0);
        else host_mouse_move(0, 0, (u == 7) ? 1 : -1);
        return;
    }
    {
        uint16_t c = keycode_to_consumer(kc);
        if (c) { host_consumer_send(c); return; }
        c = keycode_to_system(kc);
        if (c) { host_system_send(c); return; }
    }
    /* 未映射的 consumer/系统/鼠标区（0xA5..0xDF）必须拦截：
     * 否则落到兜底把 usage 当 basic 键码发出错键（COL0 同类劫持） */
    if (kc >= 0x00A5 && kc <= 0x00DF) return;
    if (kc <= 0xE7) host_keyboard_set((uint8_t)kc, true);
}

static void release_code(uint16_t kc) {
    if (kc >= QK_KB_0 && kc < QK_KB_0 + CUSTOM_KEYCODE_COUNT) {
        uint8_t u = (uint8_t)(kc - QK_KB_0);
        if (u >= 3 && u <= 6) {
            if (u <= 4) { /* TO 无松开动作 */ }
            else { momentary_mask &= ~(1 << (u - 5)); rgb_fn_flag = 0; layer_changed = true; }
            return;
        }
        if (u >= 21 && u <= 24) {
            if (u <= 22) { /* TO2/TO3 无松开动作 */ }
            else { momentary_mask &= ~(1 << (u - 21)); rgb_fn_flag = 0; layer_changed = true; }
            return;
        }
        if (u >= 7 && u <= 11) { /* TD 已移除 */
            return;
        }
        if (u == 1) { host_mouse_button(3, false); return; }
        return;
    }
    if (kc >= KC_M0 && kc <= KC_M7) return;
    if (kc >= QK_MODS && kc <= QK_MODS_MAX) { handle_mods(kc, 0); return; }
    if (kc >= 0x00D1 && kc <= 0x00D8) { host_mouse_button((uint8_t)(kc - 0x00D1), false); return; }
    if (kc >= 0x00A5 && kc <= 0x00DF) return;   /* 与 press_code 同步拦截（鼠标移动/滚轮、consumer 无松开动作） */
    if (kc <= 0xE7) host_keyboard_set((uint8_t)kc, false);
}

/* ---- 矩阵事件 ------------------------------------------------------------- */
void action_process(uint8_t row, uint8_t col, bool pressed) {
    uint8_t idx = (uint8_t)(row * MATRIX_COLS + col);
    uint16_t kc;
    if (pressed) {
        active_layer = current_layer();
        kc = keymap_get_keycode(active_layer, row, col);
        pressed_kc[idx] = kc;
        press_code(kc);
    } else {
        kc = pressed_kc[idx];
        pressed_kc[idx] = 0;
        if (kc) release_code(kc);
    }
}

/* ---- 1ms tick：宏回放 + Tap Dance 双击超时 + 灯效 ------------------------- */
void action_tick(void) {
    /* 宏回放（每 12ms 一步）：普通键「按下→下一步松开」= 一次 tap；修饰键
     * (0xE0..0xE7) 按住不放，直到其后第一个普通键松开时一并释放（支持 Ctrl+C）。
     * 旧逻辑只按下不中途松键，会把序列叠成和弦，导致录的宏不触发。 */
    if (play_slot != 0xFF && (int32_t)(millis() - play_next_ms) >= 0) {
        if (play_phase == 0) {
            if (play_pos >= macros[play_slot].len) {
                host_keyboard_clear();          /* 收尾：松开残留修饰键 */
                play_slot = 0xFF;
            } else {
                uint8_t k = macros[play_slot].keys[play_pos];
                if (k >= 0xE0 && k <= 0xE7) {
                    host_keyboard_set(k, true); /* 修饰键：按住 */
                    play_pos++;
                } else {
                    if (k) host_keyboard_set(k, true);
                    play_phase = 1;             /* 下一步 clear 松开它 */
                }
                play_next_ms = millis() + 12;
            }
        } else {
            host_keyboard_clear();              /* 松开刚按的普通键 + 其修饰键 */
            play_pos++;
            play_phase = 0;
            play_next_ms = millis() + 12;
        }
    }
    /* TD 已整链路移除 */
    if (layer_changed) { rgb_set_layer(current_layer()); layer_changed = false; }
    rgb_tick();
}

/* ---- VIA 接口：改宏 / 改 TD ---------------------------------------------- */
__xdata uint8_t macro_io[MACRO_LEN];

void action_macro_set_from_io(uint8_t slot, uint8_t len) {
    uint8_t i;
    if (slot >= MACRO_COUNT) return;
    if (len > MACRO_LEN) len = MACRO_LEN;
    macros[slot].len = len;
    for (i = 0; i < len; i++) macros[slot].keys[i] = macro_io[i];
#if !defined(ROLLBACK) && !defined(NO_PERSIST)
    persist_save_macro(slot);
#endif
}

uint8_t action_macro_get(uint8_t slot) {
    uint8_t i;
    if (slot >= MACRO_COUNT) return 0;
    for (i = 0; i < macros[slot].len; i++) macro_io[i] = macros[slot].keys[i];
    return macros[slot].len;
}

/* action_td_set 已随 TD 整链路移除（via.c 不再引用） */

#if !defined(ROLLBACK) && !defined(NO_PERSIST)
void action_factory_reset(void) {
    /* 只失效 CodeFlash 里的持久化区（RAM 里的宏/TD 下次上电时自然清空） */
    persist_wipe();
}
#endif

void action_macro_record(uint8_t slot) {
    /* 简化录制：把接下来按下的键依次写入宏（由 VIA 端先发 REC 命令） */
    play_slot = 0xFF;
    if (slot < MACRO_COUNT) {
        macros[slot].len = 0;
#if !defined(ROLLBACK) && !defined(NO_PERSIST)
        persist_save_macro(slot);
#endif
    }
}
