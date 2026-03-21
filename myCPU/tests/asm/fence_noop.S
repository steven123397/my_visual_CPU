# fence_noop.S - 固定当前 FENCE/FENCE.I 作为 no-op 的参考行为
.section .text
.global _start
_start:
    li   s0, 0x10000000   # UART_BASE

    li   t0, 1
    fence
    addi t0, t0, 1
    .word 0x0000100f      # fence.i
    addi t0, t0, 1

    li   t1, 3
    bne  t0, t1, fail

    li   t0, 'F'
    sb   t0, 0(s0)
    li   t0, 'N'
    sb   t0, 0(s0)
    li   t0, 'C'
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
