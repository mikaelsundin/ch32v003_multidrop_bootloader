#include <stdint.h>
#include <stddef.h>
#include "flash.h"


// Register offsets (index of 32-bit array)
#define R_KEYR          (0x04 / 4)
#define R_OBKEYR        (0x08 / 4)
#define R_STATR         (0x0C / 4)
#define R_CTLR          (0x10 / 4)
#define R_ADDR          (0x14 / 4)
#define R_MODEKEYR      (0x24 / 4)
#define R_BOOT_MODEKEYR (0x28 / 4)

// --- Flash Control Bits ---
#define FLASH_KEY1      0x45670123
#define FLASH_KEY2      0xCDEF89AB
#define FLASH_BUSY      (1 << 0)
#define CR_OPTPG        (1 << 4)  // Option Byte Programming
#define CR_OPTER        (1 << 5)  // Option Byte Erase
#define CR_STRT         (1 << 6)  // Start Operation
#define CR_LOCK         (1 << 7)  // Lock Flash

/* Flash Control Register bits */
#define CR_PG_Set                  ((uint32_t)0x00000001)
#define CR_PER_Set                 ((uint32_t)0x00000002)
#define CR_MER_Set                 ((uint32_t)0x00000004)
#define CR_OPTPG_Set               ((uint32_t)0x00000010)
#define CR_OPTER_Set               ((uint32_t)0x00000020)
#define CR_STRT_Set                ((uint32_t)0x00000040)
#define CR_LOCK_Set                ((uint32_t)0x00000080)
#define CR_FLOCK_Set               ((uint32_t)0x00008000)
#define CR_PAGE_PG                 ((uint32_t)0x00010000)
#define CR_PAGE_ER                 ((uint32_t)0x00020000)
#define CR_BUF_LOAD                ((uint32_t)0x00040000)
#define CR_BUF_RST                 ((uint32_t)0x00080000)

/* FLASH Status Register bits */
#define SR_BSY                     ((uint32_t)0x00000001)
#define SR_WRPRTERR                ((uint32_t)0x00000010)
#define SR_EOP                     ((uint32_t)0x00000020)


// Helper to get flash controller address
static inline uint32_t* get_flash_regs(void) {
    uint32_t* base;
    __asm__ volatile (
        ".option push\n\t"      // Store current gcc settings
        ".option norelax\n\t"   // Prevent GP optimization    
        "lui %0, 0x40022\n\t"   // High 20 bits of 0x40022000
        ".option pop"           // Restore
        : "=r"(base));
    return base;
}

//Helper to get option byte address.
static inline uint32_t* get_ob_adr(void) {
    uint32_t* base;
    __asm__ volatile (
        ".option push\n\t"
        ".option norelax\n\t"
        "lui %0, 0x20000\n\t"      // Load 0x20000000
        "addi %0, %0, -0x800\n\t"  // 0x20000000 - 0x800 = 0x1FFFF800
        ".option pop"
        : "=r"(base));
    return base;
}

// Unlock option bytes area
void flash_unlock_options(volatile uint32_t* regs) {
    __asm__ volatile (
        ".option push\n\t"
        ".option norelax\n\t"
        
        "li t0, 0x45670123\n\t"  // Load Key 1
        "li t1, 0xCDEF89AB\n\t"  // Load Key 2
        
        "sw t0, 4(%0)\n\t"       // Store Key 1 to R_KEYR
        "sw t1, 4(%0)\n\t"       // Store Key 2 to R_KEYR
        
        "sw t0, 8(%0)\n\t"       // Store Key 1 to R_OBKEYR
        "sw t1, 8(%0)\n\t"       // Store Key 2 to R_OBKEYR
        
        ".option pop"
        : 
        : "r"(regs)
        : "t0", "t1", "memory"
    );
}

