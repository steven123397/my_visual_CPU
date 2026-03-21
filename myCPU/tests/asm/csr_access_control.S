# csr_access_control.S - 验证 CSR 特权级和只读属性的非法访问行为
.section .text
.global _start
_start:
    li   s0, 0x10000000   # UART_BASE

    li   t0, 0
    csrw mscratch, t0

    la   t0, trap_handler
    csrw mtvec, t0

    li   t0, 0            # MPP = U
    csrw mstatus, t0
    la   t0, user_read_sstatus
    csrw mepc, t0
    mret

user_read_sstatus:
    csrr t0, sstatus
    j    fail

supervisor_read_mstatus:
    csrr t0, mstatus
    j    fail

machine_write_cycle:
    csrw cycle, zero
    j    fail

after_tests:
    li   t0, 'C'
    sb   t0, 0(s0)
    li   t0, 'S'
    sb   t0, 0(s0)
    li   t0, 'R'
    sb   t0, 0(s0)

    li   a7, 93
    li   a0, 0
    ecall

trap_handler:
    csrr t0, mscratch
    beqz t0, handle_user_illegal

    li   t1, 1
    beq  t0, t1, handle_supervisor_illegal

    li   t1, 2
    beq  t0, t1, handle_read_only_illegal

    j    fail

handle_user_illegal:
    csrr t0, mcause
    li   t1, 2
    bne  t0, t1, fail

    csrr t0, mepc
    la   t1, user_read_sstatus
    bne  t0, t1, fail

    csrr t0, mstatus
    li   t1, 0x1800
    and  t2, t0, t1
    bnez t2, fail

    li   t0, 1
    csrw mscratch, t0

    li   t0, 0x800        # MPP = S
    csrw mstatus, t0
    la   t0, supervisor_read_mstatus
    csrw mepc, t0
    mret

handle_supervisor_illegal:
    csrr t0, mcause
    li   t1, 2
    bne  t0, t1, fail

    csrr t0, mepc
    la   t1, supervisor_read_mstatus
    bne  t0, t1, fail

    csrr t0, mstatus
    li   t1, 0x1800
    and  t2, t0, t1
    li   t3, 0x800
    bne  t2, t3, fail

    li   t0, 2
    csrw mscratch, t0

    li   t0, 0x1800       # stay in M for read-only CSR write check
    csrw mstatus, t0
    la   t0, machine_write_cycle
    csrw mepc, t0
    mret

handle_read_only_illegal:
    csrr t0, mcause
    li   t1, 2
    bne  t0, t1, fail

    csrr t0, mepc
    la   t1, machine_write_cycle
    bne  t0, t1, fail

    csrr t0, mstatus
    li   t1, 0x1800
    and  t2, t0, t1
    bne  t2, t1, fail

    li   t0, 0x1800
    csrw mstatus, t0
    la   t0, after_tests
    csrw mepc, t0
    mret

fail:
    li   t0, 'X'
    sb   t0, 0(s0)
    li   a7, 93
    li   a0, 1
    ecall
