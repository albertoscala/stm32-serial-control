.syntax unified
.cpu cortex-m4
.thumb

.section .text.reset_handler
.thumb_func
.global reset_handler
reset_handler:
    bl main
    b .