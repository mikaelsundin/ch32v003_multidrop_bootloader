#include <ch32v00x.h>
#include <stdint.h>
#include <stddef.h>
#include "flash.h"
#include "crc32.h"
#include "packet.h"
#include "cmd.h"
#include "uart.h"

//-----------------------------------------------------------------
//Bootloader info
#define BOOTLOADER_MAJOR    01
#define BOOTLOADER_MINOR    01

const uint8_t chip_name[] = {
    0x43, 0x48, 0x33, 0x32, 
    0x56, 0x30, 0x30, 0x33, 
    0x4A, 0x34, 0x4D, 0x36, 
};
//-----------------------------------------------------------------

uint8_t get_node_id(void);
uint8_t get_firmware_id(void);
uint32_t memcmp64(const uint8_t *id1, const uint8_t *id2);
void GetChipID64(uint8_t *dest);
void bootloader_start_app(void);
void process_packet(Packet_t* rx);
void initialize(void);
void deinitilize(void);

Packet_t packet;
uint8_t tx_buffer[128];
uint8_t tx_data[64];
uint8_t stay_silent=0;
uint32_t boot_timeout = 0;

/**
 * @brief Fast variant to compare 64bit values.
 */
uint32_t memcmp64(const uint8_t *id1, const uint8_t *id2) {
    // use 32bit to take sligtly less flash.
    const uint32_t *p1 = (const uint32_t *)id1;
    const uint32_t *p2 = (const uint32_t *)id2;

    // Compare 2 words (64 bits total)
    if (p1[0] != p2[0]) return 0;
    if (p1[1] != p2[1]) return 0;

    return 1;
}

/**
 * @brief Get 64bit ChipId
 */
void GetChipID64(uint8_t *dest) {
    uint32_t* d = (uint32_t*)dest;

    //Only 64bit is unique on CH32V003.
    //Datasheet states 96bits, but only 64bit is used.
    d[0] = *(volatile uint32_t*)(0x1FFFF7E8);
    d[1] = *(volatile uint32_t*)(0x1FFFF7EC);
}

/**
 * @brief Get node id.
 */
uint8_t get_node_id(void){
    return *(uint8_t*)0x1FFFF804;
}

/**
 * @brief Get Firmware id.
 */
uint8_t get_firmware_id(void){
    return *(uint8_t*)0x1FFFF806;
}



/**
 * @brief Start the applicaton
 */
void bootloader_start_app(void){
    RCC_ClearFlag();
    flash_boot_mode_user();
    NVIC_SystemReset();

}

/**
 * @brief Process incomming packet
 */
