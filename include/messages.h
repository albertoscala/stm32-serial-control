#pragma once

#include <stdint.h>
#include <stdbool.h>

#include "stm32_qemu.h"

#include "sha256.h"
#include "hmac.h"
#include "secrets.h"

#define MESSAGE_IN_START    ((uint8_t)0xAA)
#define MESSAGE_IN_END      ((uint8_t)0xFF)

#define MESSAGE_OUT_START    ((uint8_t)0x55)
#define MESSAGE_OUT_END      ((uint8_t)0x99)

#define ARGS_SIZE       8
#define COUNTER_SIZE    4
#define HMAC_SIZE       16

static uint32_t last_counter = 0;

// COMMANDS

typedef enum {
    CMD_SSEGMENT = 1,
} command_t;

// INTERFACES

typedef struct __attribute__((packed)) 
{
    uint8_t start;
    uint8_t cmd;
    uint8_t args[ARGS_SIZE];
    uint8_t counter[COUNTER_SIZE];
    uint8_t hmac[HMAC_SIZE];
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

static uint32_t get_counter(const message_in_t* m)
{
    return (uint32_t)m->counter[0]
         | (uint32_t)m->counter[1] << 8
         | (uint32_t)m->counter[2] << 16
         | (uint32_t)m->counter[3] << 24; // Little endian
}

void compute_hmac(const uint8_t* cmd, uint8_t tag[HMAC_SIZE])
{
    uint8_t full[SIZE_OF_SHA_256_HASH];

    hmac_sha256(KEY, sizeof(KEY), cmd, 1 + ARGS_SIZE + COUNTER_SIZE, full);
    for (int i = 0; i < HMAC_SIZE; i++)
        tag[i] = full[i];
}

void read_message(message_in_t* message_in)
{
    uint8_t* raw = (uint8_t*)message_in;   // fill the struct byte by byte
    uint8_t n = 0;                         // bytes collected so far

    while (true) 
    {
        uint8_t b = (uint8_t)usart1_getbyte();

        if (n == 0) {
            // not inside a frame: discard everything until a start byte
            if (b != MESSAGE_IN_START)
                continue;
            raw[n++] = b;
            continue;
        }

        raw[n++] = b;

        if (n < sizeof(message_in_t))
            // frame not complete yet
            continue;                       

        // the struct is full: the last byte must be the end marker
        if (raw[n - 1] == MESSAGE_IN_END)
            // valid message
            return;                         

        // malformed: drop the first byte and look for the next start byte
        // among the ones we already have, then keep collecting from there
        uint8_t i = 1;
        while (i < n && raw[i] != MESSAGE_IN_START)
            i++;

        uint8_t k = 0;
        while (i < n)
            raw[k++] = raw[i++];
        n = k;
    }
}

bool validate_message(message_in_t* message_in)
{
    // Auth with HMAC
    uint8_t tag[HMAC_SIZE];
    uint8_t diff = 0;

    // Recompute hmac
    compute_hmac(&message_in->cmd, tag);

    // Verify counter
    for (int i = 0; i < HMAC_SIZE; i++)
        diff |= tag[i] ^ message_in->hmac[i];
    if (diff != 0)
        return false;
    
    // Verify counter
    uint32_t ctr = get_counter(message_in);
    if (ctr <= last_counter)
        return false;

    // Validating the message content
    switch (message_in->cmd)
    {
        case CMD_SSEGMENT:
            if (message_in->args[0] > 9) // args[0] is the digit to show: 0 -> 9
                return false;
            last_counter = ctr;
            return true;
        // TODO: Add more commands
        default:
            return false;
    }
}

void write_message(message_out_t* message_out)
{
    message_out->start   = MESSAGE_OUT_START;
    message_out->args[0] = 1;
    message_out->end     = MESSAGE_OUT_END;

    const uint8_t* raw = (const uint8_t*)message_out;
    for (uint8_t i = 0; i < sizeof(message_out_t); i++)
        usart1_putbyte(raw[i]);
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

