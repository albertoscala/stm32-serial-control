#pragma once

#include <stdint.h>

#define MESSAGE_IN_START    ((uint8_t)0xAA)
#define MESSAGE_IN_END      ((uint8_t)0xFF)

#define MESSAGE_OUT_START    ((uint8_t)0x55)
#define MESSAGE_OUT_END      ((uint8_t)0x99)

#define ARGS_SIZE 8

typedef enum {
    CMD_SSEGMENT = 1,
    CMD_TEMP = 2,
} command_t;

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

int read_message(message_in_t* message_in)
{
    return 0;
}

int write_message(message_out_t* message_out)
{
    return 0;
}



