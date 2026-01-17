#include <unity.h>
#include "crc32.h"

void setUp(void) {}
void tearDown(void) {}



/**
 * Tests the convenience wrapper function crc32_calc
 */
void test_crc32_calc_wrapper(void) {
    const uint8_t data[] = "123456789";
    // crc32_calc should internally handle init, update, and finalize
    uint32_t result = crc32_calc(data, 9);
    
    TEST_ASSERT_EQUAL_HEX32(0xCBF43926, result);
}


/**
 * Comprehensive tests for the crc32_calc wrapper
 */
void test_crc32_calc_variations(void) {
    // Test 1: Single byte
    const uint8_t single_byte = 'A';
    // Expected CRC32 for "A": 0xD3D99E8B
    TEST_ASSERT_EQUAL_HEX32(0xD3D99E8B, crc32_calc(&single_byte, 1));

    // Test 2: Multi-byte "Hello World"
    const uint8_t hello_world[] = "Hello World";
    // Expected CRC32: 0x4A17B156
    TEST_ASSERT_EQUAL_HEX32(0x4A17B156, crc32_calc(hello_world, 11));
}

/**
 * Tests crc32_calc with null or zero-length input
 */
void test_crc32_calc_empty(void) {
    // Zero length should result in 0x00000000 due to (0xFFFFFFFF ^ 0xFFFFFFFF)
    TEST_ASSERT_EQUAL_HEX32(0x00000000, crc32_calc(NULL, 0));
    
    uint8_t dummy = 0xFF;
    TEST_ASSERT_EQUAL_HEX32(0x00000000, crc32_calc(&dummy, 0));
}


void test_crc32_basic(void) {
    // Standard test vector: "123456789"
    // Expected Ethernet CRC32: 0xCBF43926
    const uint8_t data[] = "123456789";
    uint32_t crc_state;

    crc32_init(&crc_state);
    crc32_update(&crc_state, data, sizeof(data) - 1);
    uint32_t result = crc32_finalize(&crc_state);

    TEST_ASSERT_EQUAL_UINT32(0xCBF43926, result);
}

void test_crc32_leading_zeros(void) {
    // Verify sensitivity to leading zeros
    const uint8_t data1[] = {0x01, 0x02};
    const uint8_t data2[] = {0x00, 0x01, 0x02}; // One extra leading zero
    
    uint32_t state1, state2;

    crc32_init(&state1);
    crc32_update(&state1, data1, sizeof(data1));
    uint32_t res1 = crc32_finalize(&state1);

    crc32_init(&state2);
    crc32_update(&state2, data2, sizeof(data2));
    uint32_t res2 = crc32_finalize(&state2);

    // If init was 0x00, these might match. With 0xFFFFFFFF, they must differ.
    TEST_ASSERT_NOT_EQUAL(res1, res2);
}

/**
 * Standard industry test vectors for Ethernet CRC32 (Poly 0xEDB88320)
 */
void test_crc32_standard_vectors(void) {
    uint32_t state;

    // Test 1: "123456789" (The most common CRC32 check)
    const uint8_t data_ascii[] = "123456789";
    crc32_init(&state);
    crc32_update(&state, data_ascii, 9);
    TEST_ASSERT_EQUAL_HEX32(0xCBF43926, crc32_finalize(&state));

    // Test 2: Empty string (Length 0)
    // The result of an empty string should be 0 (0xFFFFFFFF ^ 0xFFFFFFFF)
    crc32_init(&state);
    crc32_update(&state, NULL, 0);
    TEST_ASSERT_EQUAL_HEX32(0x00000000, crc32_finalize(&state));

    // Test 3: Four Zeros (0x00 0x00 0x00 0x00)
    const uint8_t four_zeros[] = {0, 0, 0, 0};
    crc32_init(&state);
    crc32_update(&state, four_zeros, 4);
    TEST_ASSERT_EQUAL_HEX32(0x2144DF1C, crc32_finalize(&state));

    // Test 4: Four Ones (0xFF 0xFF 0xFF 0xFF)
    const uint8_t four_ones[] = {0xFF, 0xFF, 0xFF, 0xFF};
    crc32_init(&state);
    crc32_update(&state, four_ones, 4);
    TEST_ASSERT_EQUAL_HEX32(0xFFFFFFFF, crc32_finalize(&state));
}

/**
 * Specifically tests the "Nibble" implementation logic 
 * by checking edge-case byte values.
 */
void test_crc32_edge_bytes(void) {
    uint32_t state;
    const uint8_t edge_data[] = {0x00, 0x0F, 0xF0, 0xFF, 0x55, 0xAA};

    crc32_init(&state);
    crc32_update(&state, edge_data, sizeof(edge_data));
    uint32_t result = crc32_finalize(&state);

    TEST_ASSERT_EQUAL_HEX32(0xFAA3F7DF, result);
}

void test_crc32_streaming(void) {
    // Verify that updating in chunks yields the same result as one block
    const uint8_t data[] = {0xAA, 0xBB, 0xCC, 0xDD};
    uint32_t state_full, state_stream;

    // Single pass
    crc32_init(&state_full);
    crc32_update(&state_full, data, 4);
    uint32_t res_full = crc32_finalize(&state_full);

    // Multi pass (streaming)
    crc32_init(&state_stream);
    crc32_update(&state_stream, &data[0], 2);
    crc32_update(&state_stream, &data[2], 2);
    uint32_t res_stream = crc32_finalize(&state_stream);

    TEST_ASSERT_EQUAL_UINT32(res_full, res_stream);
}

/**
 * Verify that the pointer-based streaming works correctly 
 * across a mock "firmware page" (64 bytes)
 */
void test_crc32_firmware_page_sim(void) {
    uint8_t mock_page[64];
    for(int i = 0; i < 64; i++) mock_page[i] = (uint8_t)i;

    uint32_t state;
    crc32_init(&state);
    
    // Simulate receiving data in tiny 8-byte chunks (SRAM friendly)
    for(int i = 0; i < 64; i += 8) {
        crc32_update(&state, &mock_page[i], 8);
    }

    TEST_ASSERT_EQUAL_HEX32(0x100ECE8C, crc32_finalize(&state));
}


int main(void) {
    
    
    UNITY_BEGIN();

    //crc32_calc tests
    RUN_TEST(test_crc32_calc_wrapper);
    RUN_TEST(test_crc32_calc_variations);
    RUN_TEST(test_crc32_calc_empty);

    //crc32_update tests
    RUN_TEST(test_crc32_basic);
    RUN_TEST(test_crc32_standard_vectors);
    RUN_TEST(test_crc32_edge_bytes);
    RUN_TEST(test_crc32_leading_zeros);
    RUN_TEST(test_crc32_streaming);
    RUN_TEST(test_crc32_firmware_page_sim);
    return UNITY_END();
}
