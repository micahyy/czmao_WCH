#pragma once
#include <stdint.h>
void rgb_init(void);
void rgb_save(void);
void rgb_set_layer(uint8_t layer);
void rgb_key_action(uint8_t act);
/* MO(FN) 按住期间全灯白色：action.c 直接置位，rgb_tick 检查后恢复 */
extern __idata uint8_t rgb_fn_flag;
void rgb_set_value(uint8_t channel, uint8_t value);
uint8_t rgb_get_value(uint8_t channel);
#ifdef DIAG
/* 诊断指示（仅 --diag 诊断档）：0=熄灭并恢复灯效，1=蓝，2=绿，3=白（常亮） */
void rgb_diag(uint8_t mode);
#endif
void rgb_tick(void);
