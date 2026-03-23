# csr_semantic_consistency.S - 验证 satp WARL 与 misa 只读声明保持一致
.section .text
.global _start
_start:
    li   s0, 0x10000000   # UART_BASE

    la   t0, trap_handler
    csrw mtvec, t0

    csrr s1, misa

    # Unsupported satp.MODE values must not be reported back as active modes.
    li   t0, 9
    slli t0, t0, 60
    li   t1, 0x12345
    or   t0, t0, t1
    csrw satp, t0
    csrr t1, satp
    bnez t1, fail

    li   t0, 1
    csrw mscratch, t0

write_misa:
    csrw misa, zero
    j    fail

after_tests:
    li   t0, 'C'
    sb   t0, 0(s0)
    li   t0, 'S'
    sb   t0, 0(s0)
    li   t0, 'C'
    sb   t0, 0(s0)

    li   a7, 93
    li   a0, 0
    ecall

trap_handler:
    csrr t0, mscratch
    li   t1, 1
    bne  t0, t1, fail

    csrr t0, mcause
    li   t1, 2
    bne  t0, t1, fail

    csrr t0, mepc
    la   t1, write_misa
    bne  t0, t1, fail

    csrr t0, misa
    bne  t0, s1, fail

    la   t0, after_tests
    csrw mepc, t0
    mret

fail:
    li   t0, 'X'
    sb   t0, 0(s0)
    li   a7, 93
    li   a0, 1
    ecall
