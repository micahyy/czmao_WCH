#pragma once
#include <stdint.h>

void storage_write(uint8_t addr, uint8_t value);
uint8_t storage_read(uint8_t addr);
void storage_write_block(uint8_t addr, const __idata uint8_t *buf, uint8_t len);
