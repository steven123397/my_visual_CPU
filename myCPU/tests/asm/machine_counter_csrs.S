# machine_counter_csrs.S - 验证 mcycle/minstret 与 cycle/instret 共享同一底层计数器
.section .text
.global _start
_start:
    li   s0, 0x10000000   # UART_BASE

    li   t0, 40
    csrw mcycle, t0
    csrr t1, mcycle
    li   t2, 41
    bne  t1, t2, fail

    li   t0, 50
    csrw mcycle, t0
    csrr t1, cycle
    li   t2, 51
    bne  t1, t2, fail

    li   t0, 60
    csrw minstret, t0
    csrr t1, minstret
    li   t2, 61
    bne  t1, t2, fail

    li   t0, 70
    csrw minstret, t0
    csrr t1, instret
    li   t2, 71
    bne  t1, t2, fail

    li   t0, 'M'
    sb   t0, 0(s0)
    li   t0, 'C'
    sb   t0, 0(s0)
    li   t0, 'R'
    sb   t0, 0(s0)

    li   a7, 93
    li   a0, 0
    ecall

fail:
    li   t0, 'X'
    sb   t0, 0(s0)
    li   a7, 93
    li   a0, 1
    ecall
