#ifndef PACKET_PROTOCOL_H
#define PACKET_PROTOCOL_H

#include <stdint.h>
#include <stddef.h>

#define PREAMBLE_BYTE  0x7F
#define PREAMBLE_COUNT 5

typedef enum {
    PKT_TYPE_REQUEST  = 0x00,
    PKT_TYPE_RESPONSE = 0x01
} PacketType_t;

typedef struct {
    PacketType_t type;
    uint8_t command;

    uint8_t address[8] __attribute__((aligned(4)));;
    uint8_t addr_len;
    
    uint8_t data_len;
    uint8_t data[255] __attribute__((aligned(4)));;
} Packet_t;

/**
 * @brief Serializes a RESPONSE packet .
 * @note Only 8bit addressing supported.
 * @param buffer   Output buffer where the serialized stream will be stored.
 * @param node_id  The 8-bit destination address/node identifier.
 * @param cmd      The command byte to be executed by the receiver.
 * @param data     Pointer to the payload data buffer (can be NULL if datalen is 0).
 * @param datalen  Length of the payload (0-255).
 * @return uint32_t Total number of bytes written to the buffer (including preambles, header, and CRC).
 */
uint32_t packet_serialize(uint8_t* buffer,
    uint8_t node_id, 
    uint8_t cmd, uint8_t* data, uint8_t datalen);

/**
 * @brief Handles a single incoming byte (supports both Request and Response).
 * @return 1 if a full valid packet was completed (CRC matches), 0 otherwise.
 */
uint8_t Packet_Update_Rx(uint8_t byte, Packet_t *pkt);

/**
 * @brief Get total number of sync chars found
 */
uint32_t get_packet_total_sync_count(void);

#endif
