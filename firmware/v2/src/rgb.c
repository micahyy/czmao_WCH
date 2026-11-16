#include <stdint.h>
#include <string.h>
#include "config.h"
#include "board.h"
#include "storage.h"
#include "rgb.h"
#include "usb/USBHID.h"
#include <WS2812.h>
#include <Arduino.h>

/* ============================================================================
 * 22 颗 WS2812：复用旧板验证过的 WS2812 汇编库，色相环灯效。
 * 配置 5B 存 DataFlash addr 2..6：mode/h/s/v/speed。
 * ==========================================================================*/
#define LED_COUNT   RGB_LED_COUNT

typedef struct { uint8_t g, r, b; } LED_TYPE;   /* GRB 顺序，与 WS2812 库一致 */

typedef struct {
    uint8_t mode, hue, sat, val, speed;
} rgb_cfg_t;

/* 直接寻址内部 RAM（DSEG/OSEG）已顶满：本文件全部静态量改 __idata，
 * 把 128B 直接寻址区让给链接器的 overlay 分析（历史经验见旧注释）。 */
static __idata rgb_cfg_t cfg;
static __xdata LED_TYPE leds[LED_COUNT];
static __idata uint8_t layer_hue = 0;
static __idata uint16_t tick_count = 0;
static __idata uint8_t dirty = 1;
__idata uint8_t rgb_fn_flag = 0;      /* MO(FN) 按住标志：LED19 白色（action.c 直接赋值，不开函数省 ROM） */
#ifdef DIAG
static __idata uint8_t diag_mode = 0;  /* 1=诊断常亮，禁止灯效覆盖（放 idata：直接寻址区已满） */
#endif

/* 键位 -> LED 编号，来自 keymap.h 的 RGB_CONFIG 第一段 */
static const __code uint8_t led_map[MATRIX_ROWS][MATRIX_COLS] = {
    {21, 20, 19, 18, 17},
    {16, 15, 14, 13, 0xFF},
    {11, 10,  9,  8, 12},
    { 7,  6,  5,  4, 0xFF},
    { 2,  1,  0, 0xFF, 3},
};

/* LED 编号 -> 物理 [row][col]（led_map 的转置）：方向类灯效必须按物理位置算，
 * 此前直接拿 LED 序号当坐标，序号顺序与行列不一致导致左右/上下全乱 */
static const __code uint8_t led_rc[LED_COUNT][2] = {
    {4, 2}, {4, 1}, {4, 0}, {4, 4},            /* LED0..3 */
    {3, 3}, {3, 2}, {3, 1}, {3, 0},            /* LED4..7 */
    {2, 3}, {2, 2}, {2, 1}, {2, 0}, {2, 4},    /* LED8..12 */
    {1, 3}, {1, 2}, {1, 1}, {1, 0},            /* LED13..16 */
    {0, 4}, {0, 3}, {0, 2}, {0, 1}, {0, 0},    /* LED17..21 */
};

static void hsv2rgb(uint8_t h, uint8_t s, uint8_t v, __idata uint8_t *out) {
    uint16_t hh = (uint16_t)h * 6;
    uint8_t sector = (uint8_t)(hh >> 8);
    uint8_t frac = (uint8_t)hh;
    uint8_t p = (uint8_t)(v * (255 - s) / 255);
    uint8_t q = (uint8_t)(v * (255 - (uint16_t)s * frac / 255) / 255);
    uint8_t t = (uint8_t)(v * (255 - (uint16_t)s * (255 - frac) / 255) / 255);
    switch (sector) {
        case 0: out[0]=v; out[1]=t; out[2]=p; break;
        case 1: out[0]=q; out[1]=v; out[2]=p; break;
        case 2: out[0]=p; out[1]=v; out[2]=t; break;
        case 3: out[0]=p; out[1]=q; out[2]=v; break;
        case 4: out[0]=t; out[1]=p; out[2]=v; break;
        default:out[0]=v; out[1]=p; out[2]=q; break;
    }
}

void rgb_init(void) {
    cfg.mode  = storage_read(DF_OFF_RGB + 0);
    cfg.hue   = storage_read(DF_OFF_RGB + 1);
    cfg.sat   = storage_read(DF_OFF_RGB + 2);
    cfg.val   = storage_read(DF_OFF_RGB + 3);
    cfg.speed = storage_read(DF_OFF_RGB + 4);
    /* 未初始化或旧版 6 档留下的越界 mode（>=7）：一律回到默认并重存 */
    if (cfg.mode > 6) {
        cfg.mode = RGB_MODE_DEFAULT; cfg.hue = RGB_HUE_DEFAULT;
        cfg.sat = RGB_SAT_DEFAULT; cfg.val = RGB_VAL_DEFAULT; cfg.speed = RGB_SPD_DEFAULT;
        rgb_save();
    }
    memset(leds, 0, sizeof(leds));
    pinMode(WS2812_DI_PIN, OUTPUT);
    digitalWrite(WS2812_DI_PIN, LOW);
}

void rgb_save(void) {
    storage_write(DF_OFF_RGB + 0, cfg.mode);
    storage_write(DF_OFF_RGB + 1, cfg.hue);
    storage_write(DF_OFF_RGB + 2, cfg.sat);
    storage_write(DF_OFF_RGB + 3, cfg.val);
    storage_write(DF_OFF_RGB + 4, cfg.speed);
}

