#pragma once
#include <stdint.h>
#include <stdbool.h>

void action_init(void);
/* 每次矩阵扫描后调用，处理按下/松开的键码 */
void action_process(uint8_t row, uint8_t col, bool pressed);
/* 定时调用（1ms），驱动 tap dance / 宏回放 / 灯效 */
void action_tick(void);
/* VIA 触发宏录制：record_slot=0..7，或 0xFF 停止 */
void action_macro_record(uint8_t slot);
#if !defined(ROLLBACK) && !defined(NO_PERSIST)
/* 恢复出厂：失效 CodeFlash 里的宏/TD 配置 */
void action_factory_reset(void);
#endif

/* 宏读写经全局缓冲中转（SDCC 取 &data[4] 传参会 SIGSEGV） */
extern __xdata uint8_t macro_io[MACRO_LEN];
uint8_t action_macro_get(uint8_t slot);          /* 数据写 macro_io，返回长度 */
void action_macro_set_from_io(uint8_t slot, uint8_t len);
