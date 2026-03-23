# clint_split_access.S - 验证 CLINT 分段访问与 CSR time/mtime 一致性
#include "../../include/platform_mmio.h"
.section .text
.global _start
_start:
    li   s0, UART_BASE
    li   s1, CLINT_BASE

    li   t0, CLINT_REG_MTIME
    add  t1, s1, t0

    # Write mtime via 32-bit halves. Write high first so the ticking low half
    # cannot perturb the final programmed value before the low-half store lands.
    li   t0, 1
    sw   t0, 4(t1)
    li   t0, 0x89ABCDEF
    sw   t0, 0(t1)

    # The next instruction observes one tick after the final store.
    ld   t2, 0(t1)
    csrr t4, time
    sub  t5, t4, t2
    li   t6, 1
    bne  t5, t6, fail

    li   t3, 0x0000000189ABCDF0
    bne  t2, t3, fail

    li   t0, CLINT_REG_MTIMECMP
    add  t1, s1, t0
    li   t0, 2
    sw   t0, 4(t1)
    li   t0, 0x76543210
    sw   t0, 0(t1)

    ld   t2, 0(t1)
    li   t3, 0x0000000276543210
    bne  t2, t3, fail

    lw   t4, 4(t1)
    li   t5, 2
    bne  t4, t5, fail

    li   t0, 'C'
    sb   t0, 0(s0)
    li   t0, 'L'
    sb   t0, 0(s0)
    li   t0, 'T'
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
