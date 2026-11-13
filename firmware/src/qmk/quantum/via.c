/* Copyright 2019 Jason Williams (Wilba)
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "quantum.h"
#include "via.h"
#include "dynamic_keymap.h"
#include "../platforms/timer.h"
#include <Arduino.h>

static uint16_t eeprom_save_interval;

/* 恢复出厂 / 进 bootloader 走「先回包、后执行」：raw_hid_receive() 只挂待办，
 * via_task() 等 HID 回包发出后才重建 EEPROM + 软复位。否则 105 次 DataFlash
 * 写会拖住 VIA 命令响应，或在回包前复位把响应吞掉。
 * 软复位用 CH552 的 bSW_RESET（热复位 bBOOT_LOAD=0，重启直接跑 app），USB
 * 重新枚举后 VIA 才会重新拉一遍键表 / 灯效，界面显示恢复后的默认值。 */
#define VIA_PENDING_NONE     0
#define VIA_PENDING_FACTORY  1
#define VIA_PENDING_BOOT     2
#define VIA_ACTION_DELAY_MS  120

static __data uint8_t  via_pending_action = VIA_PENDING_NONE;
static __data uint16_t via_pending_since = 0;

// Called by QMK core to initialize dynamic keymaps etc.
void via_init(void) {
    // If the EEPROM has the magic, the data is good.
    // OK to load from EEPROM.
    if (!(eeprom_read_byte(VIA_EEPROM_MAGIC_ADDR) == VIA_MAGIC_CODE)) {
        eeconfig_init_via();
    }
}

void eeconfig_init_via(void) {
    // This resets the keymaps in EEPROM to what is in flash.
    dynamic_keymap_reset();
#ifdef DYNAMIC_MACRO_ENABLE
    // This resets the macros in EEPROM to nothing.
    dynamic_macro_reset();
    // This resets the macro loop in EEPROM to nothing.
    dynamic_macro_loop_reset();
#endif
#ifdef RGB_MATRIX_ENABLE
    // This resets the RGB Matrix settings in EEPROM to nothing.
    rgb_matrix_reset();
#endif //RGB_MATRIX_ENABLE
#ifdef TAP_DANCE_ENABLE
    // This resets the tap dance settings in EEPROM to nothing.
    dynamic_tap_dance_reset();
#endif //TAP_DANCE_ENABLE
    // Save the magic number last, in case saving was interrupted
    eeprom_write_byte(VIA_EEPROM_MAGIC_ADDR, VIA_MAGIC_CODE);
}

void bootloader_jump(void) {
    USB_CTRL = 0;
    EA = 0;	        // Disabling all interrupts is required.
    TMOD = 0;
    __asm__ ("lcall #0x3800"); // Jump to bootloader code
    while (1);
}

void via_factory_reset(void) {
    if (via_pending_action != VIA_PENDING_NONE) return;
    via_pending_action = VIA_PENDING_FACTORY;
    via_pending_since = timer_read();
}

void via_bootloader_jump(void) {
    if (via_pending_action != VIA_PENDING_NONE) return;
    via_pending_action = VIA_PENDING_BOOT;
    via_pending_since = timer_read();
}

static void via_soft_reset(void) {
    EA = 0;
    SAFE_MOD = 0x55;
    SAFE_MOD = 0xAA;
    GLOBAL_CFG |= bSW_RESET;   // 软件热复位：重启回 app，不进 0x3800 bootloader
    SAFE_MOD = 0;
    while (1);
}

void via_task(void) {
    uint8_t action;
    if (via_pending_action == VIA_PENDING_NONE) return;
    if (timer_elapsed(via_pending_since) < VIA_ACTION_DELAY_MS) return;
    action = via_pending_action;
    via_pending_action = VIA_PENDING_NONE;
    if (action == VIA_PENDING_FACTORY) {
        eeconfig_init_via();   // 键表 / RGB / magic 全部按 Flash 默认值重建
        via_soft_reset();      // USB 重新枚举 → VIA 重新读取默认键位
    } else {
        bootloader_jump();
    }
}

