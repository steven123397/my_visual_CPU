# timer_interrupt.S - 验证 CLINT 定时器中断、mtvec 向量入口和 mret 返回
.section .text
.global _start
_start:
    li   s0, 0x10000000   # UART_BASE

    li   t0, 0
    csrw mscratch, t0

    la   t0, vector_table
    ori  t0, t0, 1
    csrw mtvec, t0

    li   t0, 0x02000000   # CLINT_BASE
    li   t1, 0xBFF8       # mtime
    add  t2, t0, t1
    ld   t3, 0(t2)
    addi t3, t3, 8
    li   t1, 0x4000       # mtimecmp
    add  t2, t0, t1
    sd   t3, 0(t2)

    li   t0, 0x80
    csrw mie, t0
    csrr t1, mie
    li   t2, 0x80
    bne  t1, t2, fail

    li   t0, 8
    csrrs x0, mstatus, t0

wait_loop:
    j    wait_loop

after_interrupt:
    csrr t0, mscratch
    li   t1, 1
    bne  t0, t1, fail

    csrr t0, mip
    li   t1, 0x80
    and  t0, t0, t1
    bnez t0, fail

    li   t0, 'T'
    sb   t0, 0(s0)
    li   t0, 'M'
    sb   t0, 0(s0)
    li   t0, 'R'
    sb   t0, 0(s0)

    li   a7, 93
    li   a0, 0
    ecall

.balign 32
vector_table:
    j    unexpected_trap
    j    unexpected_trap
    j    unexpected_trap
    j    unexpected_trap
    j    unexpected_trap
    j    unexpected_trap
    j    unexpected_trap
    j    timer_handler

unexpected_trap:
    j    fail

timer_handler:
    csrr t0, mcause
    srli t1, t0, 63
    li   t2, 1
    bne  t1, t2, trap_fail

    slli t1, t0, 1
    srli t1, t1, 1
    li   t2, 7
    bne  t1, t2, trap_fail

    li   t0, 0x02000000   # CLINT_BASE
    li   t1, 0x4000       # mtimecmp
    add  t2, t0, t1
    li   t3, -1
    sd   t3, 0(t2)

    li   t0, 0x80
    csrrc x0, mip, t0

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
