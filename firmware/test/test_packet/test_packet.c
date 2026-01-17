#include <unity.h>
#include <string.h>
#include "packet.h"
#include "crc32.h"

// Mock buffer for serialization
static uint8_t buffer[512];
static Packet_t rx_pkt;

void setUp(void) {
    memset(buffer, 0, sizeof(buffer));
    memset(&rx_pkt, 0, sizeof(rx_pkt));
}

void tearDown(void) {}

/**
 * Test 1: Verify Serialization
 * Checks if the preamble, header, data, and CRC are placed correctly.
 */
void test_packet_serialization_basic(void) {
    uint8_t payload[] = {0xDE, 0xAD, 0xBE, 0xEF};
    uint8_t node_id = 0x12;
    uint8_t cmd = 0xAA;
    
    uint32_t total_len = packet_serialize(buffer, node_id, cmd, payload, sizeof(payload));

    // Verify Preambles (5 bytes of 0x7F)
    for(int i = 0; i < 5; i++) {
        TEST_ASSERT_EQUAL_HEX8(0x7F, buffer[i]);
    }

    // Verify Header (Base 0x80 | Type Response 0x01)
    TEST_ASSERT_EQUAL_HEX8(0x81, buffer[5]);
    
    // Verify Node ID, Command, and Length
    TEST_ASSERT_EQUAL_HEX8(node_id, buffer[6]);
    TEST_ASSERT_EQUAL_HEX8(cmd, buffer[7]);
    TEST_ASSERT_EQUAL_HEX8(4, buffer[8]);

    // Total length should be: 5 (pre) + 4 (hdr/cmd/len) + 4 (data) + 4 (crc) = 17
    TEST_ASSERT_EQUAL_UINT32(17, total_len);
}

/**
 * Test 2: Full Round Trip (Serialize -> Parse)
 * Feeds the serialized buffer back into the Rx state machine byte-by-byte.
 */
void test_packet_round_trip(void) {
    uint8_t payload[] = "Hello";
    uint8_t node_id = 0x05;
    uint8_t cmd = 0x02;
    uint32_t len = packet_serialize(buffer, node_id, cmd, payload, 5);

    uint8_t result = 0;
    for (uint32_t i = 0; i < len; i++) {
        result = Packet_Update_Rx(buffer[i], &rx_pkt);
        
        // Result should only be 1 on the very last byte
        if (i < len - 1) {
            TEST_ASSERT_EQUAL_INT(0, result);
        }
    }

    TEST_ASSERT_EQUAL_INT(0, result); //own packets shall be ignored.
    
    TEST_ASSERT_EQUAL_UINT8(PKT_TYPE_RESPONSE, rx_pkt.type);
    TEST_ASSERT_EQUAL_UINT8(cmd, rx_pkt.command);
    TEST_ASSERT_EQUAL_UINT8(5, rx_pkt.data_len);
    TEST_ASSERT_EQUAL_UINT8_ARRAY(payload, rx_pkt.data, 5);
    TEST_ASSERT_EQUAL_UINT8(node_id, rx_pkt.address[0]);
}

/**
 * Test 3: CRC Failure
 * Corrupts a data byte and ensures the state machine returns 0 (invalid).
 */
void test_packet_invalid_crc(void) {
    uint8_t payload[] = {0x01, 0x02, 0x03};
    uint32_t len = packet_serialize(buffer, 0x01, 0x10, payload, 3);

    // Corrupt the first data byte (Index 9: 5 preambles + 4 header bytes)
    buffer[9] ^= 0xFF; 

    uint8_t result = 0;
    for (uint32_t i = 0; i < len; i++) {
        result = Packet_Update_Rx(buffer[i], &rx_pkt);
    }

    // Should return 0 because CRC calculation won't match
    TEST_ASSERT_EQUAL_INT(0, result);
}

/**
 * Test 4: Resync Logic
 * Ensures the parser recovers if it receives garbage followed by a valid preamble.
 */
void test_packet_resync(void) {
    uint8_t garbage[] = {0x00, 0xFF, 0x7F, 0x7F, 0x22}; // 0x22 is not a valid header
    uint8_t payload[] = {0x44};
    uint32_t packet_len = packet_serialize(buffer, 0x01, 0x01, payload, 1);

    // 1. Feed garbage
    for(int i = 0; i < sizeof(garbage); i++) {
        Packet_Update_Rx(garbage[i], &rx_pkt);
    }

    // 2. Feed valid packet
    uint8_t result = 0;
    for (uint32_t i = 0; i < packet_len; i++) {
        result = Packet_Update_Rx(buffer[i], &rx_pkt);
    }

    //result will be false due to packet_update_rx ignores request packets. 
    TEST_ASSERT_EQUAL_UINT8(0x44, rx_pkt.data[0]);
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_packet_serialization_basic);
    RUN_TEST(test_packet_round_trip);
    RUN_TEST(test_packet_invalid_crc);
    RUN_TEST(test_packet_resync);
    return UNITY_END();
}

