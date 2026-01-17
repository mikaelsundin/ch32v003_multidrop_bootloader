#ifndef CMD_H
#define CMD_H

//Get bootloader version.
#define BOOT_INFO           (0x01u)

//Get chip type
#define BOOT_GET_CHIP       (0x02u)

//Search commands
#define BOOT_GET_ID         (0x11u)
#define BOOT_SILENT         (0x12u)
#define BOOT_UNSILENT       (0x13u)

//Erase and Write FLASH
#define BOOT_WRITE          (0x31)
#define BOOT_ERASE          (0x44)

//Change run address/reboot into flash.
#define BOOT_GO             (0x21)

//Calculate CRC32 of a area
#define BOOT_GET_CRC32      (0xA1)

//Set node-id and/or firmware-id
#define BOOT_GET_NODE_ID    (0xC1)
#define BOOT_SET_NODE_ID    (0xC2)


#endif