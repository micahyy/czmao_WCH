/* Copyright 2017 Jason Williams (Wilba)
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
#pragma once

#include <stdint.h>
#include <stdbool.h>
#include "quantum.h"
#include "dynamic_macro.h"
#include "dynamic_tap_dance.h"
#ifdef ENCODER_ENABLE
#    include "encoder.h"
#endif

#ifdef TWO_LAYER_ENABLE
#define DYNAMIC_KEYMAP_LAYER_COUNT          2
#else
#define DYNAMIC_KEYMAP_LAYER_COUNT          1
#endif

/* DS22Lite 19:44：总层数可以大于「动态层数」。
 * DYNAMIC_KEYMAP_LAYER_COUNT 是 VIA 协议上报的层数，决定 DataFlash 里
 * 存几套键表（每层 5x5x2=50B）。CH552 只有 128B DataFlash，4 层动态
 * 键表要 200B，物理装不下。
 * 所以引入 TOTAL_LAYER_COUNT：前 DYNAMIC_KEYMAP_LAYER_COUNT 层走 EEPROM
 * （VIA 可改键），后面的层是「静态层」，键表放 __code Flash 常量，
 * VIA 看不到也改不了。dynamic_keymap.c 的 get/set 会分流。
 * 默认相等，上游其它键盘不受影响。 */
#ifndef TOTAL_LAYER_COUNT
#   define TOTAL_LAYER_COUNT                DYNAMIC_KEYMAP_LAYER_COUNT
#endif

#define TOTAL_EEPROM_BYTE_COUNT             128   /* CH552 物理容量，不是本配置用量 */

/* 19:44 砍宏：DYNAMIC_MACRO_ENABLE 关时宏区归零、VIA 上报 0 个宏。
 * 本配置实际 DataFlash 只用 6(config) + 100(keymap) = 106B。 */
#ifdef DYNAMIC_MACRO_ENABLE
#   define DYNAMIC_KEYMAP_MACRO_COUNT       5
#else
#   define DYNAMIC_KEYMAP_MACRO_COUNT       0
#endif
#define DYNAMIC_KEYMAP_EEPROM_START         (VIA_EEPROM_CONFIG_END)
#define DYNAMIC_KEYMAP_EEPROM_MAX_ADDR      (TOTAL_EEPROM_BYTE_COUNT - 1)
#define DYNAMIC_KEYMAP_EEPROM_ADDR          DYNAMIC_KEYMAP_EEPROM_START
#define DYNAMIC_KEYMAP_ENCODER_EEPROM_ADDR  (DYNAMIC_KEYMAP_EEPROM_ADDR + (DYNAMIC_KEYMAP_LAYER_COUNT * MATRIX_ROWS * MATRIX_COLS * 2))

#ifdef ENCODER_ENABLE
#   define DYNAMIC_TAP_DANCE_EEPROM_ADDR    (DYNAMIC_KEYMAP_ENCODER_EEPROM_ADDR + (DYNAMIC_KEYMAP_LAYER_COUNT * NUM_ENCODERS * 2 * 2))
#else
#   define DYNAMIC_TAP_DANCE_EEPROM_ADDR    DYNAMIC_KEYMAP_ENCODER_EEPROM_ADDR
#endif // ENCODER_ENABLE

#ifdef TAP_DANCE_ENABLE
#   define DYNAMIC_TAP_DANCE_EEPROM_SIZE    (TAP_DANCE_COUNT * 2 * 3)  
#   define DYNAMIC_KEYMAP_MACRO_EEPROM_ADDR (DYNAMIC_TAP_DANCE_EEPROM_ADDR + DYNAMIC_TAP_DANCE_EEPROM_SIZE)
#else
#   define DYNAMIC_KEYMAP_MACRO_EEPROM_ADDR DYNAMIC_TAP_DANCE_EEPROM_ADDR
#endif // TAP_DANCE_ENABLE

#ifdef DYNAMIC_MACRO_ENABLE
#   define DYNAMIC_KEYMAP_MACRO_EEPROM_SIZE (DYNAMIC_KEYMAP_EEPROM_MAX_ADDR - DYNAMIC_KEYMAP_MACRO_EEPROM_ADDR + 1)
#else
#   define DYNAMIC_KEYMAP_MACRO_EEPROM_SIZE 0
#endif

uint16_t dynamic_keymap_get_keycode(uint8_t layer, uint8_t row, uint8_t column);
void     dynamic_keymap_set_keycode(uint8_t layer, uint8_t row, uint8_t column, uint16_t keycode);
#ifdef ENCODER_ENABLE
uint16_t dynamic_keymap_get_encoder(uint8_t layer, uint8_t encoder_id, bool clockwise);
void     dynamic_keymap_set_encoder(uint8_t layer, uint8_t encoder_id, bool clockwise, uint16_t keycode);
#endif // ENCODER_ENABLE
void dynamic_keymap_reset(void);
