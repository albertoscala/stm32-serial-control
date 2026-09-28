#pragma once

#include <stdint.h>
#include <stdbool.h>

#include "stm32_qemu.h"

#define MESSAGE_IN_START    ((uint8_t)0xAA)
#define MESSAGE_IN_END      ((uint8_t)0xFF)

#define MESSAGE_OUT_START    ((uint8_t)0x55)
#define MESSAGE_OUT_END      ((uint8_t)0x99)

#define ARGS_SIZE 8

// COMMANDS

typedef enum {
    CMD_SSEGMENT = 1,
    CMD_TEMP = 2,
} command_t;

// INTERFACES

typedef struct __attribute__((packed)) 
{
    uint8_t start;
    uint8_t cmd;
    uint8_t args[ARGS_SIZE];
    uint8_t end;
} message_in_t;

typedef struct __attribute__((packed)) 
{
    uint8_t start;
    uint8_t cmd;
    uint8_t args[ARGS_SIZE];
    uint8_t end;
} message_out_t;

// PARSING

int read_message(message_in_t* message_in)
{
    uint8_t* raw = (uint8_t*)message_in;   /* fill the struct byte by byte */
    uint8_t n = 0;                         /* bytes collected so far */

    while (true) 
    {
        uint8_t b = (uint8_t)usart1_getbyte();

        if (n == 0) {
            /* not inside a frame: discard everything until a start byte */
            if (b != MESSAGE_IN_START)
                continue;
            raw[n++] = b;
            continue;
        }

        raw[n++] = b;

        if (n < sizeof(message_in_t))
            continue;                       /* frame not complete yet */

        /* the struct is full: the last byte must be the end marker */
        if (raw[n - 1] == MESSAGE_IN_END)
            return 1;                       /* valid message */

        /* malformed: drop the first byte and look for the next start byte
           among the ones we already have, then keep collecting from there */
        uint8_t i = 1;
        while (i < n && raw[i] != MESSAGE_IN_START)
            i++;

        uint8_t k = 0;
        while (i < n)
            raw[k++] = raw[i++];
        n = k;
    }
}

int write_message(message_out_t* message_out)
{
    return 0;
}

// DEBUG

void usart2_dbg_msg_in(message_in_t* message_in)
{
    usart2_puts("===== Message In Content =====\n");
    
    usart2_puts("message_in_t.start: "); 
    usart2_puthexbyte(message_in->start); 
    usart2_putc('\n'); 
    
    usart2_puts("message_in_t.cmd: "); 
    usart2_puthexbyte(message_in->cmd); 
    usart2_putc('\n');
    
    usart2_puts("message_in_t.args:  ");
    for (int i = 0; i < ARGS_SIZE; i++) 
    {
        usart2_puthexbyte(message_in->args[i]);
        usart2_putc(' ');
    }
    usart2_putc('\n');
    
    usart2_puts("message_in_t.end: "); 
    usart2_puthexbyte(message_in->end); 
    usart2_putc('\n');
    
    usart2_puts("==============================\n");
}

