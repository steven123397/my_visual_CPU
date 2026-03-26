# mmio_access_faults.S - 验证非法 MMIO width/offset 通过 CPU 形成 access fault
#include "../../include/platform_mmio.h"
.section .text
.global _start
_start:
    li   s0, UART_BASE
    li   s1, UART_BASE
    li   s2, CLINT_BASE
    li   s3, PLIC_BASE
    li   s4, STORAGE_BASE

    li   t0, 0
    csrw mscratch, t0

    la   t0, trap_handler
    csrw mtvec, t0

uart_invalid_width_setup:
    li   t0, 'A'

uart_invalid_width:
    sw   t0, UART_REG_THR(s1)

after_uart_invalid_width:
    csrr t0, mscratch
    li   t1, 1
    bne  t0, t1, fail

uart_invalid_load:
    lb   t0, UART_REG_THR(s1)

after_uart_invalid_load:
    csrr t0, mscratch
    li   t1, 2
    bne  t0, t1, fail

clint_invalid_offset:
    lb   t0, 0(s2)

after_clint_invalid_offset:
    csrr t0, mscratch
    li   t1, 3
    bne  t0, t1, fail

clint_invalid_cross_window_setup:
    li   t0, CLINT_REG_MTIME
    add  t1, s2, t0

clint_invalid_cross_window:
    ld   t0, 1(t1)

after_clint_invalid_cross_window:
    csrr t0, mscratch
    li   t1, 4
    bne  t0, t1, fail

plic_invalid_width:
    ld   t0, PLIC_PRIORITY_OFFSET(PLIC_SOURCE_UART_THRE)(s3)

after_plic_invalid_width:
    csrr t0, mscratch
    li   t1, 5
    bne  t0, t1, fail

plic_invalid_store_pending_setup:
    li   t0, PLIC_PENDING_OFFSET
    add  t1, s3, t0

plic_invalid_store_pending:
    sw   zero, 0(t1)

after_plic_invalid_store_pending:
    csrr t0, mscratch
    li   t1, 6
    bne  t0, t1, fail

storage_invalid_offset_setup:
    li   t0, STORAGE_CMD_NONE

storage_invalid_offset:
    sd   t0, 0x48(s4)

after_storage_invalid_offset:
    csrr t0, mscratch
    li   t1, 7
    bne  t0, t1, fail

storage_invalid_load_command:
    ld   t0, STORAGE_REG_COMMAND(s4)

after_storage_invalid_load_command:
    csrr t0, mscratch
    li   t1, 8
    bne  t0, t1, fail

storage_invalid_window_cross_boundary:
    ld   t0, STORAGE_DATA_WINDOW_OFFSET + STORAGE_DATA_WINDOW_SIZE - 4(s4)

after_storage_invalid_window_cross_boundary:
    csrr t0, mscratch
    li   t1, 9
    bne  t0, t1, fail

    li   t0, 'M'
    sb   t0, 0(s0)
    li   t0, 'M'
    sb   t0, 0(s0)
    li   t0, 'I'
    sb   t0, 0(s0)
    li   t0, 'O'
    sb   t0, 0(s0)

    li   a7, 93
    li   a0, 0
    ecall

trap_handler:
    csrr t0, mepc

    la   t1, uart_invalid_width
    beq  t0, t1, handle_uart_invalid_width
    la   t1, uart_invalid_load
    beq  t0, t1, handle_uart_invalid_load
    la   t1, clint_invalid_offset
    beq  t0, t1, handle_clint_invalid_offset
    la   t1, clint_invalid_cross_window
    beq  t0, t1, handle_clint_invalid_cross_window
    la   t1, plic_invalid_width
    beq  t0, t1, handle_plic_invalid_width
    la   t1, plic_invalid_store_pending
    beq  t0, t1, handle_plic_invalid_store_pending
    la   t1, storage_invalid_offset
    beq  t0, t1, handle_storage_invalid_offset
    la   t1, storage_invalid_load_command
    beq  t0, t1, handle_storage_invalid_load_command
    la   t1, storage_invalid_window_cross_boundary
    beq  t0, t1, handle_storage_invalid_window_cross_boundary
    j    trap_fail

.macro HANDLE_MMIO_FAULT label, cause, addr, stage
\label:
    csrr t0, mcause
    li   t1, \cause
    bne  t0, t1, trap_fail
    csrr t0, mtval
    li   t1, \addr
    bne  t0, t1, trap_fail
    li   t0, \stage
    csrw mscratch, t0
    csrr t0, mepc
    addi t0, t0, 4
    csrw mepc, t0
    mret
.endm

HANDLE_MMIO_FAULT handle_uart_invalid_width, 7, (UART_BASE + UART_REG_THR), 1
HANDLE_MMIO_FAULT handle_uart_invalid_load, 5, (UART_BASE + UART_REG_THR), 2
HANDLE_MMIO_FAULT handle_clint_invalid_offset, 5, CLINT_BASE, 3
HANDLE_MMIO_FAULT handle_clint_invalid_cross_window, 5, (CLINT_BASE + CLINT_REG_MTIME + 1), 4
HANDLE_MMIO_FAULT handle_plic_invalid_width, 5, (PLIC_BASE + PLIC_PRIORITY_OFFSET(PLIC_SOURCE_UART_THRE)), 5
HANDLE_MMIO_FAULT handle_plic_invalid_store_pending, 7, (PLIC_BASE + PLIC_PENDING_OFFSET), 6
HANDLE_MMIO_FAULT handle_storage_invalid_offset, 7, (STORAGE_BASE + 0x48), 7
HANDLE_MMIO_FAULT handle_storage_invalid_load_command, 5, (STORAGE_BASE + STORAGE_REG_COMMAND), 8
HANDLE_MMIO_FAULT handle_storage_invalid_window_cross_boundary, 5, (STORAGE_BASE + STORAGE_DATA_WINDOW_OFFSET + STORAGE_DATA_WINDOW_SIZE - 4), 9

trap_fail:
    j    fail

fail:
    li   t0, 'X'
    sb   t0, 0(s0)
    li   a7, 93
    li   a0, 1
    ecall
