# plic_supervisor_external_interrupt.S - 验证 PLIC 驱动的 S-mode 外部中断递送与 claim/complete
.section .text
.global _start
_start:
    li   s0, 0x10000000   # UART_BASE
    li   s1, 0x0c000000   # PLIC_BASE

    la   t0, machine_trap
    csrw mtvec, t0

    la   t0, supervisor_trap
    csrw stvec, t0

    # Source 1 priority = 1
    li   t0, 1
    sw   t0, 4(s1)

    # Enable source 1 for supervisor context.
    li   t0, 2
    li   t1, 0x2080
    add  t1, s1, t1
    sw   t0, 0(t1)

    # Threshold = 0
    li   t1, 0x201000
    add  t1, s1, t1
    sw   zero, 0(t1)

    li   t0, 0x200
    csrw mideleg, t0
    csrw sie, t0

    li   t0, 0x802        # MPP = S, SIE = 1
    csrw mstatus, t0

    # Enable UART THRE interrupt to raise PLIC source 1.
    li   t0, 2
    sb   t0, 1(s0)

    la   t0, supervisor_wait_loop
    csrw mepc, t0
    mret

supervisor_wait_loop:
    j    supervisor_wait_loop

after_interrupt:
    li   t0, 'P'
    sb   t0, 0(s0)
    li   t0, 'S'
    sb   t0, 0(s0)
    li   t0, 'E'
    sb   t0, 0(s0)

    li   a7, 93
    li   a0, 0
    ecall

supervisor_trap:
    csrr t0, scause
    srli t1, t0, 63
    li   t2, 1
    bne  t1, t2, fail

    slli t1, t0, 1
    srli t1, t1, 1
    li   t2, 9
    bne  t1, t2, fail

    csrr t0, sepc
    la   t1, supervisor_wait_loop
    bne  t0, t1, fail

    li   t1, 0x201004
    add  t1, s1, t1
    lw   t2, 0(t1)
    li   t3, 1
    bne  t2, t3, fail

    li   t0, 0
    sb   t0, 1(s0)

    sw   t2, 0(t1)

    la   t0, after_interrupt
    csrw sepc, t0
    sret

machine_trap:
    j    fail

fail:
    li   t0, 'X'
    sb   t0, 0(s0)
    li   a7, 93
    li   a0, 1
    ecall
