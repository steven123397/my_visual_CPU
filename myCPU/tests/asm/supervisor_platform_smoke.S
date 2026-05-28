# supervisor_platform_smoke.S - 最小 supervisor 平台层 smoke test，验证 UART/PLIC/storage 驱动入口
#include "../../include/platform_mmio.h"
.option norelax
.section .text
.global _start
_start:
    la   t0, machine_trap
    csrw mtvec, t0

    la   t0, supervisor_trap
    csrw stvec, t0

    li   t0, (1 << 9)     # delegate supervisor external interrupt
    csrw mideleg, t0
    csrw sie, t0

    li   t0, 0x802        # MPP = S, SIE = 1
    csrw mstatus, t0

    la   t0, supervisor_main
    csrw mepc, t0
    mret

supervisor_main:
    lla  s2, interrupt_seen
    sd   zero, 0(s2)
    call platform_plic_supervisor_init

    lla  a1, storage_buffer
    li   a0, 0
    call platform_storage_read_block
    bnez a0, fail

    lla  t0, storage_buffer
    lbu  t1, 0(t0)
    li   t2, 'S'
    bne  t1, t2, fail
    lbu  t1, 1(t0)
    li   t2, 't'
    bne  t1, t2, fail
    lbu  t1, 2(t0)
    li   t2, 'o'
    bne  t1, t2, fail
    lbu  t1, 3(t0)
    li   t2, 'r'
    bne  t1, t2, fail

    call platform_uart_enable_thre_irq

wait_for_irq:
    ld   t1, 0(s2)
    beqz t1, wait_for_irq

after_irq:
    li   a0, 'D'
    call platform_uart_putc
    li   a0, 'R'
    call platform_uart_putc
    li   a0, 'V'
    call platform_uart_putc

    li   a7, 93
    li   a0, 0
success_exit:
    ebreak

supervisor_trap:
    csrr t0, scause
    srli t1, t0, 63
    li   t2, 1
    bne  t1, t2, fail

    slli t1, t0, 1
    srli t1, t1, 1
    li   t2, 9
    bne  t1, t2, fail

    call platform_plic_supervisor_claim
    li   t0, PLIC_SOURCE_UART_THRE
    bne  a0, t0, fail

    call platform_uart_disable_irq

    li   a0, PLIC_SOURCE_UART_THRE
    call platform_plic_supervisor_complete

    li   t1, 1
    sd   t1, 0(s2)

    la   t0, after_irq
    csrw sepc, t0
    sret

machine_trap:
    csrr t0, mcause
    li   t1, 3
    bne  t0, t1, fail

    csrr t0, mepc
    la   t1, success_exit
    bne  t0, t1, fail

    li   a7, 93
    li   a0, 0
    ecall

fail:
    li   a0, 'X'
    call platform_uart_putc
    li   a7, 93
    li   a0, 1
    ecall

#include "include/platform_drivers.inc"

.section .bss
.balign 16
storage_buffer:
    .space STORAGE_BLOCK_SIZE

interrupt_seen:
    .dword 0
