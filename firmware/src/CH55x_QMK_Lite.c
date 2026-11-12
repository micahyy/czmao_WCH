
#include "qmk/quantum/keyboard.h"
#include "ch55xduino/USBHID.h"

/* 只前向声明，不直接 include via.h：via.h 会牵动 RGB EEPROM 地址宏的
 * include 顺序，抢先包含会让 rgb_matrix.h 里的枚举拿不到常量。 */
void via_task(void);

void ch55xQmkLite_init(void)
{
    USBInit();
    
    keyboard_init();
}

void ch55xQmkLite_task(void)
{
    keyboard_task();
    via_task();   // 恢复出厂 / 进 bootloader 的回包后延迟动作
}

