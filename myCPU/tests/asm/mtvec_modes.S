# mtvec_modes.S - 验证 mtvec direct/vectored 模式下的异常与中断入口选择
.section .text
.global _start
_start:
    li   s0, 0x10000000   # UART_BASE

    li   t0, 0
    csrw mscratch, t0

    # direct mode: exception should jump straight to handler
    la   t0, direct_handler
    csrw mtvec, t0
    ecall

after_direct:
    csrr t0, mscratch
    li   t1, 1
    bne  t0, t1, fail

    # vectored mode: exception should still land at base, not base + 4 * cause
    la   t0, vector_base
    ori  t0, t0, 1
    csrw mtvec, t0
    ecall

after_vectored_exception:
    csrr t0, mscratch
    li   t1, 2
    bne  t0, t1, fail

    # vectored mode: timer interrupt should land in slot 7
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
    li   t0, 8
    csrrs x0, mstatus, t0

wait_loop:
    j    wait_loop

after_timer_interrupt:
    li   t0, 0
    csrw mie, t0

    csrr t0, mscratch
    li   t1, 3
    bne  t0, t1, fail

    li   t0, 'M'
    sb   t0, 0(s0)
    li   t0, 'T'
    sb   t0, 0(s0)
    li   t0, 'V'
    sb   t0, 0(s0)

    li   a7, 93
    li   a0, 0
    ecall

direct_handler:
    csrr t0, mcause
    li   t1, 11
    bne  t0, t1, trap_fail

    li   t0, 1
    csrw mscratch, t0

    csrr t0, mepc
    addi t0, t0, 4
    csrw mepc, t0
    mret

.balign 32
vector_base:
    j    vectored_exception_handler
    j    unexpected_trap
    j    unexpected_trap
    j    unexpected_trap
    j    unexpected_trap
    j    unexpected_trap
    j    unexpected_trap
    j    timer_handler

vectored_exception_handler:
    csrr t0, mcause
    li   t1, 11
    bne  t0, t1, trap_fail

    li   t0, 2
    csrw mscratch, t0

    csrr t0, mepc
    addi t0, t0, 4
    csrw mepc, t0
    mret

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

    li   t0, 3
    csrw mscratch, t0

    csrr t0, mepc
    addi t0, t0, 4
    csrw mepc, t0
    mret

unexpected_trap:
    j    fail

trap_fail:
    j    fail

fail:
    li   t0, 'X'
    sb   t0, 0(s0)
    li   a7, 93
    li   a0, 1
    ecall
