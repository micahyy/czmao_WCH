#include <stdint.h>
#include "config.h"

/* CH552 DataFlash：128B，直接复用 ch55xduino core 的 eeprom.c 实现 */
extern void eeprom_write_byte(uint8_t addr, uint8_t value);
extern uint8_t eeprom_read_byte(uint8_t addr);

void storage_write(uint8_t addr, uint8_t value) {
    if (addr < DF_SIZE) eeprom_write_byte(addr, value);
}

uint8_t storage_read(uint8_t addr) {
    return (addr < DF_SIZE) ? eeprom_read_byte(addr) : 0;
}

void storage_write_block(uint8_t addr, const __idata uint8_t *buf, uint8_t len) {
    uint8_t i;
    for (i = 0; i < len && (uint8_t)(addr + i) < DF_SIZE; i++) {
        eeprom_write_byte(addr + i, buf[i]);
    }
}
