#pragma once
/* ============================================================================
 * DS22Lite v2 —— 板级配置（不依赖 QMK）
 *
 * 复用旧板的电气连接与 USB 身份，PID 改 0x4A51 以区分 v1 固件。
 * 引脚编号沿用框架规则：两位数 = 端口*10 + 位号（32 = P3.2）。
 * ==========================================================================*/

/* ---- USB 身份 ------------------------------------------------------------ */
#define VENDOR_ID               0x4A17
#define PRODUCT_ID              0x4A51
#define MANUFACTURER            'M','a','i','k','u'
#define PRODUCT                 'D','S','2','2','L','i','t','e',' ','v','2'

/* ---- 矩阵：5x5，COL2ROW -------------------------------------------------- */
#define MATRIX_ROWS             5
#define MATRIX_COLS             5
#define MATRIX_ROW_PINS         { 33, 34, 35, 13, 12 }   /* P3.3 P3.4 P3.5 P1.3 P1.2 */
#define MATRIX_COL_PINS         { 32, 14, 15, 16, 17 }   /* P3.2 P1.4 P1.5 P1.6 P1.7 */
#define MATRIX_IO_DELAY         3
#define DEBOUNCE_MS             5

/* ---- RGB：22 颗 WS2812，DIN = P1.0 -------------------------------------- */
#define RGB_ENABLE              1
#define WS2812_DI_PIN           10
#define RGB_LED_COUNT           22

/* ---- 功能开关：v2 目标是把 v1 砍掉的都捡回来 ---------------------------- */
#define LAYER_COUNT             4          /* VIA 可见可改的全部层数 */
#define CUSTOM_KEYCODE_COUNT    25         /* 与 DS22Lite_v2_via.json 下标一一对应 */
#define MACRO_COUNT             8          /* 动态宏 0..7 */
#define MACRO_LEN               8          /* 每条宏最大步数；缩小以腾出 DataFlash 给宏持久化（见下分区） */
#define TAP_DANCE_COUNT         5          /* TD0..TD4 */

/* 空位：(1,4) (3,4) (4,3) 物理不装键，也不存键码 */
#define IS_EMPTY_CELL(r, c)     (((r) == 1 && (c) == 4) || ((r) == 3 && (c) == 4) || ((r) == 4 && (c) == 3))

/* ---- DataFlash 128B 分区 -------------------------------------------------
 * CH552 唯一可像 EEPROM 反复改写（不需先擦）的非易失存储只有这 128B；
 * CodeFlash 应用侧只能把位 1->0、不能在固件内擦回 1，改一次宏即写坏。
 * 因此键表 + RGB + 宏全部要塞进 128B：键表改「差量存储」（只存与出厂默认
 * 不同的键，出厂状态仅 ~5B），把大头让给宏。
 *   0..1   格式/magic（当前仅作占位，各分区自带有效性标记）
 *   2..6   RGB 配置 5B（mode/hue/sat/val/speed）
 *   7..54  键表差量 48B（首字节 KM_MARK=有效，否则用 board.h 默认）
 *   55..127 宏 73B（首字节 MACRO_MARK=已保存，后接 8×(len+keys[8])=72B）*/
#define DF_SIZE                 128
#define DF_OFF_MAGIC            0
#define DF_OFF_RGB              2
#define DF_OFF_KEYMAP           7
#define DF_KEYMAP_CAP           48                      /* 键表差量区（7..54） */
#define DF_OFF_MACRO            (DF_OFF_KEYMAP + DF_KEYMAP_CAP)   /* 55 */
#define DF_MACRO_CAP            (DF_SIZE - DF_OFF_MACRO)          /* 73 = 标记1 + 8*9 */
#define DF_MAGIC                0x5E

/* VIA 协议版本与 magic：改变分区/命令布局时同步递增 */
#define VIA_PROTOCOL_VERSION    0x000C
#define VIA_MAGIC_CODE          0x5E
#define VIA_EEPROM_CONFIG_END   DF_OFF_KEYMAP

/* ---- Raw HID 通道 -------------------------------------------------------- */
#define RAW_HID_SIZE            32
#define EP2_SIZE                32
