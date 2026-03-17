# csr_trap.S - 验证 CSR 读写、立即数变体和 ecall/mret 路径
.section .text
.global _start
_start:
    li   s0, 0x10000000   # UART_BASE

    li   t0, 0x12
    csrw mscratch, t0
    csrr t1, mscratch
    li   t2, 0x12
    bne  t1, t2, fail

    li   t0, 0x0f
    csrrw t1, mscratch, t0
    li   t2, 0x12
    bne  t1, t2, fail

    li   t2, 0x03
    csrrs t1, mscratch, t2
    li   t3, 0x0f
    bne  t1, t3, fail

    li   t2, 0x03
    csrrc t1, mscratch, t2
    li   t3, 0x0f
    bne  t1, t3, fail
    csrr t1, mscratch
    li   t3, 0x0c
    bne  t1, t3, fail

    csrrwi t1, mscratch, 26
    li   t2, 0x0c
    bne  t1, t2, fail
    csrr t1, mscratch
    li   t2, 26
    bne  t1, t2, fail

    csrrsi t1, mscratch, 1
    li   t2, 26
    bne  t1, t2, fail
    csrrci t1, mscratch, 3
    li   t2, 27
    bne  t1, t2, fail
    csrr t1, mscratch
    li   t2, 24
    bne  t1, t2, fail

    la   t0, trap_handler
    csrw mtvec, t0
    csrr t1, mtvec
    bne  t0, t1, fail

    ecall

after_trap:
    li   t0, 'C'
    sb   t0, 0(s0)
    li   t0, 'S'
    sb   t0, 0(s0)
    li   t0, 'R'
    sb   t0, 0(s0)

    li   a7, 93
    li   a0, 0
    ecall

trap_handler:
    csrr t0, mcause
    li   t1, 11
    bne  t0, t1, trap_fail

    csrr t0, mepc
    addi t0, t0, 4
    csrw mepc, t0
    mret

trap_fail:
    la   t0, fail
    csrw mepc, t0
    mret

fail:
    li   t0, 'X'
    sb   t0, 0(s0)
    li   a7, 93
    li   a0, 1
    ecall
