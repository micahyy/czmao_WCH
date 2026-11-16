#include <stdint.h>
#include <string.h>
#include "config.h"
#include "board.h"
#include "keymap.h"
#include "storage.h"

/* ============================================================================
 * 4 层键表：XRAM 展开缓存 200B + DataFlash 变长压缩。
 * VIA get/set_buffer 用 km_ram 的线性偏移，布局与未压缩时一致。
 *
 * 编码（16bit 键码无损）：
 *   0x00..0xE7 -> 1B；0xE8 -> 0x01xx；0xE9 -> 0x7Exx（本板自定义）
 *   0xEA -> 0x52xx（层切换）；0xEB -> 0x78xx（RGB）；0xEC -> 0x7Cxx
 *   0xED -> 0x40xx；0xEE -> 0x20xx；0xEF -> 0x70xx；0xFF -> 3B 原样
 * 3 个物理空位不存。
 * ==========================================================================*/
#define KM_MARK             0xA5
#define KM_RAM_BYTES        (LAYER_COUNT * MATRIX_ROWS * MATRIX_COLS * 2)   /* 200 */

static __xdata uint8_t km_ram[KM_RAM_BYTES];
static __xdata uint8_t km_blob[DF_KEYMAP_CAP];   /* 121B 编码暂存 */

static const __code uint16_t default_keymaps[LAYER_COUNT][MATRIX_ROWS][MATRIX_COLS] = KEY_MAPS;

static uint8_t km_empty(uint8_t r, uint8_t c) { return IS_EMPTY_CELL(r, c); }
static uint16_t km_index(uint8_t l, uint8_t r, uint8_t c) {
    return ((uint16_t)l * MATRIX_ROWS + r) * MATRIX_COLS + c;
}

/* 把 board.h 出厂默认表铺进 km_ram：keymap_reset_default 与差量解码共用一份
 * （否则 large-model 下两处各生成 ~200B 展开，诊断档会撑爆 ROM）*/
static void km_fill_defaults(void) {
    uint8_t l, r, c;
    for (l = 0; l < LAYER_COUNT; l++) {
        for (r = 0; r < MATRIX_ROWS; r++) {
            for (c = 0; c < MATRIX_COLS; c++) {
                uint16_t idx = km_index(l, r, c) * 2;
                uint16_t kc = default_keymaps[l][r][c];
                km_ram[idx] = (uint8_t)(kc >> 8);
                km_ram[idx + 1] = (uint8_t)kc;
            }
        }
    }
}

static uint8_t km_encode(uint16_t kc, __xdata uint8_t *out) {
    uint8_t hi = (uint8_t)(kc >> 8), lo = (uint8_t)kc;
    if (kc <= 0xE7) { out[0] = lo; return 1; }
    out[1] = lo;
    switch (hi) {
        case 0x01: out[0] = 0xE8; return 2;
        case 0x7E: out[0] = 0xE9; return 2;
        case 0x52: out[0] = 0xEA; return 2;
        case 0x78: out[0] = 0xEB; return 2;
        case 0x7C: out[0] = 0xEC; return 2;
        case 0x40: out[0] = 0xED; return 2;
        case 0x20: out[0] = 0xEE; return 2;
        case 0x70: out[0] = 0xEF; return 2;
        default: out[0] = 0xFF; out[2] = hi; return 3;
    }
}

static uint16_t km_decode(__xdata uint8_t *in, uint8_t *used) {
    uint8_t t = in[0];
    if (t <= 0xE7) { *used = 1; return t; }
    if (t == 0xFF) { *used = 3; return ((uint16_t)in[2] << 8) | in[1]; }
    if (t >= 0xE8 && t <= 0xEF) {
        *used = 2;
        switch (t) {
            case 0xE8: return 0x0100 | in[1];
            case 0xE9: return 0x7E00 | in[1];
            case 0xEA: return 0x5200 | in[1];
            case 0xEB: return 0x7800 | in[1];
            case 0xEC: return 0x7C00 | in[1];
            case 0xED: return 0x4000 | in[1];
            case 0xEE: return 0x2000 | in[1];
            case 0xEF: return 0x7000 | in[1];
        }
    }
    *used = 1;
    return 0;
}

