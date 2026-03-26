# illegal_integer_encodings.S - 验证保留整数编码稳定触发 illegal instruction
.section .text
.global _start
_start:
    li   s0, 0x10000000   # UART_BASE

    li   t0, 0
    csrw mscratch, t0

    la   t0, trap_handler
    csrw mtvec, t0

illegal_shift_imm:
    .word 0x04111093

after_illegal_shift_imm:
    csrr t0, mscratch
    li   t1, 1
    bne  t0, t1, fail

illegal_shift_right_imm:
    .word 0x20115093

after_illegal_shift_right_imm:
    csrr t0, mscratch
    li   t1, 2
    bne  t0, t1, fail

illegal_shiftw_imm:
    .word 0x0211109b

after_illegal_shiftw_imm:
    csrr t0, mscratch
    li   t1, 3
    bne  t0, t1, fail

illegal_shiftw_right_imm:
    .word 0x2011509b

after_illegal_shiftw_right_imm:
    csrr t0, mscratch
    li   t1, 4
    bne  t0, t1, fail

illegal_rtype:
    .word 0x043100b3

after_illegal_rtype:
    csrr t0, mscratch
    li   t1, 5
    bne  t0, t1, fail

illegal_shift_reg:
    .word 0x403110b3

after_illegal_shift_reg:
    csrr t0, mscratch
    li   t1, 6
    bne  t0, t1, fail

illegal_rtypew:
    .word 0x043100bb

after_illegal_rtypew:
    csrr t0, mscratch
    li   t1, 7
    bne  t0, t1, fail

illegal_mulw_reserved:
    .word 0x023110bb

after_illegal_mulw_reserved:
    csrr t0, mscratch
    li   t1, 8
    bne  t0, t1, fail

    li   t0, 'I'
    sb   t0, 0(s0)
    li   t0, 'L'
    sb   t0, 0(s0)
    li   t0, 'L'
    sb   t0, 0(s0)

    li   a7, 93
    li   a0, 0
    ecall

trap_handler:
    csrr t0, mcause
    li   t1, 2
    bne  t0, t1, trap_fail

    csrr t0, mepc
    la   t1, illegal_shift_imm
    beq  t0, t1, handle_illegal_shift_imm
    la   t1, illegal_shift_right_imm
    beq  t0, t1, handle_illegal_shift_right_imm
    la   t1, illegal_shiftw_imm
    beq  t0, t1, handle_illegal_shiftw_imm
    la   t1, illegal_shiftw_right_imm
    beq  t0, t1, handle_illegal_shiftw_right_imm
    la   t1, illegal_rtype
    beq  t0, t1, handle_illegal_rtype
    la   t1, illegal_shift_reg
    beq  t0, t1, handle_illegal_shift_reg
    la   t1, illegal_rtypew
    beq  t0, t1, handle_illegal_rtypew
    la   t1, illegal_mulw_reserved
    beq  t0, t1, handle_illegal_mulw_reserved
    j    trap_fail

.macro HANDLE_ILLEGAL label, raw, stage
\label:
    csrr t0, mtval
    li   t1, \raw
    bne  t0, t1, trap_fail
    li   t0, \stage
    csrw mscratch, t0
    csrr t0, mepc
    addi t0, t0, 4
    csrw mepc, t0
    mret
.endm

HANDLE_ILLEGAL handle_illegal_shift_imm, 0x04111093, 1
HANDLE_ILLEGAL handle_illegal_shift_right_imm, 0x20115093, 2
HANDLE_ILLEGAL handle_illegal_shiftw_imm, 0x0211109b, 3
HANDLE_ILLEGAL handle_illegal_shiftw_right_imm, 0x2011509b, 4
HANDLE_ILLEGAL handle_illegal_rtype, 0x043100b3, 5
HANDLE_ILLEGAL handle_illegal_shift_reg, 0x403110b3, 6
HANDLE_ILLEGAL handle_illegal_rtypew, 0x043100bb, 7
HANDLE_ILLEGAL handle_illegal_mulw_reserved, 0x023110bb, 8

trap_fail:
    j    fail

fail:
    li   t0, 'X'
    sb   t0, 0(s0)
    li   a7, 93
    li   a0, 1
    ecall
