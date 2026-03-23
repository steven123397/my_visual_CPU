#include "../../include/platform_mmio.h"
.option norelax

.equ TRAP_FRAME_SIZE, 240

.section .text.start
.global _start

_start:
    la   t0, machine_trap
    csrw mtvec, t0

    la   t0, supervisor_trap
    csrw stvec, t0

    li   t0, ((1 << 9) | (1 << 5))
    csrw mideleg, t0
    csrw sie, t0

    li   t0, 0x802        # MPP = S, SIE = 1
    csrw mstatus, t0

    la   t0, supervisor_entry
    csrw mepc, t0
    mret

supervisor_entry:
    lla  sp, boot_stack_top
    call kernel_main
    li   a0, 0
    call platform_shutdown

supervisor_trap:
    addi sp, sp, -TRAP_FRAME_SIZE
    sd   ra, 0(sp)
    sd   gp, 8(sp)
    sd   tp, 16(sp)
    sd   t0, 24(sp)
    sd   t1, 32(sp)
    sd   t2, 40(sp)
    sd   s0, 48(sp)
    sd   s1, 56(sp)
    sd   a0, 64(sp)
    sd   a1, 72(sp)
    sd   a2, 80(sp)
    sd   a3, 88(sp)
    sd   a4, 96(sp)
    sd   a5, 104(sp)
    sd   a6, 112(sp)
    sd   a7, 120(sp)
    sd   s2, 128(sp)
    sd   s3, 136(sp)
    sd   s4, 144(sp)
    sd   s5, 152(sp)
    sd   s6, 160(sp)
    sd   s7, 168(sp)
    sd   s8, 176(sp)
    sd   s9, 184(sp)
    sd   s10, 192(sp)
    sd   s11, 200(sp)
    sd   t3, 208(sp)
    sd   t4, 216(sp)
    sd   t5, 224(sp)
    sd   t6, 232(sp)

    call supervisor_trap_dispatch

    ld   ra, 0(sp)
    ld   gp, 8(sp)
    ld   tp, 16(sp)
    ld   t0, 24(sp)
    ld   t1, 32(sp)
    ld   t2, 40(sp)
    ld   s0, 48(sp)
    ld   s1, 56(sp)
    ld   a0, 64(sp)
    ld   a1, 72(sp)
    ld   a2, 80(sp)
    ld   a3, 88(sp)
    ld   a4, 96(sp)
    ld   a5, 104(sp)
    ld   a6, 112(sp)
    ld   a7, 120(sp)
    ld   s2, 128(sp)
    ld   s3, 136(sp)
    ld   s4, 144(sp)
    ld   s5, 152(sp)
    ld   s6, 160(sp)
    ld   s7, 168(sp)
    ld   s8, 176(sp)
    ld   s9, 184(sp)
    ld   s10, 192(sp)
    ld   s11, 200(sp)
    ld   t3, 208(sp)
    ld   t4, 216(sp)
    ld   t5, 224(sp)
    ld   t6, 232(sp)
    addi sp, sp, TRAP_FRAME_SIZE
    sret

supervisor_trap_fail:
    li   a0, 'X'
    call platform_uart_putc
    li   a0, 1
    call platform_shutdown

machine_trap:
    li   a0, 'X'
    call platform_uart_putc
    li   a0, 1
    call platform_shutdown

.section .bss
.balign 16
boot_stack:
    .space 4096
boot_stack_top:
