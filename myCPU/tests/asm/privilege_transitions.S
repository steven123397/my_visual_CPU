# privilege_transitions.S - 验证 U/S/M privilege 过渡与 ecall cause / MPP 语义
.section .text
.global _start
_start:
    li   s0, 0x10000000   # UART_BASE

    li   t0, 0
    csrw mscratch, t0

    la   t0, trap_handler
    csrw mtvec, t0

    li   t0, 0
    csrw mstatus, t0      # MPP = U
    la   t0, user_ecall
    csrw mepc, t0
    mret

user_ecall:
    ecall

supervisor_ecall:
    ecall

machine_ecall:
    ecall

after_tests:
    li   t0, 'P'
    sb   t0, 0(s0)
    li   t0, 'R'
    sb   t0, 0(s0)
    li   t0, 'V'
    sb   t0, 0(s0)

    li   a7, 93
    li   a0, 0
    ecall

trap_handler:
    csrr t0, mscratch
    beqz t0, handle_user_ecall

    li   t1, 1
    beq  t0, t1, handle_supervisor_ecall

    li   t1, 2
    beq  t0, t1, handle_machine_ecall

    j    trap_fail

handle_user_ecall:
    csrr t0, mcause
    li   t1, 8
    bne  t0, t1, trap_fail

    csrr t0, mstatus
    li   t1, 0x1800
    and  t2, t0, t1
    bnez t2, trap_fail

    csrr t0, mepc
    la   t1, user_ecall
    bne  t0, t1, trap_fail

    li   t0, 1
    csrw mscratch, t0

    li   t0, 0x800       # MPP = S
    csrw mstatus, t0
    la   t0, supervisor_ecall
    csrw mepc, t0
    mret

handle_supervisor_ecall:
    csrr t0, mcause
    li   t1, 9
    bne  t0, t1, trap_fail

    csrr t0, mstatus
    li   t1, 0x1800
    and  t2, t0, t1
    li   t3, 0x800
    bne  t2, t3, trap_fail

    csrr t0, mepc
    la   t1, supervisor_ecall
    bne  t0, t1, trap_fail

    li   t0, 2
    csrw mscratch, t0

    li   t0, 0x1800      # MPP = M
    csrw mstatus, t0
    la   t0, machine_ecall
    csrw mepc, t0
    mret

handle_machine_ecall:
    csrr t0, mcause
    li   t1, 11
    bne  t0, t1, trap_fail

    csrr t0, mstatus
    li   t1, 0x1800
    and  t2, t0, t1
    bne  t2, t1, trap_fail

    csrr t0, mepc
    la   t1, machine_ecall
    bne  t0, t1, trap_fail

    li   t0, 0x1800      # stay in M when returning to test epilogue
    csrw mstatus, t0
    la   t0, after_tests
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
