# exception_traps.S - 验证 breakpoint / illegal instruction trap 与 halfword target 语义
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

reserved_system_point:
    .word 0x10400073

after_reserved_system:
    csrr t0, mscratch
    li   t1, 3
    bne  t0, t1, fail

halfword_jalr_point:
    la   t0, halfword_jalr_target
halfword_jalr_insn:
    jalr x0, 0(t0)
    j    fail

    .2byte 0
halfword_jalr_target:
    li   t0, 4
    csrw mscratch, t0
    j    after_halfword_jalr

after_halfword_jalr:
    csrr t0, mscratch
    li   t1, 4
    bne  t0, t1, fail

halfword_jal_point:
halfword_jal_insn:
    jal  x0, halfword_jal_target
    j    fail

    .2byte 0
halfword_jal_target:
    li   t0, 5
    csrw mscratch, t0
    j    after_halfword_jal

after_halfword_jal:
    csrr t0, mscratch
    li   t1, 5
    bne  t0, t1, fail

halfword_branch_point:
    li   t0, 1
    li   t1, 1
halfword_branch_insn:
    beq  t0, t1, halfword_branch_target
    j    fail

    .2byte 0
halfword_branch_target:
    li   t0, 6
    csrw mscratch, t0
    j    after_halfword_branch

after_halfword_branch:
    csrr t0, mscratch
    li   t1, 6
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

    .2byte 0
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
    beq  t0, t1, handle_illegal_insn

    la   t1, reserved_system_point
    beq  t0, t1, handle_reserved_system

    j    trap_fail

handle_illegal_insn:
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

handle_reserved_system:
    csrr t0, mtval
    li   t1, 0x10400073
    bne  t0, t1, trap_fail

    li   t0, 3
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
