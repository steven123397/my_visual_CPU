# muldiv.S - 验证 M 扩展乘除与除零边界语义
.section .text
.global _start
_start:
    li   s0, 0x10000000   # UART_BASE

    li   t0, 6
    li   t1, -7
    mul  t2, t0, t1
    li   t3, -42
    bne  t2, t3, fail

    li   t0, 0x8000000000000000
    li   t1, 2
    mulh t2, t0, t1
    li   t3, -1
    bne  t2, t3, fail

    li   t0, -1
    li   t1, 2
    mulhsu t2, t0, t1
    li   t3, -1
    bne  t2, t3, fail

    li   t0, -1
    li   t1, 2
    mulhu t2, t0, t1
    li   t3, 1
    bne  t2, t3, fail

    li   t0, -7
    li   t1, 3
    div  t2, t0, t1
    li   t3, -2
    bne  t2, t3, fail
    rem  t2, t0, t1
    li   t3, -1
    bne  t2, t3, fail

    li   t0, 7
    li   t1, 3
    divu t2, t0, t1
    li   t3, 2
    bne  t2, t3, fail
    remu t2, t0, t1
    li   t3, 1
    bne  t2, t3, fail

    li   t0, 5
    li   t1, 0
    div  t2, t0, t1
    li   t3, -1
    bne  t2, t3, fail
    divu t2, t0, t1
    li   t3, -1
    bne  t2, t3, fail
    rem  t2, t0, t1
    li   t3, 5
    bne  t2, t3, fail
    remu t2, t0, t1
    li   t3, 5
    bne  t2, t3, fail

    li   t0, 'M'
    sb   t0, 0(s0)
    li   t0, 'L'
    sb   t0, 0(s0)
    li   t0, 'D'
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
