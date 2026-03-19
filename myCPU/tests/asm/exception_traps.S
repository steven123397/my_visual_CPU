# exception_traps.S - 验证 breakpoint / illegal instruction trap 语义
.section .text
.global _start
_start:
    li   s0, 0x10000000   # UART_BASE

    li   t0, 0
    csrw mscratch, t0

    la   t0, trap_handler
    csrw mtvec, t0

breakpoint_point:
    ebreak

after_breakpoint:
    csrr t0, mscratch
    li   t1, 1
    bne  t0, t1, fail

illegal_point:
    .word 0xffffffff

after_illegal:
    csrr t0, mscratch
    li   t1, 2
    bne  t0, t1, fail

    li   t0, 'E'
    sb   t0, 0(s0)
    li   t0, 'X'
    sb   t0, 0(s0)
    li   t0, 'C'
    sb   t0, 0(s0)

    li   a7, 93
    li   a0, 0
    ecall

trap_handler:
    csrr t0, mcause

    li   t1, 3
    beq  t0, t1, handle_breakpoint

    li   t1, 2
    beq  t0, t1, handle_illegal

    j    trap_fail

handle_breakpoint:
    csrr t0, mepc
    la   t1, breakpoint_point
    bne  t0, t1, trap_fail

    li   t0, 1
    csrw mscratch, t0

    csrr t0, mepc
    addi t0, t0, 4
    csrw mepc, t0
    mret

handle_illegal:
    csrr t0, mepc
    la   t1, illegal_point
    bne  t0, t1, trap_fail

    csrr t0, mtval
    li   t1, 1
    slli t1, t1, 32
    addi t1, t1, -1
    bne  t0, t1, trap_fail

    li   t0, 2
    csrw mscratch, t0

    csrr t0, mepc
    addi t0, t0, 4
    csrw mepc, t0
    mret

trap_fail:
    j    fail

fail:
    li   t0, 'X'
    sb   t0, 0(s0)
    li   a7, 93
    li   a0, 1
    ecall
