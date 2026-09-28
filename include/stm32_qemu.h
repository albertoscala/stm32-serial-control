#pragma once

#include <stdint.h>

// USART

#define RCC_APB2ENR   (*(volatile uint32_t *)0x40023844)
#define USART1_SR     (*(volatile uint32_t *)0x40011000)
#define USART1_DR     (*(volatile uint32_t *)0x40011004)
#define USART1_BRR    (*(volatile uint32_t *)0x40011008)
#define USART1_CR1    (*(volatile uint32_t *)0x4001100C)

#define RCC_USART1EN  ((uint32_t)1 << 4)
#define USART_SR_TXE  ((uint32_t)1 << 7)
#define USART_CR1_TE  ((uint32_t)1 << 3)
#define USART_CR1_UE  ((uint32_t)1 << 13)

void usart_init(void)
{
    RCC_APB2ENR |= RCC_USART1EN;             /* clock on */
    USART1_BRR   = 16000000 / 115200;        /* 16 MHz HSI, 115200 baud */
    USART1_CR1   = USART_CR1_UE | USART_CR1_TE;
}

void usart_putc(unsigned char c)
{
    while (!(USART1_SR & USART_SR_TXE)) {}
    USART1_DR = (uint32_t)c;
}

void usart_puts(const char* s)
{
    while(*s)
        usart_putc(*(s++));
}