void rgb_set_layer(uint8_t layer) { layer_hue = (uint8_t)(layer * 48); dirty = 1; }

void rgb_key_action(uint8_t act) {
    switch (act) {
        case 0: cfg.mode++; if (cfg.mode > 6) cfg.mode = 0; break; /* MODE_U：7 档 */
        case 1: cfg.hue += 8; break;
        case 2: cfg.hue -= 8; break;
        case 3: cfg.sat += 8; break;
        case 4: cfg.sat -= 8; break;
        case 5: cfg.val += 8; break;
        case 6: cfg.val -= 8; break;
        case 7: cfg.speed += 5; break;                    /* Spd+ */
        case 8: if (cfg.speed >= 5) cfg.speed -= 5; break;/* Spd-：曾缺失，u=20 被误当成 TO2 */
        default: return;
    }
    dirty = 1;
    rgb_save();
}

/* VIA custom-value 通道：0=模式 1=色相 2=饱和 3=亮度 4=速度 */
void rgb_set_value(uint8_t channel, uint8_t value) {
    switch (channel) {
        case 0: cfg.mode = value % 7; break;
        case 1: cfg.hue = value; break;
        case 2: cfg.sat = value; break;
        case 3: cfg.val = value; break;
        case 4: cfg.speed = value; break;
        default: return;
    }
    dirty = 1;
    rgb_save();
}

uint8_t rgb_get_value(uint8_t channel) {
    switch (channel) {
        case 0: return cfg.mode;
        case 1: return cfg.hue;
        case 2: return cfg.sat;
        case 3: return cfg.val;
        case 4: return cfg.speed;
        default: return 0;
    }
}

/* 诊断指示（仅 --diag 诊断档编译，避免污染正常档的基线规格）：
 * 0 = 熄灭并恢复灯效；1 = 蓝(b)；2 = 绿(g)；3 = 白(g+r+b)。
 * 单通道按位点亮，不查表，省 ROM。
 * 诊断态下 rgb_tick() 直接返回，灯不会被正常灯效覆盖，读到的颜色即状态。 */
#ifdef DIAG
void rgb_diag(uint8_t mode) {
    __idata uint8_t i;                   /* idata 局部：不占已满的直接寻址 OSEG */
    if (mode > 3) mode = 0;
    diag_mode = (mode != 0);
    for (i = 0; i < LED_COUNT; i++) {
        leds[i].g = (mode & 0x02) ? 32 : 0;
        leds[i].r = (mode & 0x04) ? 32 : 0;
        leds[i].b = (mode & 0x01) ? 32 : 0;
    }
    neopixel_show_P1_0((__xdata uint8_t *)leds, LED_COUNT * 3);
}
#endif

void rgb_tick(void) {
    uint8_t i, r, c, anim;
    __idata uint8_t rgb[3];            /* hsv2rgb 形参是 idata 指针 */
#ifdef DIAG
    if (diag_mode) return;               /* 诊断常亮时不被灯效覆盖 */
#endif
    /* 枚举完成前禁止刷新 WS2812：一帧要关全局中断 ~700µs，
     * 会打断 USB 枚举事务，导致主机完全不识别设备 */
    if (!usb_configured()) return;
    if (++tick_count % 12) return;       /* 每 12ms 更新一次 */
    /* 动画相位：speed(0..255) 映射到每 1..11 帧 +1，色相 256 帧一轮 ≈ 3..33s */
    anim = (uint8_t)(tick_count / (12 - (cfg.speed >> 5)));
    for (i = 0; i < LED_COUNT; i++) {
        uint8_t base;
        if (cfg.mode == 0) { leds[i].g = leds[i].r = leds[i].b = 0; continue; }
        r = led_rc[i][0]; c = led_rc[i][1];
        switch (cfg.mode) {
            case 1: base = cfg.hue; break;                          /* 单色 */
            case 2: base = (uint8_t)(cfg.hue + anim); break;         /* 全局循环：整板同色流转 */
            case 3: base = (uint8_t)(cfg.hue + c * 48 + anim); break; /* 左右循环：同列同色、逐列相位横向流动。原 (r&1)*128 奇偶行反相会让横向色带被看成上下移动，已移除 */
            case 4: base = (uint8_t)(cfg.hue + r * 48 + (c & 1) * 128 + anim); break; /* 上下循环：同行同色，逐行相位，奇偶列反向 */
            case 5: base = (uint8_t)(cfg.hue + anim + (uint8_t)(r * 10 + c * 14)); break; /* 彩虹流动：按物理坐标斜向相位 */
            default: base = (uint8_t)(cfg.hue + anim + (c > 2 ? 4 - c : c) * 40); break;  /* 色相波浪：中间列领先向两侧延迟 */
        }
        hsv2rgb(base, cfg.sat, cfg.val, rgb);
        leds[i].g = rgb[1]; leds[i].r = rgb[0]; leds[i].b = rgb[2];  /* hsv 出 R,G,B；WS2812 上线 GRB，曾把 R 写进 G 槽→红显绿 */
    }
    if (rgb_fn_flag) { leds[18].g = leds[18].r = leds[18].b = 60; }  /* FN 按住：仅 LED19（0-based 18，用户从 1 数）白色 */
    neopixel_show_P1_0((__xdata uint8_t *)leds, LED_COUNT * 3);
    dirty = 0;
}
