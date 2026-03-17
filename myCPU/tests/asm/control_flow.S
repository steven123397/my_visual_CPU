# control_flow.S - 验证 beq/jal/jalr/backward branch 的控制流语义
.section .text
.global _start
_start:
    li   s0, 0x10000000   # UART_BASE

    li   t0, 0
    beqz t0, start_tests
    li   t1, 'X'
    sb   t1, 0(s0)
    j    fail

start_tests:
    jal  ra, emit_o
    jal  ra, emit_k

    li   a7, 93
    li   a0, 0
    ecall

emit_o:
    li   t1, 'O'
    sb   t1, 0(s0)
    jalr x0, ra, 0

emit_k:
    li   t0, 0
    li   t1, 2
loop:
    addi t0, t0, 1
    blt  t0, t1, loop
    li   t2, 'K'
    sb   t2, 0(s0)
    jalr x0, ra, 0

fail:
    li   a7, 93
    li   a0, 1
    ecall