/* 差量编码：只存与 board.h 出厂默认不同的键（48B 区放不下整表，见 config.h 分区）。
 *   [0] KM_MARK，随后每层 [count] 后接 count 条 ( [pos] [enc(kc)..] )，pos=r*COLS+c
 * 出厂状态 4 个 count 全 0 => 仅 5B，把 DataFlash 大头让给宏持久化。
 * 返回 0xFF = 改动过多超出 DF_KEYMAP_CAP（调用方据此放弃本次持久化，绝不越界写宏区）*/
static uint8_t km_encode_all(__xdata uint8_t *blob) {
    uint8_t out = 0, l, r, c, cnt, cpos;
    blob[out++] = KM_MARK;
    for (l = 0; l < LAYER_COUNT; l++) {
        cpos = out; out++; cnt = 0;
        for (r = 0; r < MATRIX_ROWS; r++) {
            for (c = 0; c < MATRIX_COLS; c++) {
                uint16_t idx, kc;
                if (km_empty(r, c)) continue;
                idx = km_index(l, r, c) * 2;
                kc = ((uint16_t)km_ram[idx] << 8) | km_ram[idx + 1];
                if (kc == default_keymaps[l][r][c]) continue;
                if (out + 4 > DF_KEYMAP_CAP) return 0xFF;   /* pos(1)+最长编码(3) */
                blob[out++] = (uint8_t)(r * MATRIX_COLS + c);
                out += km_encode(kc, &blob[out]);
                cnt++;
            }
        }
        blob[cpos] = cnt;
    }
    return out;
}

/* 差量解码：先用出厂默认铺满 km_ram，再逐条覆盖变化格。0=成功，非 0=无效(回退默认) */
static uint8_t km_decode_all(const __xdata uint8_t *blob) {
    uint8_t in, l, r, c, k, cnt, pos, used;
    uint16_t idx, kc;
    if (blob[0] != KM_MARK) return 1;
    km_fill_defaults();
    in = 1;
    for (l = 0; l < LAYER_COUNT; l++) {
        if (in >= DF_KEYMAP_CAP) return 1;
        cnt = blob[in++];
        for (k = 0; k < cnt; k++) {
            if (in >= DF_KEYMAP_CAP) return 1;
            pos = blob[in++];
            if (pos >= MATRIX_ROWS * MATRIX_COLS) return 1;
            if (in >= DF_KEYMAP_CAP) return 1;
            kc = km_decode((__xdata uint8_t *)&blob[in], &used);
            in += used;
            if (in > DF_KEYMAP_CAP) return 1;
            r = pos / MATRIX_COLS; c = pos % MATRIX_COLS;
            if (km_empty(r, c)) continue;                  /* 变化落在物理空位：保持默认 */
            idx = km_index(l, r, c) * 2;
            km_ram[idx] = (uint8_t)(kc >> 8);
            km_ram[idx + 1] = (uint8_t)kc;
        }
    }
    return 0;
}

void keymap_flush(void) {
    uint8_t len = km_encode_all(km_blob), i;
    if (len == 0xFF) { storage_write(DF_OFF_KEYMAP, 0x00); return; }  /* 放不下：标记失效，下次上电回退默认，宏区不被越界破坏 */
    for (i = 0; i < len; i++) storage_write(DF_OFF_KEYMAP + i, km_blob[i]);
}

void keymap_reset_default(void) {
    km_fill_defaults();
    keymap_flush();
}

void keymap_init(void) {
    uint8_t i;
    for (i = 0; i < DF_KEYMAP_CAP; i++) km_blob[i] = storage_read(DF_OFF_KEYMAP + i);
    if (km_decode_all(km_blob) != 0) keymap_reset_default();
}

uint16_t keymap_get_keycode(uint8_t layer, uint8_t row, uint8_t col) {
    uint16_t idx;
    if (layer >= LAYER_COUNT) return 0;
    idx = km_index(layer, row, col) * 2;
    return ((uint16_t)km_ram[idx] << 8) | km_ram[idx + 1];
}

void keymap_set_keycode(uint8_t layer, uint8_t row, uint8_t col, uint16_t kc) {
    uint16_t idx;
    if (layer >= LAYER_COUNT) return;
    idx = km_index(layer, row, col) * 2;
    km_ram[idx] = (uint8_t)(kc >> 8);
    km_ram[idx + 1] = (uint8_t)kc;
    keymap_flush();
}

uint8_t keymap_buffer_byte(uint16_t offset) {
    return (offset < KM_RAM_BYTES) ? km_ram[offset] : 0;
}

void keymap_set_buffer_byte(uint16_t offset, uint8_t v) {
    if (offset < KM_RAM_BYTES) km_ram[offset] = v;
}


