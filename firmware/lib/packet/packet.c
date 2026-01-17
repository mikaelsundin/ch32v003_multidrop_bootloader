#include "packet.h"
#include "crc32.h"


typedef enum { 
    STATE_IDLE, STATE_HDR, STATE_ADDR, STATE_CMD, STATE_LEN, STATE_DATA, STATE_CRC 
} RxState_t;


#define HDR_MASK_BASE       0x80 // Top 6 bits are 100000
#define HDR_FLAG_ADR_128BIT 0x02 // Bit 1
#define HDR_MASK_TYPE       0x01 // Bit 0


/**
 * @brief Serialize a packet into a buffer.
 */
uint32_t packet_serialize(uint8_t* buffer,
    uint8_t node_id, 
    uint8_t cmd, uint8_t* data, uint8_t datalen)
{
    uint32_t i = 0;
    
    // 1. Fill Preambles
    for(int p = 0; p < PREAMBLE_COUNT; p++) {
        buffer[i++] = PREAMBLE_BYTE;
    }

    // 2. Build Packet Body
    buffer[i++] = HDR_MASK_BASE | PKT_TYPE_RESPONSE; 
    buffer[i++] = node_id;
    buffer[i++] = cmd;
    buffer[i++] = datalen;

    // 3. Copy Data
    for(uint8_t d = 0; d < datalen; d++) {
        buffer[i++] = data[d];
    }

    // 4. Single CRC Update using your suggested offset logic
    // We pass the start of the header and calculate length relative to preamble
    uint32_t final_crc = crc32_calc(&buffer[PREAMBLE_COUNT], i - PREAMBLE_COUNT);

    // 5. Append CRC (Little Endian)
    buffer[i++] = (uint8_t)(final_crc);
    buffer[i++] = (uint8_t)(final_crc >> 8);
    buffer[i++] = (uint8_t)(final_crc >> 16);
    buffer[i++] = (uint8_t)(final_crc >> 24);

    return i;
}

volatile uint32_t total_sync_count=0;

uint32_t get_packet_total_sync_count(void){
    return total_sync_count;
}

uint8_t Packet_Update_Rx(uint8_t byte, Packet_t *pkt) {
    static RxState_t state = STATE_IDLE;
    static uint8_t sync_count = 0;
    static uint8_t index ;
    static uint8_t crc_buf[4];
    static uint32_t crc_state;

    // --- Resync Logic ---
    // Always active to detect resync.
    if (byte == PREAMBLE_BYTE) {
        sync_count++;
        total_sync_count++;
    } else {
        if (sync_count >= PREAMBLE_COUNT){
            //Check if we got valid HDR.
            if((byte & 0xFC) == HDR_MASK_BASE) {
                state = STATE_HDR;
            }
        }
        
        //Reset sync counter.
        sync_count = 0;
    }

    //Skip rest code if we are in idle state.
    if(state == STATE_IDLE)
    {
        return 0;
    }

    
    //state machine.
    if(state == STATE_HDR){
        crc32_init(&crc_state);

        // Decode attributes using bit 0-1 for type
        pkt->type = (PacketType_t)(byte & HDR_MASK_TYPE);
        pkt->addr_len = (byte & HDR_FLAG_ADR_128BIT) ? 8 : 1;

        index = 0;
        state = STATE_ADDR;
    }else if(state == STATE_ADDR){
        pkt->address[index++] = byte;
        
        if (index == pkt->addr_len){
            state = STATE_CMD;
        } 
    }else if(state == STATE_CMD){
        pkt->command = byte;
        state = STATE_LEN;
    }else if(state == STATE_LEN){
        pkt->data_len = byte;
        index = 0;
        state = (pkt->data_len > 0) ? STATE_DATA : STATE_CRC;
    }else if(state == STATE_DATA){
        pkt->data[index++] = byte;
        if (index == pkt->data_len){
            index=0;
            state = STATE_CRC;
        } 
    }else if(state == STATE_CRC){
        crc_buf[index++] = byte;

        if (index == 4) 
        {
            uint32_t crc_rx = *(uint32_t*)&crc_buf[0];  

            //get the calculated CRC32. 
            uint32_t crc_calc = crc32_finalize(&crc_state);
            
            //Restore state machine.
            state = STATE_IDLE;
            sync_count = 0;

            //only process packages that are request type.
            if(pkt->type == PKT_TYPE_REQUEST){
                //Check for CRC32 match
                return (crc_rx == crc_calc);
            }else{
                return 0;
            }

        }

        //do not include in CRC32.
        return 0;
    }

    //Update the CRC
    crc32_update(&crc_state, &byte, 1);
    return 0;
}

