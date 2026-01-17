#include "uart.h"
#include <ch32v00x.h>

/**
 * @brief Write outgoing data
 */
void uart_write(uint8_t ch){
    //Wait for free buffer before adding new data.
    //may use USART_FLAG_TC instead?.
    while((USART1->STATR & USART_FLAG_TXE) == (uint16_t)RESET);
    USART1->DATAR = ch;
}

/**
 * @brief check if we got incomming data
 */
uint32_t uart_available(void){
    return (USART1->STATR & USART_FLAG_RXNE) != (uint16_t)RESET;
}

/**
 * @brief Read incomming data
 */
uint8_t uart_read(void){
    return (uint8_t)USART1->DATAR;
}


/**
 * @brief Init UART. Fixed baudrate to save flash. 
 */
void uart_init(void){
    //Configure UART
    // 9600 bps @ 8Mhz
    // Half-duplex
    // Eanabled with Tx and RX 
    USART1->BRR = 833;
    USART1->CTLR3 = USART_CTLR3_HDSEL; 
    USART1->CTLR1 = USART_CTLR1_UE | USART_CTLR1_TE | USART_CTLR1_RE;
}

void uart_deinit(void){
    //Disable Uart.
    USART1->CTLR1 = 0;
    USART1->CTLR3 = 0; 
}