// Called by QMK core to process VIA-specific keycodes.
bool process_record_via(uint16_t keycode, keyrecord_t *record) {
    /* 恢复出厂 = QK_CLEAR_EEPROM(EE_CLR)、
     * 进 bootloader = QK_BOOT，系统键值和 VIA 按钮共用同一条路径。 */
    if (record->event.pressed) {
        switch (keycode) {
            case QK_CLEAR_EEPROM:
                via_factory_reset();
                return false;
            case QK_BOOTLOADER:
                via_bootloader_jump();
                return false;
        }
    }
#ifdef DYNAMIC_MACRO_ENABLE
    // Handle macros
    if (record->event.pressed) {
        if (keycode >= QK_MACRO && keycode <= QK_MACRO_MAX) {
            dynamic_macro_pressed(keycode - QK_MACRO);
            return false;
        }
    }
#else
    (void)keycode;
    (void)record;   /* 未启用宏时 process_record_via 恒返回 true */
#endif

    return true;
}

#ifdef RGB_MATRIX_ENABLE 
void via_qmk_rgb_matrix_command(__idata uint8_t *data)
{
    // data = [ command_id, channel_id, value_id, value_data ]
    __idata uint8_t *command_id = &(data[0]);
    __idata uint8_t *channel_id = &(data[1]);
    __idata uint8_t *value_id = &(data[2]);
    __idata uint8_t *value_data = &(data[3]);

    switch (*command_id) {
        case id_custom_set_value:
        case id_custom_save:
            if (timer_elapsed(eeprom_save_interval) >= VIA_EEPROM_SAVE_INTERVAL) {
                switch (*value_id) {
                    case id_qmk_rgb_matrix_effect:
                        rgb_matrix_set_mode(*value_data);
                        break;
#   ifdef RGB_EFFECTS_PLUS
                    case id_qmk_rgb_matrix_brightness:
                        rgb_matrix_set_val(*value_data);
                        break;
                    case id_qmk_rgb_matrix_effect_speed:
                        rgb_matrix_set_speed(*value_data);
                        break;
                    case id_qmk_rgb_matrix_color:
                        rgb_matrix_set_hs(*value_data, *(value_data + 1));
                        break;
#   endif //RGB_EFFECTS_PLUS
                    default:
#   ifndef RGB_EFFECTS_PLUS
                        if (*value_id >= id_qmk_rgb_matrix_color_red && *value_id <= id_qmk_rgb_matrix_color_blue) {
                            eeprom_write_byte(VIA_EEPROM_CUSTOM_RGB_MATRIX_ADDR + *value_id - 5, *value_data);
                        }
#   endif //RGB_EFFECTS_PLUS
                        break;
                }
                eeprom_save_interval = timer_read();
            }
            break;

        case id_custom_get_value:
            switch (*value_id) {
                case id_qmk_rgb_matrix_effect:
                    *value_data = eeprom_read_byte(RGB_MATRIX_EEPROM_ADDR_EFFECT);
                    break;
#   ifdef RGB_EFFECTS_PLUS
                case id_qmk_rgb_matrix_brightness:
                    *value_data = eeprom_read_byte(RGB_MATRIX_EEPROM_ADDR_VAL);
                    break;
                case id_qmk_rgb_matrix_effect_speed:
                    *value_data = eeprom_read_byte(RGB_MATRIX_EEPROM_ADDR_SPEED);
                    break;
                case id_qmk_rgb_matrix_color:
                    *value_data = eeprom_read_byte(RGB_MATRIX_EEPROM_ADDR_HUE);
                    *(value_data + 1) = eeprom_read_byte(RGB_MATRIX_EEPROM_ADDR_SAT);
                    break;
#   endif //RGB_EFFECTS_PLUS
                default:
#   ifndef RGB_EFFECTS_PLUS
                    if (*value_id >= id_qmk_rgb_matrix_color_red && *value_id <= id_qmk_rgb_matrix_color_blue) {
                        *value_data = eeprom_read_byte(VIA_EEPROM_CUSTOM_RGB_MATRIX_ADDR + *value_id - 5);
                    }
#   endif //RGB_EFFECTS_PLUS
                    break;
            }
            break;
    }
}
#endif //RGB_MATRIX_ENABLE

