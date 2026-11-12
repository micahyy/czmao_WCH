/* DS22Lite — Arduino sketch 入口的 C 等价物。
 * Arduino IDE 用户直接用 CH55x_QMK_Lite.ino 即可（内容一致）。
 * 本文件只给 build.py 的便携 SDCC 构建链用。
 */
#include <Arduino.h>
#include "src/CH55x_QMK_Lite.h"

#ifndef USER_USB_RAM
#error "This example needs to be compiled with a USER USB setting"
#endif

void setup(void) {
  delay(1000);
  ch55xQmkLite_init();
}

void loop(void) {
  ch55xQmkLite_task();
}
