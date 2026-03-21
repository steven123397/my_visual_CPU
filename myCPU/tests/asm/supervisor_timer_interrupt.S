# supervisor_timer_interrupt.S - 验证 mideleg 驱动的 S-mode 定时器中断递送与 sret 返回
.section .text
.global _start
_start:
    li   s0, 0x10000000   # UART_BASE

    la   t0, machine_trap
    csrw mtvec, t0

    la   t0, supervisor_vector_table
    ori  t0, t0, 1
    csrw stvec, t0

    li   t0, 0x02000000   # CLINT_BASE
    li   t1, 0xBFF8       # mtime
    add  t2, t0, t1
    ld   t3, 0(t2)
    addi t3, t3, 8
    li   t1, 0x4000       # mtimecmp
    add  t2, t0, t1
    sd   t3, 0(t2)

    li   t0, 0x20         # delegate + enable supervisor timer interrupt
    csrw mideleg, t0
    csrw sie, t0

    li   t0, 0x802        # MPP = S, SIE = 1
    csrw mstatus, t0
    la   t0, supervisor_wait_loop
    csrw mepc, t0
    mret

supervisor_wait_loop:
    j    supervisor_wait_loop

after_interrupt:
    csrr t0, sip
    li   t1, 0x20
    and  t0, t0, t1
    bnez t0, fail

    li   t0, 'S'
    sb   t0, 0(s0)
    li   t0, 'T'
    sb   t0, 0(s0)
    li   t0, 'I'
    sb   t0, 0(s0)

    li   a7, 93
    li   a0, 0
    ecall

.balign 32
supervisor_vector_table:
    j    unexpected_supervisor_trap
    j    unexpected_supervisor_trap
    j    unexpected_supervisor_trap
    j    unexpected_supervisor_trap
    j    unexpected_supervisor_trap
    j    supervisor_timer_handler

unexpected_supervisor_trap:
    j    fail

supervisor_timer_handler:
    csrr t0, scause
    srli t1, t0, 63
    li   t2, 1
    bne  t1, t2, fail

    slli t1, t0, 1
    srli t1, t1, 1
    li   t2, 5
    bne  t1, t2, fail

    csrr t0, sepc
    la   t1, supervisor_wait_loop
    bne  t0, t1, fail

    csrr t0, sstatus
    li   t1, 0x2
    and  t2, t0, t1
    bnez t2, fail

    li   t1, 0x20
    and  t2, t0, t1
    beqz t2, fail

    li   t1, 0x100
    and  t2, t0, t1
    beqz t2, fail

    li   t0, 0x02000000   # CLINT_BASE
    li   t1, 0x4000       # mtimecmp
    add  t2, t0, t1
    li   t3, -1
    sd   t3, 0(t2)

    li   t0, 0x20
    csrrc x0, sip, t0

    csrr t0, sepc
    addi t0, t0, 4
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