void via_custom_value_command(__idata uint8_t *data) {
    // data = [ command_id, channel_id, value_id, value_data ]
    __idata uint8_t *command_id = &(data[0]);
    __idata uint8_t *channel_id = &(data[1]);
    __idata uint8_t *value_id = &(data[2]);
    __idata uint8_t *value_data = &(data[3]);

    if (*channel_id == id_qmk_macro_loop_channel) {
#ifdef DYNAMIC_MACRO_ENABLE
        switch (*command_id) {
            case id_custom_set_value:
            case id_custom_save:
                if (*value_id >= id_qmk_macro_loop_m0 && *value_id <= id_qmk_macro_loop_m4) {
                    eeprom_write_byte(VIA_EEPROM_CUSTOM_MACRO_LOOP_ADDR + *value_id - 1, *value_data);
                }
                break;
            case id_custom_get_value:
                *value_data = eeprom_read_byte(VIA_EEPROM_CUSTOM_MACRO_LOOP_ADDR + *value_id - 1);
                break;
        }
#endif
        /* 宏功能未启用，这个 channel 不读写 EEPROM。addr 1..5 现在是
         * RGB 配置区，旧版 VIA 若来配宏循环会把 RGB 设置写坏。 */
        return;
    } else if (*channel_id == id_qmk_macro_system) {
        switch (*command_id) {
            case id_custom_set_value:
                /* 按钮带 value=1；旧版 toggle 关回 0 时不该再触发一次。 */
                if (*value_data == 0) break;
                switch (*value_id) {
                    case id_system_eeprom_reset:
                        via_factory_reset();
                        break;
                    case id_system_bootloader_jump:
                        via_bootloader_jump();
                        break;
                }
                break;
            case id_custom_save:
                /* VIA 在 set 之后必补发 save（channel 6，value_id=0）。动作
                 * 已在 set 里挂起，这里显式吞掉，不能重复触发也不能回 unhandled。 */
                break;
            case id_custom_get_value:
                /* 动作型控件在菜单加载时仍会 GET：回 0，不能回 id_unhandled，
                 * 否则 VIA 会把整个菜单响应判为错误。 */
                *value_data = 0;
                break;
            default:
                *command_id = id_unhandled;
                break;
        }
        return;
    }
#ifdef RGB_MATRIX_ENABLE 
    else if (*channel_id == id_qmk_rgb_matrix_channel) {
        via_qmk_rgb_matrix_command(data);
        return;
    }
#endif //RGB_MATRIX_ENABLE
#ifdef TAP_DANCE_ENABLE
    else if (*channel_id == id_qmk_tap_dance_channel) {
        __data uint16_t keycode;
        switch (*command_id) {
            case id_custom_set_value:
            case id_custom_save:
                if (*value_id >= id_qmk_tap_dance_td0 && *value_id <= id_qmk_tap_dance_td4) {
                    keycode = ((value_data[1] << 8) | value_data[2]);
                    dynamic_set_tap_dance_keycode(*value_id - 1, value_data[0], keycode);
                }
                break;
            case id_custom_get_value:
                keycode = dynamic_get_tap_dance_keycode(*value_id - 1, value_data[0]);
                value_data[1] = keycode >> 8;
                value_data[2] = keycode & 0xFF;
                break;
        }
        return;
    }
#endif //TAP_DANCE_ENABLE

    *command_id = id_unhandled;
}

