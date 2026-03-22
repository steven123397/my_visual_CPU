# access_faults.S - 验证 unmapped fetch/load/store 触发 access fault trap
.section .text
.global _start
_start:
    li   s0, 0x10000000   # UART_BASE

    li   t0, 0
    csrw mscratch, t0

    la   t0, trap_handler
    csrw mtvec, t0

    li   t0, 0x1800       # stay in M-mode when returning from the handler
    csrw mstatus, t0
    li   t0, 0
    csrw mepc, t0
    mret

load_fault_point:
    ld   t0, 0(zero)
    j    fail

store_fault_setup:
    li   t0, 0x55

store_fault_point:
    sd   t0, 0(zero)
    j    fail

after_tests:
    li   t0, 'A'
    sb   t0, 0(s0)
    li   t0, 'C'
    sb   t0, 0(s0)
    li   t0, 'F'
    sb   t0, 0(s0)

    li   a7, 93
    li   a0, 0
    ecall

trap_handler:
    csrr t0, mscratch
    beqz t0, handle_instruction_fault

    li   t1, 1
    beq  t0, t1, handle_load_fault

    li   t1, 2
    beq  t0, t1, handle_store_fault

    j    fail

handle_instruction_fault:
    csrr t0, mcause
    li   t1, 1
    bne  t0, t1, fail

    csrr t0, mepc
    bnez t0, fail

    csrr t0, mtval
    bnez t0, fail

    li   t0, 1
    csrw mscratch, t0
    la   t0, load_fault_point
    csrw mepc, t0
    mret

handle_load_fault:
    csrr t0, mcause
    li   t1, 5
    bne  t0, t1, fail

    csrr t0, mepc
    la   t1, load_fault_point
    bne  t0, t1, fail

    csrr t0, mtval
    bnez t0, fail

    li   t0, 2
    csrw mscratch, t0
    la   t0, store_fault_setup
    csrw mepc, t0
    mret

handle_store_fault:
    csrr t0, mcause
    li   t1, 7
    bne  t0, t1, fail

    csrr t0, mepc
    la   t1, store_fault_point
    bne  t0, t1, fail

    csrr t0, mtval
    bnez t0, fail

    la   t0, after_tests
    csrw mepc, t0
    mret

fail:
    li   t0, 'X'
    sb   t0, 0(s0)
    li   a7, 93
    li   a0, 1
    ecall
