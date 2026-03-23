# interrupt_csr_views.S - 验证 mie/mip/sie/sip 的可见位与可写位约束
.section .text
.global _start
_start:
    li   s0, 0x10000000   # UART_BASE

    li   t0, 0
    csrw mideleg, t0
    csrw mie, t0
    csrw mip, t0

    # Without mideleg, S-mode aliases must neither expose nor modify STIP.
    li   t0, 0x20
    csrw sie, t0
    csrr t1, mie
    bnez t1, fail
    csrr t1, sie
    bnez t1, fail

    csrw mie, t0
    csrr t1, sie
    bnez t1, fail

    csrw sip, t0
    csrr t1, mip
    bnez t1, fail

    csrw mip, t0
    csrr t1, sip
    bnez t1, fail

    # mideleg only exposes delegable supervisor interrupt bits.
    li   t0, -1
    csrw mideleg, t0
    csrr t1, mideleg
    li   t2, 0x222
    bne  t1, t2, fail

    # With STIP delegated, sie/sip become writable aliases for that bit.
    li   t0, 0
    csrw mie, t0
    li   t0, 0x20
    csrw sie, t0
    csrr t1, mie
    li   t2, 0x20
    bne  t1, t2, fail
    csrr t1, sie
    bne  t1, t2, fail

    li   t0, 0
    csrw mip, t0
    li   t0, 0x20
    csrw sip, t0
    csrr t1, mip
    li   t2, 0x20
    bne  t1, t2, fail
    csrr t1, sip
    bne  t1, t2, fail

    li   t0, 0x20
    csrrc x0, sip, t0
    csrr t1, mip
    bnez t1, fail

    li   t0, 'I'
    sb   t0, 0(s0)
    li   t0, 'C'
    sb   t0, 0(s0)
    li   t0, 'V'
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
