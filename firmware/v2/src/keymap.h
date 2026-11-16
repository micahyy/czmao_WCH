#pragma once
#include <stdint.h>

void keymap_flush(void);
void keymap_reset_default(void);
void keymap_init(void);
uint16_t keymap_get_keycode(uint8_t layer, uint8_t row, uint8_t col);
void keymap_set_keycode(uint8_t layer, uint8_t row, uint8_t col, uint16_t kc);
uint8_t keymap_buffer_byte(uint16_t offset);
void keymap_set_buffer_byte(uint16_t offset, uint8_t v);
