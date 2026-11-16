#include <stdint.h>
#include "config.h"
#include "persist.h"
#include "storage.h"

/* ============================================================================
 * 宏掉电保持：存 DataFlash DF_OFF_MACRO..（128B 里的一段，见 config.h 分区）。
 *
 * 为什么不用 CodeFlash：CH552 应用侧 IAP 写 Code 区只能把位 1->0，无法在固件
 * 内擦回 1（擦除要经 USB bootloader 整片擦）。首次写 magic 能落，改一次宏的
 * 数据字节要 0->1 就写坏 -> 断电读回 0xFF 乱码。DataFlash 走 ROM_CMD_WRITE，
 * 能像 EEPROM 一样单字节反复改写、不需先擦（键表一直就是这么保存的），
 * 所以宏迁到 DataFlash。键表相应改「差量存储」为宏腾出空间。
 *
 * 布局（相对 DF_OFF_MACRO）：
 *   +0            保存标记 MACRO_MARK（0x5A）；非此值 => 从未保存，用默认宏
 *   +1 + i*9      第 i 条宏：len(1B) + keys[MACRO_LEN]（MACRO_LEN=8）
 *   共 1 + 8*9 = 73B，正好落在 DF_MACRO_CAP 内
 *
 * 每条独立保存（VIA 每次落一个槽调一次），只写该槽 9B，阻塞最短。
 * ==========================================================================*/
#define MACRO_MARK    0x5A
#define MACRO_STRIDE  (1 + MACRO_LEN)      /* 每条 9B：len + keys[8] */

void persist_load(void) {
    uint8_t i, j, base, len;
    if (storage_read(DF_OFF_MACRO) != MACRO_MARK) return;   /* 未保存过：保留 action_init 默认 */
    for (i = 0; i < MACRO_COUNT; i++) {
        base = DF_OFF_MACRO + 1 + i * MACRO_STRIDE;
        len = storage_read(base);
        if (len > MACRO_LEN) len = 0;                        /* 越界/擦除态 => 视为空宏 */
        macros[i].len = len;
        for (j = 0; j < MACRO_LEN; j++)
            macros[i].keys[j] = (j < len) ? storage_read(base + 1 + j) : 0;
    }
}

/* 诊断档只测「上电加载（只读）」这一条路径，不写宏；写函数置空省 ROM
 * （诊断档 = 完整版 + 诊断指示，ROM 已顶满，少一个调用点展开就可能过）。*/
#ifdef DIAG
void persist_save_macro(uint8_t slot) { (void)slot; }
void persist_wipe(void) { }
#else
void persist_save_macro(uint8_t slot) {
    uint8_t j, base;
    if (slot >= MACRO_COUNT) return;
    storage_write(DF_OFF_MACRO, MACRO_MARK);                 /* 标记区已初始化 */
    base = DF_OFF_MACRO + 1 + slot * MACRO_STRIDE;
    storage_write(base, macros[slot].len);
    for (j = 0; j < MACRO_LEN; j++) storage_write(base + 1 + j, macros[slot].keys[j]);
}

/* 恢复出厂：清标记 => 下次上电 persist_load 直接返回，宏回 action_init 默认 */
void persist_wipe(void) {
    storage_write(DF_OFF_MACRO, 0x00);
}
#endif  /* !DIAG */
