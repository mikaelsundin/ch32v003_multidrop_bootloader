#ifndef BSL_H
#define BSL_H

#include <cstdint>
#include <cstddef>

/**
 * These function lives in BSL code. This saves ~0.5kbyte from APP.
 * Jump table starts at address 0x1FFFF00C
 */

// void flash_write(uint32_t adr, uint8_t data[64]);
typedef void (*FlashWriteFn)(uint32_t, uint8_t[64]);
const auto flash_write = reinterpret_cast<FlashWriteFn>(0x1FFFF00C);

// void flash_erase(uint32_t adr);
typedef void (*FlashEraseFn)(uint32_t);
const auto flash_erase = reinterpret_cast<FlashEraseFn>(0x1FFFF010);

// void flash_write_option_data(uint8_t data0, uint8_t data1);
typedef void (*FlashWriteOptionFn)(uint8_t, uint8_t);
const auto flash_write_option_data = reinterpret_cast<FlashWriteOptionFn>(0x1FFFF014);

// uint32_t crc32_calc(const uint8_t *data, size_t len);
typedef uint32_t (*Crc32CalcFn)(const uint8_t*, size_t);
const auto crc32_calc = reinterpret_cast<Crc32CalcFn>(0x1FFFF018);

#endif // BSL_H


