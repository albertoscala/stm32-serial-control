#pragma once

#include <stdint.h>

// SPECS

#define BAUD_RATE   ((uint32_t)(16000000 / 115200))     // 16 MHz HSI, 115200 baud

// USART

// 2 USART

// USART1 -> COMM

#define RCC_APB2ENR   (*(volatile uint32_t*)0x40023844)    // Clock enable register for APB2 peripherals
#define USART1_SR     (*(volatile uint32_t*)0x40011000)    // Status register of the USART1
#define USART1_DR     (*(volatile uint32_t*)0x40011004)    // Data register of the USART1
#define USART1_BRR    (*(volatile uint32_t*)0x40011008)    // Baud rate register for USART1
#define USART1_CR1    (*(volatile uint32_t*)0x4001100C)    // Control register for USART1

#define RCC_USART1EN  ((uint32_t)1 << 4)                    // USART1 clock enable

// USART2 -> DBG

#define RCC_APB1ENR   (*(volatile uint32_t*)0x40023840)    // Clock enable register for APB1 peripherals
#define USART2_SR     (*(volatile uint32_t*)0x40004400)    // Status register of the USART2
#define USART2_DR     (*(volatile uint32_t*)0x40004404)    // Data register of the USART2
#define USART2_BRR    (*(volatile uint32_t*)0x40004408)    // Baud rate register for USART2
#define USART2_CR1    (*(volatile uint32_t*)0x4000440C)    // Control register for USART2

#define RCC_USART2EN  ((uint32_t)1 << 17)                   // USART2 clock enable

// USART3 -> 7 SEGMENT DISPLAY

// RCC_APB1ENR is shared between USART2 and USART3
#define USART3_SR     (*(volatile uint32_t*)0x40004800)    // Status register of the USART3
#define USART3_DR     (*(volatile uint32_t*)0x40004804)    // Data register of the USART3
#define USART3_BRR    (*(volatile uint32_t*)0x40004808)    // Baud rate register for USART3
#define USART3_CR1    (*(volatile uint32_t*)0x4000480C)    // Control register for USART3

#define RCC_USART3EN  ((uint32_t)1 << 18)                   // USART3 clock enable (APB1)

// FLAGS

#define USART_SR_TXE  ((uint32_t)1 << 7)    // USART TX status register bitmasks
#define USART_SR_RXNE ((uint32_t)1 << 5)    // USART RX status register bitmasks
#define USART_CR1_UE  ((uint32_t)1 << 13)   // USART enable
#define USART_CR1_TE  ((uint32_t)1 << 3)    // USART enable TX
#define USART_CR1_RE  ((uint32_t)1 << 2)    // USART enable RX

// INIT USARTS

static inline void usart1_init()
{
    RCC_APB2ENR |= RCC_USART1EN;
    USART1_BRR   = BAUD_RATE;
    USART1_CR1   = USART_CR1_UE | USART_CR1_TE | USART_CR1_RE;
}

static inline void usart2_init()
{
    RCC_APB1ENR |= RCC_USART2EN;
    USART2_BRR   = BAUD_RATE;
    USART2_CR1   = USART_CR1_UE | USART_CR1_TE;
}

static inline void usart3_init()
{
    RCC_APB1ENR |= RCC_USART3EN;
    USART3_BRR   = BAUD_RATE;
    USART3_CR1   = USART_CR1_UE | USART_CR1_TE;
}

// USART1 IO

static inline void usart1_putbyte(unsigned char c)
{
    while (!(USART1_SR & USART_SR_TXE)) {}
    USART1_DR = (uint32_t)c;
}

static inline unsigned char usart1_getbyte()
{
    while (!(USART1_SR & USART_SR_RXNE)) {}
    return (unsigned char)USART1_DR;
}

static inline void usart1_putbytes(const char* s)
{
    while(*s)
        usart1_putbyte(*(s++));
}

// USART2 IO

static inline void usart2_putc(unsigned char c)
{
    while (!(USART2_SR & USART_SR_TXE)) {}
    USART2_DR = (uint32_t)c;
}

static inline void usart2_puts(const char* s)
{
    while(*s)
        usart2_putc(*(s++));
}

static inline void usart2_puthexbyte(uint8_t b)
{
    static const char hex[] = "0123456789ABCDEF";
    usart2_putc(hex[(b >> 4) & 0xF]);
    usart2_putc(hex[b & 0xF]);
}