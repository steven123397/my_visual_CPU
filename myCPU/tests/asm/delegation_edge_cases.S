# delegation_edge_cases.S - 验证 medeleg/mideleg 的边界行为
.section .text
.global _start
_start:
    li   s0, 0x10000000   # UART_BASE

    li   t0, 0
    csrw mscratch, t0

    la   t0, machine_trap
    csrw mtvec, t0

    la   t0, supervisor_trap
    csrw stvec, t0

    # medeleg only exposes implemented, delegable synchronous exception bits.
    li   t0, -1
    csrw medeleg, t0
    csrr t1, medeleg
    li   t2, 0xB3AE
    bne  t1, t2, fail

    # M-mode ecall delegation attempts must be rejected.
    li   t0, 0x800
    csrw medeleg, t0
    csrr t1, medeleg
    bnez t1, fail

    # Even if illegal-instruction delegation is enabled, an M-mode illegal
    # instruction must still trap to M-mode.
    li   t0, 1
    slli t0, t0, 2
    csrw medeleg, t0

illegal_in_machine:
    .word 0xffffffff

after_machine_illegal:
    # mideleg must reject machine timer interrupt delegation attempts.
    li   t0, 0x80
    csrw mideleg, t0
    csrr t1, mideleg
    bnez t1, fail

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

machine_wait_loop:
    j    machine_wait_loop

after_machine_timer:
    li   t0, 'D'
    sb   t0, 0(s0)
    li   t0, 'E'
    sb   t0, 0(s0)
    li   t0, 'G'
    sb   t0, 0(s0)

    li   a7, 93
    li   a0, 0
    ecall

machine_trap:
    csrr t0, mscratch
    beqz t0, handle_machine_illegal

    li   t1, 1
    beq  t0, t1, handle_machine_timer

    j    fail

handle_machine_illegal:
    csrr t0, mcause
    li   t1, 2
    bne  t0, t1, fail

    csrr t0, mepc
    la   t1, illegal_in_machine
    bne  t0, t1, fail

    li   t0, 1
    csrw mscratch, t0
    la   t0, after_machine_illegal
    csrw mepc, t0
    mret

handle_machine_timer:
    csrr t0, mcause
    srli t1, t0, 63
    li   t2, 1
    bne  t1, t2, fail

    slli t1, t0, 1
    srli t1, t1, 1
    li   t2, 7
    bne  t1, t2, fail

    csrr t0, mepc
    la   t1, machine_wait_loop
    bne  t0, t1, fail

    li   t0, 0x02000000   # CLINT_BASE
    li   t1, 0x4000       # mtimecmp
    add  t2, t0, t1
    li   t3, -1
    sd   t3, 0(t2)

    li   t0, 0x80
    csrrc x0, mip, t0
    la   t0, after_machine_timer
    csrw mepc, t0
    mret

supervisor_trap:
    j    fail

fail:
    li   t0, 'X'
    sb   t0, 0(s0)
    li   a7, 93
    li   a0, 1
    ecall
