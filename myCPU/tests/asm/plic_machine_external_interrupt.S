# plic_machine_external_interrupt.S - 验证 PLIC 驱动的 M-mode 外部中断递送与 claim/complete
.section .text
.global _start
_start:
    li   s0, 0x10000000   # UART_BASE
    li   s1, 0x0c000000   # PLIC_BASE

    la   t0, machine_trap
    csrw mtvec, t0

    # Source 1 priority = 1
    li   t0, 1
    sw   t0, 4(s1)

    # Enable source 1 for machine context.
    li   t0, 2
    li   t1, 0x2000
    add  t1, s1, t1
    sw   t0, 0(t1)

    # Threshold = 0
    li   t1, 0x200000
    add  t1, s1, t1
    sw   zero, 0(t1)

    # Enable machine external interrupts globally.
    li   t0, 0x800
    csrw mie, t0
    li   t0, 8
    csrrs x0, mstatus, t0

    # Enable UART THRE interrupt to raise PLIC source 1.
    li   t0, 2
    sb   t0, 1(s0)

machine_wait_loop:
    j    machine_wait_loop

after_interrupt:
    li   t0, 'P'
    sb   t0, 0(s0)
    li   t0, 'M'
    sb   t0, 0(s0)
    li   t0, 'E'
    sb   t0, 0(s0)

    li   a7, 93
    li   a0, 0
    ecall

machine_trap:
    csrr t0, mcause
    srli t1, t0, 63
    li   t2, 1
    bne  t1, t2, fail

    slli t1, t0, 1
    srli t1, t1, 1
    li   t2, 11
    bne  t1, t2, fail

    csrr t0, mepc
    la   t1, machine_wait_loop
    bne  t0, t1, fail

    li   t1, 0x200004
    add  t1, s1, t1
    lw   t2, 0(t1)
    li   t3, 1
    bne  t2, t3, fail

    li   t0, 0
    sb   t0, 1(s0)

    sw   t2, 0(t1)

    la   t0, after_interrupt
    csrw mepc, t0
    mret

fail:
    li   t0, 'X'
    sb   t0, 0(s0)
    li   a7, 93
    li   a0, 1
    ecall
