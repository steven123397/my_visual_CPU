# plic_supervisor_external_interrupt.S - 验证 PLIC 驱动的 S-mode 外部中断递送与 claim/complete
#include "../../include/platform_mmio.h"
.section .text
.global _start
_start:
    li   s0, UART_BASE
    li   s1, PLIC_BASE

    la   t0, machine_trap
    csrw mtvec, t0

    la   t0, supervisor_trap
    csrw stvec, t0

    # Configure the UART PLIC source priority.
    li   t0, 1
    li   t1, PLIC_PRIORITY_OFFSET(PLIC_SOURCE_UART_THRE)
    add  t1, s1, t1
    sw   t0, 0(t1)

    # Enable the UART source for supervisor context.
    li   t0, (1 << PLIC_SOURCE_UART_THRE)
    li   t1, PLIC_ENABLE_OFFSET(PLIC_CONTEXT_SUPERVISOR)
    add  t1, s1, t1
    sw   t0, 0(t1)

    # Threshold = 0
    li   t1, PLIC_THRESHOLD_OFFSET(PLIC_CONTEXT_SUPERVISOR)
    add  t1, s1, t1
    sw   zero, 0(t1)

    li   t0, 0x200
    csrw mideleg, t0
    csrw sie, t0

    li   t0, 0x802        # MPP = S, SIE = 1
    csrw mstatus, t0

    # Enable UART THRE interrupt to raise the UART PLIC source.
    li   t0, UART_IER_THRI
    sb   t0, UART_REG_IER(s0)

    la   t0, supervisor_wait_loop
    csrw mepc, t0
    mret

supervisor_wait_loop:
    j    supervisor_wait_loop

after_interrupt:
    li   t0, 'P'
    sb   t0, 0(s0)
    li   t0, 'S'
    sb   t0, 0(s0)
    li   t0, 'E'
    sb   t0, 0(s0)

    li   a7, 93
    li   a0, 0
    ecall

supervisor_trap:
    csrr t0, scause
    srli t1, t0, 63
    li   t2, 1
    bne  t1, t2, fail

    slli t1, t0, 1
    srli t1, t1, 1
    li   t2, 9
    bne  t1, t2, fail

    csrr t0, sepc
    la   t1, supervisor_wait_loop
    bne  t0, t1, fail

    li   t1, PLIC_CLAIM_OFFSET(PLIC_CONTEXT_SUPERVISOR)
    add  t1, s1, t1
    lw   t2, 0(t1)
    li   t3, PLIC_SOURCE_UART_THRE
    bne  t2, t3, fail

    li   t0, 0
    sb   t0, UART_REG_IER(s0)

    sw   t2, 0(t1)

    la   t0, after_interrupt
    csrw sepc, t0
    sret

machine_trap:
    j    fail

fail:
    li   t0, 'X'
    sb   t0, 0(s0)
    li   a7, 93
    li   a0, 1
    ecall
