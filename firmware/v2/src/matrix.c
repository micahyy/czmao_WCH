#include <stdint.h>
#include <stdbool.h>
#include "config.h"
#include <Arduino.h>

/* COL2ROW：逐行拉低读 5 列；主循环每 DEBOUNCE_MS 调一次 scan，天然消抖。
 * 3 个物理空位在读取时由 IS_EMPTY_CELL 屏蔽。 */
static const __code uint8_t row_pins[MATRIX_ROWS] = MATRIX_ROW_PINS;
static const __code uint8_t col_pins[MATRIX_COLS] = MATRIX_COL_PINS;

__xdata uint8_t matrix_state[MATRIX_ROWS];   /* bit(c)=1 表示按下 */

void matrix_init(void) {
    uint8_t i;
    for (i = 0; i < MATRIX_ROWS; i++) {
        pinMode(row_pins[i], OUTPUT);
        digitalWrite(row_pins[i], HIGH);     /* 未选中行保持高，避免鬼影 */
        matrix_state[i] = 0;
    }
    for (i = 0; i < MATRIX_COLS; i++) {
        pinMode(col_pins[i], INPUT_PULLUP);
    }
}

void matrix_scan(void) {
    uint8_t r, c;
    for (r = 0; r < MATRIX_ROWS; r++) {
        uint8_t bits = 0;
        digitalWrite(row_pins[r], LOW);
        delayMicroseconds(MATRIX_IO_DELAY);
        for (c = 0; c < MATRIX_COLS; c++) {
            if (!digitalRead(col_pins[c])) bits |= (1 << c);
        }
        digitalWrite(row_pins[r], HIGH);
        matrix_state[r] = bits;
    }
}

uint8_t matrix_is_pressed(uint8_t r, uint8_t c) {
    if (IS_EMPTY_CELL(r, c)) return 0;
    return (matrix_state[r] & (1 << c)) ? 1 : 0;
}
