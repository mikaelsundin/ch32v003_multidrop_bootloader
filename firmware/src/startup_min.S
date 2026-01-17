/*****************************************************************************
* Minimal Bootloader Startup for CH32V00x (no interrupts)
******************************************************************************/
    .section  .init, "ax", @progbits
    .globl  _start
    .align  2
_start:
    .option norvc
    j       handle_reset        /* Reset Handler */                             //0x1FFFF000
    .word   0                   /* NMI */                                       //0x1FFFF004
    .word   HardFault_Handler   /* Hard Fault - Kept for debugging crashes */   //0x1FFFF008

    /* Jump table - Expose some functions to main APP */
    j flash_write               //0x1FFFF00C
    j flash_erase               //0x1FFFF010
    j flash_write_option_data   //0x1FFFF014
    j crc32_calc                //0x1FFFF018
    
    /* All other vectors removed to save space */

    .section  .text.vector_handler, "ax", @progbits
    .weak   HardFault_Handler
HardFault_Handler:
1:  j 1b

    .section  .text.handle_reset, "ax", @progbits
    .globl    handle_reset
    .align    1
handle_reset:
.option push
.option norelax
    la gp, __global_pointer$    /* Initialize Global Pointer */
.option pop
    la sp, _eusrstack           /* Initialize Stack Pointer */

/* Load data section from flash to RAM */
    la a0, _data_lma
    la a1, _data_vma
    la a2, _edata
    bgeu a1, a2, 2f
1:  lw t0, (a0)
    sw t0, (a1)
    addi a0, a0, 4
    addi a1, a1, 4
    bltu a1, a2, 1b
2:

/* Clear bss section */
    la a0, _sbss
    la a1, _ebss
    bgeu a0, a1, 2f
1:  sw zero, (a0)
    addi a0, a0, 4
    bltu a0, a1, 1b
2:

    /* Basic CPU Setup */
    //Default is 0x1800 = 
    //li t0, 0x1880               /* MSTATUS: Enable Privileged mode */
    //csrw mstatus, t0

    //Set interrupt vector, ignore we do not use interrupt in bootloader
    //la t0, _start
    //ori t0, t0, 3               /* Set MTVEC to start address */
    //csrw mtvec, t0

    //la t0, main
    //csrw mepc, t0
    //mret                        /* Jump to main */
    j main