#include "../include/stm32_qemu.h"

#include <stdbool.h>

// .data section labels
extern uint32_t _sidata, _sdata, _edata;
// .bss section labels
extern uint32_t _sbss, _ebss;

void init_data()
{
    // Copy .data from FLASH to RAM
    uint32_t* ssrc = &_sidata;
    uint32_t* sdst = &_sdata;
    while (sdst < &_edata)
        *(sdst++) = *(ssrc++);
}

void init_bss()
{
    // Zeroing the .bss
    uint32_t* sbss = &_sbss;
    while (sbss < &_ebss)
        *(sbss++) = 0; 
}

void main()
{   
    init_data();

    init_bss();

    usart_init();

    while (true) 
    {
        // Read message

        // Exec message

        // Write response
    }
}