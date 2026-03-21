# branches_signed_unsigned.S - 验证 signed/unsigned 分支语义差异
.section .text
.global _start
_start:
    li   s0, 0x10000000   # UART_BASE

    li   t0, -1
    li   t1, 1

    blt  t0, t1, blt_ok
    j    fail

blt_ok:
    bge  t0, t1, fail
    bltu t0, t1, fail
    bgeu t0, t1, bgeu_ok
    j    fail

bgeu_ok:
    li   t0, 1
    li   t1, -1

    bltu t0, t1, bltu_ok
    j    fail

bltu_ok:
    bge  t0, t1, bge_ok
    j    fail

bge_ok:
    bgeu t0, t1, fail

    li   t0, 'B'
    sb   t0, 0(s0)
    li   t0, 'R'
    sb   t0, 0(s0)
    li   t0, 'U'
    sb   t0, 0(s0)

    li   a7, 93
    li   a0, 0
    ecall

fail:
    li   t0, 'X'
    sb   t0, 0(s0)
    li   a7, 93
    li   a0, 1
    ecall
