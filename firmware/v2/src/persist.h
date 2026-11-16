#pragma once

#include <stdint.h>
#include <stdbool.h>
#include "config.h"

/* ---- 运行时结构：action.c 与 persist.c 共用，保证 Flash 布局一致 --------- */
typedef struct {
    uint8_t len;
    uint8_t keys[MACRO_LEN];
} macro_t;

/* td_t / TD 运行时结构已整链路移除（Flash 布局保留 TD 区 20B 不动，
 * 以免 PF_CODE_BASE/ROM 上限变动） */

extern __xdata macro_t macros[];

/* 宏掉电保持（DataFlash）
 *
 * CH552 应用侧可反复改写（不需先擦）的非易失存储只有 128B DataFlash，
 * 宏就存在其中 DF_OFF_MACRO.. 区（见 config.h 分区）。旧实现写 CodeFlash
 * 用户区顶端，但应用内 IAP 只能把位 1->0、无法擦回 1，宏内容一改即写坏，
 * 断电读回乱码 —— 故迁到 DataFlash。
 */
void persist_load(void);          /* 上电加载全部 */
void persist_save_macro(uint8_t slot);
void persist_wipe(void);          /* 恢复出厂：失效持久化区 */
