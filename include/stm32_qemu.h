#pragma once

#include <stdint.h>

#define USART2_BASE  ((uintptr_t)0x40020000)
#define USART2_SR    (*(volatile uint32_t*)(USART2_BASE + 0x00))
#define USART2_DR    (*(volatile uint32_t*)(USART2_BASE + 0x04))
#define USART_SR_TXE ((uint32_t)1 << 7)

void usart_put(unsigned char c)
{
    while (!(USART2_SR & USART_SR_TXE)) {}
    USART2_DR = (uint32_t)c;
}

void usart_puts(const char* s)
{
    for (int i = 0; s[i] != '\0'; i++)
    {
        usart_put(s[i]);
    }
}