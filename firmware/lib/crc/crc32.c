#include <stdint.h>
#include <stddef.h>

//CRC32 config
//The crc32_nibble_table used attribute and assembly instruction
//is required to get absolute address to the table.
//Risc-V have something called global pointer.

//lookup take more flash but is is around 4x faster.
//Takes around ~72Byte more
#define USE_CRC_LOOKUP_TABLE 


#ifdef USE_CRC_LOOKUP_TABLE

__attribute__((__used__))
static const uint32_t crc32_nibble_table[16] = {
    0x00000000, 0x1DB71064, 0x3B6E20C8, 0x26D930AC,
    0x76DC4190, 0x6B6B51F4, 0x4DB26158, 0x5005713C,
    0xEDB88320, 0xF00F9344, 0xD6D6A3E8, 0xCB61B38C,
    0x9B64C2B0, 0x86D3D2D4, 0xA00AE278, 0xBDBDF21C
};


void crc32_update(uint32_t *state, const uint8_t *data, size_t len) {
    uint32_t crc = *state;
    const uint32_t *table_ptr;
    
    // Uses the Program Counter (PC) to find the table.
    // Done to avoid using GP and Linker Relaxation.
    __asm__ (
        ".option push\n\t"                                      // Store current gcc settings
        ".option norelax\n\t"                                   // Prevent GP optimization    
        "1: auipc %0, %%pcrel_hi(crc32_nibble_table)\n\t"       // Load address
        "addi %0, %0, %%pcrel_lo(1b)\n\t"                       // ...
        ".option pop"                                           //Restore
        : "=r"(table_ptr));

    while (len--) {
        uint8_t byte = *data++;

        // Process low nibble
        crc = (crc >> 4) ^ table_ptr[(crc ^ (byte >> 0)) & 0x0F];
        
        // Process high nibble
        crc = (crc >> 4) ^ table_ptr[(crc ^ (byte >> 4)) & 0x0F];
    }
    *state = crc;
}

#else

/**
 * Update CRC32
 */
void crc32_update(uint32_t *state, const uint8_t *data, size_t len) {
    uint32_t crc = *state;

    while (len--) {
        crc ^= *data++;
        for (uint8_t i = 0; i < 8; i++) {
            if (crc & 1){
                 crc = (crc >> 1) ^ 0xEDB88320;
            }else{ 
                crc >>= 1;
            }
        }
    }
    *state = crc;
}


#endif

/**
 * Initialize the CRC value.
 * Ethernet standard starts with all 1s (0xFFFFFFFF).
 */
void crc32_init(uint32_t *crc_state) {
    *crc_state = 0xFFFFFFFFuL;
}



/** 
 * @calculate a CRC32 value from data.
 */
uint32_t crc32_calc(const uint8_t *data, size_t len){

    uint32_t state = 0xFFFFFFFFuL;
    crc32_update(&state, data, len);

    return state ^ 0xFFFFFFFF;
}


/**
 * Finalize the CRC value.
 * Ethernet standard requires XORing the result with 0xFFFFFFFF.
 */
uint32_t crc32_finalize(uint32_t *crc_state) {
    return *crc_state ^ 0xFFFFFFFF;
}

