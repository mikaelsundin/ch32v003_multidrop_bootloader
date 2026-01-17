#ifndef FLASH_H
#define FLASH_H

#include <stdint.h>
#include <stddef.h>

//Set flash boot mode.
void flash_boot_mode_user(void);

//Flase erase and write function.
void flash_erase(uint32_t adr);
void flash_write(uint32_t adr, uint8_t data[64]);
void flash_write_option_data(uint8_t data0, uint8_t data1);



#endif