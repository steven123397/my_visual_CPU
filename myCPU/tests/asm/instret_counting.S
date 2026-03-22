# instret_counting.S - 验证 instret 只统计已退休指令
.section .text
.global _start
_start:
    li   s0, 0x10000000   # UART_BASE
    li   s5, '?'

    la   t0, trap_handler
    csrw mtvec, t0

    li   t0, 0
    csrw mscratch, t0

    li   s5, 'A'
    csrr s1, instret
    nop
    csrr s2, instret
    sub  t0, s2, s1
    li   t1, 2
    bne  t0, t1, fail

    li   t0, 1
    csrw mscratch, t0

    li   s5, 'B'
    csrr s3, instret
ecall_point:
    ecall

after_ecall:
    li   s5, 'C'
    sub  t0, s4, s3
    li   t1, 1
    bne  t0, t1, fail

    li   t0, 'I'
    sb   t0, 0(s0)
    li   t0, 'R'
    sb   t0, 0(s0)
    li   t0, 'T'
    sb   t0, 0(s0)

    li   a7, 93
    li   a0, 0
    ecall

trap_handler:
    csrr s4, instret
    li   s5, 'T'
    csrr t0, mscratch
    li   t1, 1
    bne  t0, t1, fail

    csrr t0, mcause
    li   t1, 11
    bne  t0, t1, fail

    csrr t0, mepc
    la   t1, ecall_point
    bne  t0, t1, fail

    la   t0, after_ecall
    csrw mepc, t0
    mret

fail:
    sb   s5, 0(s0)
    li   t0, 'X'
    sb   t0, 0(s0)
    li   a7, 93
    li   a0, 1
    ecall
