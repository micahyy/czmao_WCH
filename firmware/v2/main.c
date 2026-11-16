#include <stdint.h>
#include <stdbool.h>
#include "config.h"
#include "matrix.h"
#include "keymap.h"
#include "action.h"
#include "host.h"
#include "rgb.h"
#include "storage.h"
#ifndef ROLLBACK
#include "persist.h"
#endif
#include "usb/USBInit.h"
#include <Arduino.h>

extern void via_task(void);      /* via.c：恢复出厂延迟执行主循环钩子 */

/* ============================================================================
 * DS22Lite v2 主循环 —— 不用 QMK 框架，事件驱动的极简主干。
 *   1ms tick：矩阵扫描 / 按键事件 / 宏与灯效
 *   每 10ms：同步 USB LED（NumLock 等）
 * ==========================================================================*/
static __idata uint8_t prev[MATRIX_ROWS];
#ifdef DIAG
static __idata uint8_t skip_persist;    /* 诊断：上电按住任意键 => 跳过持久化 */
#endif

static void process_matrix(void) {
    uint8_t r, c, now, diff;
    for (r = 0; r < MATRIX_ROWS; r++) {
        now = matrix_state[r];
        diff = now ^ prev[r];
        for (c = 0; c < MATRIX_COLS; c++) {
            if (diff & (1 << c)) action_process(r, c, (now & (1 << c)) != 0);
        }
        prev[r] = now;
    }
}

void setup(void) {
    uint8_t i;
    for (i = 0; i < MATRIX_ROWS; i++) prev[i] = 0;
    matrix_init();
    host_init();
    keymap_init();      /* DataFlash magic/压缩键表 -> XRAM 缓存 */
    rgb_init();
#ifdef DIAG
    /* 诊断固件：蓝灯 = 固件活着并已走到这里。
     * 若灯完全不亮 => 挂在 matrix_init/host_init/keymap_init/rgb_init 之前或之中。 */
    rgb_diag(1);
    /* 上电按住任意键 => 跳过持久化加载（矩阵层无消抖，一次 scan 即真实状态）。
     * 读完立刻同步 prev[]，避免把"按住"当成一次按键事件。
     * 不新增函数：直接寻址内部 RAM 已满，任何新函数局部变量都会挤爆 OSEG。 */
    matrix_scan();
    skip_persist = 0;
    for (i = 0; i < MATRIX_ROWS; i++) {
        prev[i] = matrix_state[i];            /* 同步 prev，避免按住被当成按键事件 */
        if (matrix_state[i]) skip_persist = 1; /* 上电按住任意键 => 跳过持久化 */
    }
#endif
    action_init();
    USBInit();
#ifdef DIAG
    /* 绿灯 = USBInit 已返回，USB 子系统已就绪（此时应能被主机枚举） */
    rgb_diag(2);
#endif
    /* 宏/TD 的掉电保持放最后加载：即使读 Code 区出意外，USB 也已经枚举，
     * 设备仍可识别，不会变成"插上没反应"。 */
#if !defined(ROLLBACK) && !defined(NO_PERSIST)
    persist_load();
#endif
#ifdef DIAG
    /* 白灯 = 持久化正常走完；蓝灯 = 本次已跳过持久化。
     * 停在绿灯 = persist_load 卡死。 */
    rgb_diag(skip_persist ? 1 : 3);
#endif
}

void loop(void) {
    static uint32_t last_ms = 0, last_led = 0;
    uint32_t now = millis();
    if ((int32_t)(now - last_ms) >= 0) {
        last_ms = now + DEBOUNCE_MS;
        matrix_scan();
        process_matrix();
        action_tick();
    }
    if ((int32_t)(now - last_led) >= 50) {
        last_led = now;
        host_leds = host_keyboard_leds();
    }
    raw_hid_task();
    via_task();       /* 处理延迟下来的恢复出厂（先回包后热复位） */
}

/* 入口由 ch55xduino core 的 main() 提供（init → setup → loop）。
 * **v2 里不要再自己写 main()**：SDCC 只把中断向量表生成在定义 main
 * 的编译单元里，build.py 若裁掉 core main 又无人发 IVT，USB/Timer0
 * 中断向量变垃圾，设备完全不枚举（历史事故，见 build.py patch_core_main 注释）。 */
