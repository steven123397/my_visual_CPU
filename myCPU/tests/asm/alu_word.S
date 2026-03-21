# alu_word.S - 验证 W 类整数算术与移位语义
.section .text
.global _start
_start:
    li   s0, 0x10000000   # UART_BASE

    li   t0, -1
    addiw t1, t0, 1
    bnez t1, fail

    li   t0, 1
    slliw t1, t0, 31
    li   t2, -2147483648
    bne  t1, t2, fail

    li   t0, -1
    srliw t1, t0, 1
    li   t2, 0x7fffffff
    bne  t1, t2, fail

    li   t0, -4
    sraiw t1, t0, 1
    li   t2, -2
    bne  t1, t2, fail

    li   t0, 0x7fffffff
    li   t1, 1
    addw t2, t0, t1
    li   t3, -2147483648
    bne  t2, t3, fail

    li   t0, 0
    li   t1, 1
    subw t2, t0, t1
    li   t3, -1
    bne  t2, t3, fail

    li   t0, 1
    li   t1, 31
    sllw t2, t0, t1
    li   t3, -2147483648
    bne  t2, t3, fail

    li   t0, -1
    li   t1, 1
    srlw t2, t0, t1
    li   t3, 0x7fffffff
    bne  t2, t3, fail

    li   t0, -4
    li   t1, 1
    sraw t2, t0, t1
    li   t3, -2
    bne  t2, t3, fail

    li   t0, 'W'
    sb   t0, 0(s0)
    li   t0, '3'
    sb   t0, 0(s0)
    li   t0, '2'
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
