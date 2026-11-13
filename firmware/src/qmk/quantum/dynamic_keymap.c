/* Copyright 2023 lbtrobit
 * Copyright 2017 Jason Williams (Wilba)
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

#include "../platforms/progmem.h"
#include "quantum.h"
#include "dynamic_keymap.h"
#include "via.h"
#include <stdint.h>
#include <ctype.h>

/* 维度用 TOTAL_LAYER_COUNT(4)，前 2 层是 VIA 可改的动态层，
 * 后 2 层是 Flash 常量静态层。见 dynamic_keymap.h 的注释。 */
const __code uint16_t keymaps[TOTAL_LAYER_COUNT][MATRIX_ROWS][MATRIX_COLS] = KEY_MAPS
#ifdef ENCODER_ENABLE
const __code uint16_t encoder_map[DYNAMIC_KEYMAP_LAYER_COUNT][NUM_ENCODERS][NUM_DIRECTIONS] = ENCODER_MAP
#endif // ENCODER_ENABLE

uint16_t dynamic_keymap_get_keycode(uint8_t layer, uint8_t row, uint8_t column) {
    /* 静态层：键表在 __code Flash，不读 DataFlash，也不受 128B 限制。 */
    if (layer >= DYNAMIC_KEYMAP_LAYER_COUNT) {
        if (layer >= TOTAL_LAYER_COUNT) return KC_NO;
        return pgm_read_word(&keymaps[layer][row][column]);
    }
    uint8_t address = DYNAMIC_KEYMAP_EEPROM_ADDR + (layer * MATRIX_ROWS * MATRIX_COLS * 2) + (row * MATRIX_COLS * 2) + (column * 2);
    // Big endian, so we can read/write EEPROM directly from host if we want
    uint16_t keycode = (eeprom_read_byte(address) << 8) + (eeprom_read_byte(address + 1) & 0xFF);
    return keycode;
}

void dynamic_keymap_set_keycode(uint8_t layer, uint8_t row, uint8_t column, uint16_t keycode) {
    /* 静态层只读：忽略 VIA 写入，改键请改 keymap.h 重编。 */
    if (layer >= DYNAMIC_KEYMAP_LAYER_COUNT) return;
    uint8_t address = DYNAMIC_KEYMAP_EEPROM_ADDR + (layer * MATRIX_ROWS * MATRIX_COLS * 2) + (row * MATRIX_COLS * 2) + (column * 2);
    // Big endian, so we can read/write EEPROM directly from host if we want
    eeprom_write_byte(address, (uint8_t)(keycode >> 8));
    eeprom_write_byte(address + 1, (uint8_t)(keycode & 0xFF));
}

#ifdef ENCODER_ENABLE
uint16_t dynamic_keymap_get_encoder(uint8_t layer, uint8_t encoder_id, bool clockwise) {
    uint8_t address = DYNAMIC_KEYMAP_ENCODER_EEPROM_ADDR + (layer * NUM_ENCODERS * 2 * 2) + (encoder_id * 2 * 2);
    // Big endian, so we can read/write EEPROM directly from host if we want
    uint16_t keycode = ((uint16_t)eeprom_read_byte(address + (clockwise ? 0 : 2))) << 8;
    keycode |= eeprom_read_byte(address + (clockwise ? 0 : 2) + 1);
    return keycode;
}

void dynamic_keymap_set_encoder(uint8_t layer, uint8_t encoder_id, bool clockwise, uint16_t keycode) {
    uint8_t address = DYNAMIC_KEYMAP_ENCODER_EEPROM_ADDR + (layer * NUM_ENCODERS * 2 * 2) + (encoder_id * 2 * 2);
    // Big endian, so we can read/write EEPROM directly from host if we want
    eeprom_write_byte(address + (clockwise ? 0 : 2), (uint8_t)(keycode >> 8));
    eeprom_write_byte(address + (clockwise ? 0 : 2) + 1, (uint8_t)(keycode & 0xFF));
}
#endif // ENCODER_ENABLE

void dynamic_keymap_reset(void) {
    // Reset the keymaps in EEPROM to what is in flash.
    for (uint8_t layer = 0; layer < DYNAMIC_KEYMAP_LAYER_COUNT; layer++) {
        for (uint8_t row = 0; row < MATRIX_ROWS; row++) {
            for (uint8_t column = 0; column < MATRIX_COLS; column++) {
                dynamic_keymap_set_keycode(layer, row, column, pgm_read_word(&keymaps[layer][row][column]));
            }
        }
    }
#ifdef ENCODER_ENABLE
    for (uint8_t layer = 0; layer < DYNAMIC_KEYMAP_LAYER_COUNT; layer++) {
        for (uint8_t encoder = 0; encoder < NUM_ENCODERS; encoder++) {
            dynamic_keymap_set_encoder(layer, encoder, true, pgm_read_word(&encoder_map[layer][encoder][true ? 0 : 1]));
            dynamic_keymap_set_encoder(layer, encoder, false, pgm_read_word(&encoder_map[layer][encoder][false ? 0 : 1]));
        }
    }
#endif // ENCODER_ENABLE
}
