#include "../include/stm32_qemu.h"

#include <stdbool.h>

#include "../include/messages.h"

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

    usart1_init();

    usart2_init();

    message_in_t msg;
    while (true) 
    {
        // Read message
        read_message(&msg);

        // Debug
        usart2_dbg_msg_in(&msg);

        // Exec message

        // Write response

    }
}