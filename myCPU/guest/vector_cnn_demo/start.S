#include "../../include/platform_mmio.h"
.option norelax

.section .text.start
.global _start
.extern vector_cnn_demo_main

_start:
    la   t0, trap_fail
    csrw mtvec, t0
    la   sp, boot_stack_top
    call vector_cnn_demo_main
    li   a0, 1
    call platform_shutdown

trap_fail:
    li   a0, 'X'
    call platform_uart_putc
    li   a0, 1
    call platform_shutdown

.section .bss
.balign 16
boot_stack:
    .space 4096
boot_stack_top:
