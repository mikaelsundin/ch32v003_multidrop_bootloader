//
//This file contain UART transport layer for UNITY test framework
///

#ifndef UNITTEST_TRANSPORT_H
#define UNITTEST_TRANSPORT_H



#ifdef __cplusplus
extern "C" {
#endif

#include <ch32v00x.h> // Ensure this matches your SDK header
#include "debug.h"

#define OUTPUT_BAUDRATE 115200

static inline void unity_output_init(void) {
    GPIO_InitTypeDef  GPIO_InitStructure = {0};
    USART_InitTypeDef USART_InitStructure = {0};

    //Extra delay needed for platformio to be ready to receive data from the test.
    Delay_Init();
    Delay_Ms(100);

    // Enable Clocks: GPIOD (for PD6),  USART1, and AFIO (for remap)
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOD | RCC_APB2Periph_USART1 | RCC_APB2Periph_AFIO, ENABLE);

    // --- PD6 Configuration (UART Half-Duplex) ---
    // Remap USART1 to move TX/RX to PD6/PD5, Half duplex only PD6 will be used.
    GPIO_PinRemapConfig(GPIO_PartialRemap2_USART1, ENABLE);

    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_6;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AF_PP; // Use AF_PP for easy testing
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_Init(GPIOD, &GPIO_InitStructure);

    // --- USART1 Configuration ---
    USART_InitStructure.USART_BaudRate = OUTPUT_BAUDRATE;
    USART_InitStructure.USART_WordLength = USART_WordLength_8b;
    USART_InitStructure.USART_StopBits = USART_StopBits_1;
    USART_InitStructure.USART_Parity = USART_Parity_No;
    USART_InitStructure.USART_HardwareFlowControl = USART_HardwareFlowControl_None;
    USART_InitStructure.USART_Mode = USART_Mode_Tx | USART_Mode_Rx;
    USART_Init(USART1, &USART_InitStructure);

    // Enable Half-Duplex and Start
    USART_HalfDuplexCmd(USART1, ENABLE);
    USART_Cmd(USART1, ENABLE);
}

static inline void unity_output_char(int c) {
    while (USART_GetFlagStatus(USART1, USART_FLAG_TXE) == RESET);
    USART_SendData(USART1, c);
}

static inline void unity_output_flush(void) {
    //Do nothing
}

static inline void unity_output_complete(void) {
    //stay here to avoid run test again.
    while(1) {
        __asm__("nop");
    } 
}

#ifdef __cplusplus
}
#endif

#endif

