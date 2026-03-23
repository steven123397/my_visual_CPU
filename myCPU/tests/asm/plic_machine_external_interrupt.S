# plic_machine_external_interrupt.S - 验证 PLIC 驱动的 M-mode 外部中断递送与 claim/complete
#include "../../include/platform_mmio.h"
.section .text
.global _start
_start:
    li   s0, UART_BASE
    li   s1, PLIC_BASE

    la   t0, machine_trap
    csrw mtvec, t0

    # Source 1 priority = 1
    li   t0, 1
    li   t1, PLIC_PRIORITY_OFFSET(PLIC_SOURCE_UART_THRE)
    add  t1, s1, t1
    sw   t0, 0(t1)

    # Enable source 1 for machine context.
    li   t0, 2
    li   t1, PLIC_ENABLE_OFFSET(PLIC_CONTEXT_MACHINE)
    add  t1, s1, t1
    sw   t0, 0(t1)

    # Threshold = 0
    li   t1, PLIC_THRESHOLD_OFFSET(PLIC_CONTEXT_MACHINE)
    add  t1, s1, t1
    sw   zero, 0(t1)

    # Enable machine external interrupts globally.
    li   t0, 0x800
    csrw mie, t0
    li   t0, 8
    csrrs x0, mstatus, t0

    # Enable UART THRE interrupt to raise PLIC source 1.
    li   t0, UART_IER_THRI
    sb   t0, UART_REG_IER(s0)

machine_wait_loop:
    j    machine_wait_loop

after_interrupt:
    li   t0, 'P'
    sb   t0, 0(s0)
    li   t0, 'M'
    sb   t0, 0(s0)
    li   t0, 'E'
    sb   t0, 0(s0)

    li   a7, 93
    li   a0, 0
    ecall

machine_trap:
    csrr t0, mcause
    srli t1, t0, 63
    li   t2, 1
    bne  t1, t2, fail

    slli t1, t0, 1
    srli t1, t1, 1
    li   t2, 11
    bne  t1, t2, fail

    csrr t0, mepc
    la   t1, machine_wait_loop
    bne  t0, t1, fail

    li   t1, PLIC_CLAIM_OFFSET(PLIC_CONTEXT_MACHINE)
    add  t1, s1, t1
    lw   t2, 0(t1)
    li   t3, PLIC_SOURCE_UART_THRE
    bne  t2, t3, fail

    li   t0, 0
    sb   t0, UART_REG_IER(s0)

    sw   t2, 0(t1)

    la   t0, after_interrupt
    csrw mepc, t0
    mret

fail:
    li   t0, 'X'
    sb   t0, 0(s0)
    li   a7, 93
    li   a0, 1
    ecall
