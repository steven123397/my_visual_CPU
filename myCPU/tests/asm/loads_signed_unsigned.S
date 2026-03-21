# loads_signed_unsigned.S - 验证带符号/无符号 load 语义
.section .text
.global _start
_start:
    li   s0, 0x10000000   # UART_BASE
    li   t0, 0x80001000

    li   t1, 0x80
    sb   t1, 0(t0)
    lb   t1, 0(t0)
    li   t2, -128
    bne  t1, t2, fail
    lbu  t1, 0(t0)
    li   t2, 0x80
    bne  t1, t2, fail

    li   t1, 0x8001
    sh   t1, 2(t0)
    lh   t1, 2(t0)
    li   t2, -32767
    bne  t1, t2, fail
    lhu  t1, 2(t0)
    li   t2, 0x8001
    bne  t1, t2, fail

    li   t1, 0x80000002
    sw   t1, 8(t0)
    lw   t1, 8(t0)
    li   t2, -2147483646
    bne  t1, t2, fail
    lwu  t1, 8(t0)
    li   t2, 0x80000002
    bne  t1, t2, fail

    li   t1, 0x1122334455667788
    sd   t1, 16(t0)
    ld   t1, 16(t0)
    li   t2, 0x1122334455667788
    bne  t1, t2, fail

    li   t0, 'L'
    sb   t0, 0(s0)
    li   t0, 'D'
    sb   t0, 0(s0)
    li   t0, 'S'
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
