.syntax unified
.cpu cortex-m4
.thumb

/* Init the vector table */
.section .isr_vector, "a"
.word _estack             /* word 0: initial SP (loaded by the CPU) */
.word reset_handler       /* word 1: reset vector (loaded into PC) */
.word default_handler     /* word 2: NMI */
.word default_handler     /* word 3: HardFault */

.section .text.reset_handler
.thumb_func
.global reset_handler
reset_handler:
    bl main
    b .

.thumb_func
default_handler:
    b .
