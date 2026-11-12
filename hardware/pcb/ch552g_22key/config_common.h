#pragma once
/* ============================================================================
 * 用法：把本文件覆盖框架里的 keyboards/config_common.h
 *       （原文件只有两行：#include "RP17/config.h" / "RP17/keymap.h"）
 *
 * 前提：把本目录整个复制成 keyboards/DS22Lite/，并且
 *       - 选 G 版：保留 config.h，删掉 config_ch552t.h
 *       - 选 T 版：把 config_ch552t.h 改名为 config.h（覆盖 G 版那份）
 *       DS22Lite_via.json 不参与编译，是给 VIA 网页手动导入用的。
 *
 * 编译环境（README 原话）：Arduino IDE + CH55xduino 0.0.18，
 * USB Settings 选 "USER CODE w/ 266B ram"，打开 CH55x_QMK_Lite.ino 编译上传。
 * ==========================================================================*/

#include "DS22Lite/config.h"
#include "DS22Lite/keymap.h"
