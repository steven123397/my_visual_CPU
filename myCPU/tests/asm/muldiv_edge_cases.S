# muldiv_edge_cases.S - 验证 DIV/REM 与 DIVW/REMW 的 INT_MIN / -1 边界
.section .text
.global _start
_start:
    li   s0, 0x10000000   # UART_BASE

    li   t0, 1
    slli t0, t0, 63
    li   t1, -1

    div  t2, t0, t1
    bne  t2, t0, fail

    rem  t2, t0, t1
    bnez t2, fail

    lui  t0, 0x80000
    li   t1, -1

    divw t2, t0, t1
    lui  t3, 0x80000
    addiw t3, t3, 0
    bne  t2, t3, fail

    remw t2, t0, t1
    bnez t2, fail

    li   t0, 'M'
    sb   t0, 0(s0)
    li   t0, 'D'
    sb   t0, 0(s0)
    li   t0, 'E'
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