// Unlock Flash
void flash_unlock(volatile uint32_t* regs) {
    __asm__ volatile (
        ".option push\n\t"
        ".option norelax\n\t"
        
        // 1. Load the 32-bit keys into temporary registers t0 and t1
        // This expands to the LUI/ADDI sequence you saw in your disassembly
        "li t0, 0x45670123\n\t"  // FLASH_KEY1
        "li t1, 0xCDEF89AB\n\t"  // FLASH_KEY2
        
        // 2. Write keys to R_KEYR (Offset 0x04)
        "sw t0, 4(%0)\n\t"
        "sw t1, 4(%0)\n\t"
        
        // 3. Write keys to R_MODEKEYR (Offset 0x24 = 36 decimal)
        "sw t0, 36(%0)\n\t"
        "sw t1, 36(%0)\n\t"

        ".option pop"
        : 
        : "r"(regs)
        : "t0", "t1", "memory"
    );
}


void flash_boot_mode_user(void) {
    volatile uint32_t* regs = get_flash_regs();
    flash_unlock(regs);

    regs[R_STATR] &= ~(1<<14);
    regs[R_CTLR] |= CR_LOCK_Set; 
}

void flash_erase(uint32_t adr) {
    volatile uint32_t* regs = get_flash_regs();

    flash_unlock(regs);

    regs[R_CTLR] |= CR_PAGE_ER;
    regs[R_ADDR] = adr;
    regs[R_CTLR] |= CR_STRT_Set;
    while(regs[R_STATR] & SR_BSY){};
    regs[R_CTLR] &= ~CR_PAGE_ER;
}

void flash_write(uint32_t adr, uint8_t data[64]) {

    volatile uint32_t *dst = (volatile uint32_t*)adr;
    volatile uint32_t* regs = get_flash_regs();
    uint32_t* buf = (uint32_t*)data;
    
    flash_unlock(regs);

    //clear buffer
    regs[R_CTLR] |= CR_PAGE_PG;
    regs[R_CTLR] |= CR_BUF_RST;
    while(regs[R_STATR] & SR_BSY);
    regs[R_CTLR] &= ~CR_PAGE_PG;

    //Load data into buffer.
    for(int i=0;i<64;i+=4)
    {
        //FLASH_BufLoad(adr+4*j, buf[j]);

        //set prog flag.
        regs[R_CTLR] |= CR_PAGE_PG;

        //copy data
        *dst++ = *buf++;

        //request store data
        regs[R_CTLR] |= CR_BUF_LOAD;
        while(regs[R_STATR] & SR_BSY);

        //Clear prog flag
        regs[R_CTLR] &= ~CR_PAGE_PG; //clear prog flag
     }

    //Request write buffered data.
    regs[R_CTLR] |= CR_PAGE_PG;
    regs[R_ADDR] = adr;
    regs[R_CTLR] |= CR_STRT_Set;
    while(regs[R_STATR] & SR_BSY);
    regs[R_CTLR] &= ~CR_PAGE_PG;
}

void flash_write_option_data(uint8_t data0, uint8_t data1) {
    volatile uint32_t* regs = get_flash_regs();
    volatile uint32_t* pu32_option = get_ob_adr();
    volatile uint16_t au16_holding[6];
    
    //pointers to data
    uint32_t *pu32_holding = (uint32_t *)au16_holding;
   
    //Access the registers as 32bit to save flash.
    pu32_holding[0] = pu32_option[0];                // RDPR and USER
    pu32_holding[1] = data0 | (data1 << 16);        // New data0/data1
    pu32_holding[2] = pu32_option[2];                // WRPR0 and WRPR1

    //Do an undocumented bit in OB->USER 0x20 tell the MCU to enter bootloader first?
    flash_unlock_options(regs);

    // Erase Option Bytes (Required before writing new values)
    regs[R_CTLR] |= CR_OPTER;
    regs[R_CTLR] |= CR_STRT;
    while (regs[R_STATR] & FLASH_BUSY);
    regs[R_CTLR] &= ~CR_OPTER;

    // Write data back
    regs[R_CTLR] |= CR_OPTPG;
    uint16_t *ob16p = (uint16_t *)pu32_option;
    for (int i = 0; i < 6; i++) {
        ob16p[i] = au16_holding[i];
        while (regs[R_STATR] & FLASH_BUSY);
     }
    regs[R_CTLR] &= ~CR_OPTPG;

    // Relock
    regs[R_CTLR] |= CR_LOCK;
}
