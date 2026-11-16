#include <stdint.h>
#include "include/ch5xx.h"
#include "include/ch5xx_usb.h"

/* 跳 bootloader：独立编译单元，避开 SDCC 在 via.c 大 switch 里处理
 * 内联汇编 + while(1) 时的 SIGSEGV。 */
void bootloader_jump(void) {
    USB_CTRL = 0;
    EA = 0;
    TMOD = 0;
    __asm__("lcall #0x3800");
    while (1) {}
}

/* 恢复出厂后的热复位（对齐 v1 via_soft_reset）：CH552 bSW_RESET 重启直接跑 app，
 * USB 重新枚举后 VIA 才会重新拉一遍默认键表 / 灯效，界面即时反映恢复结果。
 * 独立编译单元：SDCC 在 via.c 大 switch 里内联 while(1) 会 SIGSEGV。 */
void soft_reset(void) {
    EA = 0;
    SAFE_MOD = 0x55;
    SAFE_MOD = 0xAA;
    GLOBAL_CFG |= bSW_RESET;
    SAFE_MOD = 0;
    while (1) {}
}