void raw_hid_receive(__idata uint8_t *data, __idata uint8_t length) {
    __idata uint8_t *command_id   = &(data[0]);
    __idata uint8_t *command_data = &(data[1]);

    switch (*command_id) {
#ifdef RGB_MATRIX_ENABLE
        case id_signalrgb_stream_leds: {
            rgb_matrix_signalrgb_set_leds(data);
            break;
        }
        case id_signalrgb_effect_enable: {
            rgb_matrix_set_mode_noeeprom(RGB_MATRIX_SIGNAL_RGB);
            break;
        }
        case id_signalrgb_effect_disable: {
            rgb_matrix_reload_mode();
            break;
        }
#endif
        case id_get_protocol_version: {
            command_data[0] = VIA_PROTOCOL_VERSION >> 8;
            command_data[1] = VIA_PROTOCOL_VERSION & 0xFF;
            break;
        }
        case id_get_keyboard_value:
        case id_set_keyboard_value:
            break;
        case id_dynamic_keymap_get_keycode: {
            uint16_t keycode = dynamic_keymap_get_keycode(command_data[0], command_data[1], command_data[2]);
            command_data[3]  = keycode >> 8;
            command_data[4]  = keycode & 0xFF;
            break;
        }
        case id_dynamic_keymap_set_keycode: {
            dynamic_keymap_set_keycode(command_data[0], command_data[1], command_data[2], (command_data[3] << 8) | command_data[4]);
            break;
        }
        case id_dynamic_keymap_reset: {
            dynamic_keymap_reset();
            break;
        }
        case id_custom_set_value:
        case id_custom_get_value:
        case id_custom_save:
            via_custom_value_command(data);
            break;
        /* VIA Debug 页 / 新版系统菜单发的标准命令 0x0A / 0x0B。
         * 上游此处为空实现，现已接通。 */
        case id_eeprom_reset:
            via_factory_reset();
            break;
        case id_bootloader_jump:
            via_bootloader_jump();
            break;
        case id_dynamic_keymap_macro_get_count: {
            command_data[0] = DYNAMIC_KEYMAP_MACRO_COUNT;
            break;
        }
        case id_dynamic_keymap_macro_get_buffer_size: {
            uint16_t size   = DYNAMIC_KEYMAP_MACRO_EEPROM_SIZE;
            command_data[0] = size >> 8;
            command_data[1] = size & 0xFF;
            break;
        }
#ifdef DYNAMIC_MACRO_ENABLE
        case id_dynamic_keymap_macro_get_buffer: {
            for (uint8_t i = 0; i < command_data[2]; i++) {
                command_data[3 + i] = eeprom_read_byte(DYNAMIC_KEYMAP_MACRO_EEPROM_ADDR + command_data[1] + i);
            }
            break;
        }
        case id_dynamic_keymap_macro_set_buffer: {
            for (uint8_t i = 0; i < command_data[2]; i++) {
                eeprom_write_byte(DYNAMIC_KEYMAP_MACRO_EEPROM_ADDR + command_data[1] + i, command_data[3 + i]);
            }
            break;
        }
        case id_dynamic_keymap_macro_reset: {
            dynamic_macro_reset();
            break;
        }
#endif
        /* get_count / get_buffer_size 不包 #ifdef：VIA 仍会查询，
         * 返回 0 个宏 / 0 字节缓冲区，网页宏页显示为空而不是报错。 */
        case id_dynamic_keymap_get_layer_count: {
            command_data[0] = DYNAMIC_KEYMAP_LAYER_COUNT;
            break;
        }
        case id_dynamic_keymap_get_buffer: {
            __data uint16_t offset = (command_data[0] << 8) | command_data[1];
            for (uint8_t i = 0; i < command_data[2]; i++) {
                command_data[3 + i] = eeprom_read_byte(DYNAMIC_KEYMAP_EEPROM_ADDR + offset + i);
            }
            break;
        }
        case id_dynamic_keymap_set_buffer: {
            __data uint16_t offset = (command_data[0] << 8) | command_data[1];
            for (uint8_t i = 0; i < command_data[2]; i++) {
                eeprom_write_byte(DYNAMIC_KEYMAP_EEPROM_ADDR + offset + i, command_data[3 + i]);
            }
            break;
        }
#ifdef ENCODER_ENABLE
        case id_dynamic_keymap_get_encoder: {
            uint16_t keycode = dynamic_keymap_get_encoder(command_data[0], command_data[1], command_data[2] != 0);
            command_data[3]  = keycode >> 8;
            command_data[4]  = keycode & 0xFF;
            break;
        }
        case id_dynamic_keymap_set_encoder: {
            dynamic_keymap_set_encoder(command_data[0], command_data[1], command_data[2] != 0, (command_data[3] << 8) | command_data[4]);
            break;
        }
#endif
        default: {
            // The command ID is not known
            // Return the unhandled state
            *command_id = id_unhandled;
            break;
        }
    }

    // Return the same buffer, optionally with values changed
    // (i.e. returning state to the host, or the unhandled state).
    raw_hid_send(data, length);
}
