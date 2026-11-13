#pragma once
/* 目标板由 build.py 依据 boards.json 传入 -DBOARD_ID=<directory>。
 * 未定义时（例如直接用 Arduino IDE 编译）使用默认板，保持向后兼容。
 * 新增板卡无需修改本文件，只要把目录名写进 boards.json。
 */
#ifndef BOARD_ID
#define BOARD_ID DS22Lite
#endif

#define BC_STR(x) #x
#define BC_XSTR(x) BC_STR(x)
#define BC_PATH(dir, file) BC_XSTR(dir/file)
#include BC_PATH(BOARD_ID, config.h)
#include BC_PATH(BOARD_ID, keymap.h)
