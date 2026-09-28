#include "../include/stm32_qemu.h"

extern uint32_t _estack;
extern uint32_t _sidata, _sdata, _edata;
extern uint32_t _sbss, _ebss;

int core()
{
    

    usart_puts("hello world!\n");

    return 0;
}