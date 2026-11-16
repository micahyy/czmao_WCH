#pragma once
#include <stdint.h>
#include <stdbool.h>
#include "report.h"

void host_init(void);
void host_keyboard_set(uint8_t code, bool pressed);
void host_keyboard_clear(void);
void host_mouse_button(uint8_t idx, bool pressed);
void host_mouse_move(int8_t x, int8_t y, int8_t v);
void host_consumer_send(uint16_t usage);
void host_system_send(uint16_t usage);
void host_dial_send(int16_t value);
uint8_t host_keyboard_leds(void);
extern __idata uint8_t host_leds;
