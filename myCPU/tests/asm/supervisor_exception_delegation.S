# supervisor_exception_delegation.S - 验证 medeleg 将 U-mode ecall 委派到 S-mode
.section .text
.global _start
_start:
    li   s0, 0x10000000   # UART_BASE

    la   t0, machine_trap
    csrw mtvec, t0

    la   t0, supervisor_trap
    csrw stvec, t0

    li   t0, 1
    slli t0, t0, 8        # delegate U-mode ecall (cause 8)
    csrw 0x302, t0        # medeleg

    li   t0, 0            # MPP = U
    csrw mstatus, t0

    li   t0, 2            # SSTATUS_SIE, keep enabled across delegated trap entry
    csrw sstatus, t0

    la   t0, user_ecall
    csrw mepc, t0
    mret

user_ecall:
    ecall

after_supervisor_return:
    li   t0, 'S'
    sb   t0, 0(s0)
    li   t0, 'D'
    sb   t0, 0(s0)
    li   t0, 'G'
    sb   t0, 0(s0)

    li   a7, 93
    li   a0, 0
    ecall

supervisor_trap:
    csrr t0, scause
    li   t1, 8
    bne  t0, t1, fail

    csrr t0, sepc
    la   t1, user_ecall
    bne  t0, t1, fail

    csrr t0, stval
    bnez t0, fail

    csrr t0, sstatus
    li   t1, 2
    and  t2, t0, t1
    bnez t2, fail

    li   t1, 0x20
    and  t2, t0, t1
    beqz t2, fail

    li   t1, 0x100
    and  t2, t0, t1
    bnez t2, fail

    la   t0, after_supervisor_return
    csrw sepc, t0
    sret

machine_trap:
    j    fail

fail:
    li   t0, 'X'
    sb   t0, 0(s0)
    li   a7, 93
    li   a0, 1
    ecall
