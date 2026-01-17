#ifndef CRC32_H
#define CRC32_H

#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Initialize the Ethernet CRC32
 */
void crc32_init(uint32_t *crc_state);


/**
 * @brief Calculate CRC32 of a block of data
 */
uint32_t crc32_calc(const uint8_t *data, size_t len);

/**
 * @brief Update the Ethernet CRC32 with data.
 */
void crc32_update(uint32_t *state, const uint8_t *data, size_t len) ;

/**
 * @brief Finalize the Ethernet CRC32.
 * @return The final Ethernet-compliant CRC32 value.
 */
uint32_t crc32_finalize(uint32_t *crc_state);

#ifdef __cplusplus
}
#endif

#endif // CRC32_H
