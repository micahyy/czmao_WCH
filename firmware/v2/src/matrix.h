#pragma once
#include <stdint.h>
extern __xdata uint8_t matrix_state[MATRIX_ROWS];
void matrix_init(void);
void matrix_scan(void);
uint8_t matrix_is_pressed(uint8_t r, uint8_t c);
/* 诊断态"上电按住任意键=跳过持久化"由 main.c 直接读 matrix_state[] 实现，
 * 不新增函数（直接寻址内部 RAM 已满，新函数的局部变量会挤爆 OSEG）。 */