void process_packet(Packet_t* rx){
    uint8_t chip_id[8];
    uint8_t node_id;
    uint8_t firmware_id;
    uint32_t isBroadcast;

    //fetch info
    firmware_id = get_firmware_id();
    node_id = get_node_id();
    GetChipID64(&chip_id[0]);

    //check if the address is for us or broadcast
    if(rx->addr_len == 1){
        uint8_t adr8 = rx->address[0];
        isBroadcast = (adr8 == 0xFF); 
        if((adr8 != node_id) && !isBroadcast){
            return;
        } 
    }else{
        //check if the UID match.
        if(memcmp64(rx->address, chip_id) == 0){
            return;
        }
    }

    const uint8_t cmd = rx->command;
    const uint8_t datalen = rx->data_len;

    uint32_t tx_len=0;
    uint8_t* tx_ptr = (uint8_t*)&tx_data[0];


    if(cmd == BOOT_INFO){
        tx_len = 2;
        tx_ptr[0] = BOOTLOADER_MAJOR;
        tx_ptr[1] = BOOTLOADER_MINOR;
    }else if(cmd == BOOT_GET_CHIP){
        //Point tx_ptr to stored chip_name.
        tx_len = sizeof(chip_name);
        tx_ptr = (uint8_t*)&chip_name[0];
    }else if(cmd == BOOT_ERASE && datalen == 3){
        //only allow specific firmware.
        if(rx->data[0] != firmware_id){
            return;
        }

        uint16_t block = *(uint16_t*)(&rx->data[1]);
        uint32_t adr = 0x08000000 + block*64;

        if(adr < 0xFFFF){
            flash_erase(adr);
        }else{
            //TODO: bulk erase.
        }
    }else if(cmd == BOOT_WRITE && datalen == 70){
        //only allow specific firmware.
        if(rx->data[0] != firmware_id){
            return;
        }

        uint8_t corr = rx->data[1];
        
        //apply correction to adr+data.
        //This is done to avoid 0x7F in payload/address.
        //The host is responsible to calculate this number.
        //
        //Move data to beginning of rx buffer to get 4 byte boundry.
        //flash_write requires this.
        for(int i=0;i<68;i++){
            rx->data[i] =  rx->data[i+2] + corr;
        }

        //Fetch address
        uint32_t adr = *(uint32_t*)(&rx->data[0]);
        flash_erase(adr);
        flash_write(adr, &rx->data[4]);
        
    }else if(cmd == BOOT_GET_ID){
        //set response to UID.
        tx_len = 8;
        tx_ptr = &chip_id[0];

        //Delay according to delay window.
        if(datalen == 1){
            uint32_t slot;
            volatile uint32_t delay;

            //16..288 slots => ~1000ms to 11500ms
            uint32_t slot_count = rx->data[0] + 32;

            //"random" number by reusing CRC32 block.
            crc32_update(&slot, &chip_id[0], 8);
            crc32_update(&slot, &rx->data[0], 4);
            
            //take out lowest 16bits.
            slot = (uint16_t)slot;

            //use two step size to speed up the loop
            while(slot > slot_count){
                if(slot > slot_count*32){
                    slot -= slot_count*32;
                }else{
                    slot -= slot_count;
                }
            }

            //convert slot to time ~40millisec.
            delay = slot * 40000;

            //perform the delay.
            while(delay--){}
        }
    }else if(cmd == BOOT_SILENT){
        stay_silent=1;
    }else if(cmd == BOOT_UNSILENT){
        stay_silent=0;
    }else if(cmd == BOOT_GO){
        //handled after transmitt is done.
        boot_timeout=1;
    }else if(cmd == BOOT_GET_CRC32 && datalen == 8){
        uint32_t* ptr32 = (uint32_t*)&tx_ptr[0];
        uint32_t crc;

        uint32_t adr = *(uint32_t*)&rx->data[0];
        uint32_t cnt = *(uint32_t*)&rx->data[4];

        //Set response to CRC32.
        crc = crc32_calc((const uint8_t*)adr, cnt);

        //Data response.
        tx_len=4;
        ptr32[0] = crc;
    }else if(cmd == BOOT_GET_NODE_ID){
        tx_len=2;
        tx_ptr[0] = node_id;
        tx_ptr[1] = firmware_id;
    }else if(cmd == BOOT_SET_NODE_ID && datalen == 2){
        uint8_t subindex = rx->data[0];
        uint8_t value = rx->data[1];

        if(subindex == 0){
            flash_write_option_data(value, firmware_id);
        }

        if(subindex == 1){
            flash_write_option_data(node_id, value);
        }
    }else{
        //ignore invalid commands.
        return;
    }
    
    //check if we shall stay silence.
    if(stay_silent != 0){
        return;
    }

    //Build response
    uint8_t* tx_buffer_ptr = (uint8_t*)&tx_buffer[0];
    uint32_t packet_len = packet_serialize(tx_buffer_ptr, node_id, rx->command, tx_ptr, tx_len);
    while(packet_len--){
        uart_write(*tx_buffer_ptr++);
    }
}

/**
 * @brief initialize hardware
 */
void initialize(void){
    //Clock init
    //  HSI on, HSItrim 0x10
    //  Systclk / 3 => 8Mhz
    //  No wait states
    RCC->CTLR = 0x00000001 | (0x10<<3);
    RCC->CFGR0 = RCC_HPRE_DIV3;
    //FLASH->ACTLR = 0x00000000; //0x00 at Reset, no need to change.

    //Enable Clocks blocks
    RCC->APB2PCENR |= RCC_IOPDEN | RCC_USART1EN | RCC_AFIOEN;

    //Remapping:
    //  UARTTX to PD6, 
    //  PA-PA2 as GPIO
    AFIO->PCFR1 = 0x00208000;

    //GPIOD config
    // PD6 Alternate Function Open-Drain (Mode 3, CNF 3)
    // PD1 pull-up for debugging.
    GPIOD->CFGLR = 0x4F444484;

    uart_init();
}

/**
 * @brief Deintilize hardware
 */
void deinitilize(void){
    //Restore GPIOD config
    GPIOD->CFGLR = 0x44444484;

    uart_deinit();
}


/**
 * @brief Bootloader main.
 */
int main(){
    initialize();
    boot_timeout = 1<<20;       //~4.5 seconds
    

    while (1){
        //Must be run first, process_packet may change boot_timeout to exist bootloader.
        if(get_packet_total_sync_count() > 10){
            boot_timeout = 0;
        }
        
        //Handle incomming data.
        if(uart_available()){
            uint8_t rx = uart_read();

            if(Packet_Update_Rx(rx, &packet)){
                process_packet(&packet);
            }
        }

        //check if we should leave the bootloader.
        //This must be run last in main loop due to process_packet can set
        //boot_timeout to 0 to leave bootloader.
        if(boot_timeout > 0){
            boot_timeout--;

            if(boot_timeout == 0){                
                deinitilize();
                bootloader_start_app();
            }
        }
    }
}

