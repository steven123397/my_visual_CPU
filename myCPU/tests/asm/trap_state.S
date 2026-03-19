# trap_state.S - 验证 trap 进入/返回时 mstatus 与 mepc 的关键状态变化
.section .text
.global _start
_start:
    li   s0, 0x10000000   # UART_BASE

    li   t0, 0
    csrw mscratch, t0

    la   t0, trap_handler
    csrw mtvec, t0

    li   t0, 8            # MSTATUS_MIE
    csrrs x0, mstatus, t0

trap_point:
    ecall

after_trap:
    csrr t0, mscratch
    li   t1, 1
    bne  t0, t1, fail

    csrr t0, mstatus
    li   t1, 8
    and  t2, t0, t1
    beqz t2, fail

    li   t1, 0x80
    and  t2, t0, t1
    beqz t2, fail

    li   t0, 'S'
    sb   t0, 0(s0)
    li   t0, 'T'
    sb   t0, 0(s0)
    li   t0, 'S'
    sb   t0, 0(s0)

    li   a7, 93
    li   a0, 0
    ecall

trap_handler:
    csrr t0, mcause
    li   t1, 11
    bne  t0, t1, trap_fail

    csrr t0, mepc
    la   t1, trap_point
    bne  t0, t1, trap_fail

    csrr t0, mstatus
    li   t1, 8
    and  t2, t0, t1
    bnez t2, trap_fail

    li   t1, 0x80
    and  t2, t0, t1
    beqz t2, trap_fail

    li   t0, 1
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
