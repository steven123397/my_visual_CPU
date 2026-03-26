# csr_illegal_matrix.S - 验证 CSR 非法访问在不同指令形式与特权级上的 trap 合同
.section .text
.global _start
_start:
    li   s0, 0x10000000   # UART_BASE

    la   t0, trap_handler
    csrw mtvec, t0

    csrr s1, misa
    li   t0, 0
    csrw mscratch, t0

machine_csrrwi_time:
    csrrwi x0, time, 0
    j    fail

user_write_sstatus:
    csrrw x0, sstatus, zero
    j    fail

user_read_mie:
    csrr t0, mie
    j    fail

supervisor_write_mscratch:
    csrrw x0, mscratch, zero
    j    fail

supervisor_read_mepc:
    csrr t0, mepc
    j    fail

machine_prepare_csrrs_cycle:
    li   t0, 1
machine_csrrs_cycle:
    csrrs x0, cycle, t0
    j    fail

machine_prepare_csrrc_instret:
    li   t0, 1
machine_csrrc_instret:
    csrrc x0, instret, t0
    j    fail

machine_csrrci_misa:
    csrrci x0, misa, 1
    j    fail

after_tests:
    li   t0, 'C'
    sb   t0, 0(s0)
    li   t0, 'R'
    sb   t0, 0(s0)
    li   t0, 'M'
    sb   t0, 0(s0)

    li   a7, 93
    li   a0, 0
    ecall

trap_handler:
    csrr t0, mscratch
    beqz t0, handle_machine_csrrwi_time

    li   t1, 1
    beq  t0, t1, handle_user_write_sstatus

    li   t1, 2
    beq  t0, t1, handle_user_read_mie

    li   t1, 3
    beq  t0, t1, handle_supervisor_write_mscratch

    li   t1, 4
    beq  t0, t1, handle_supervisor_read_mepc

    li   t1, 5
    beq  t0, t1, handle_machine_csrrs_cycle

    li   t1, 6
    beq  t0, t1, handle_machine_csrrc_instret

    li   t1, 7
    beq  t0, t1, handle_machine_csrrci_misa

    j    fail

handle_machine_csrrwi_time:
    csrr t0, mcause
    li   t1, 2
    bne  t0, t1, fail

    csrr t0, mepc
    la   t1, machine_csrrwi_time
    bne  t0, t1, fail

    csrr t0, mstatus
    li   t1, 0x1800
    and  t2, t0, t1
    bne  t2, t1, fail

    li   t0, 1
    csrw mscratch, t0

    li   t0, 0            # MPP = U
    csrw mstatus, t0
    la   t0, user_write_sstatus
    csrw mepc, t0
    mret

handle_user_write_sstatus:
    csrr t0, mcause
    li   t1, 2
    bne  t0, t1, fail

    csrr t0, mepc
    la   t1, user_write_sstatus
    bne  t0, t1, fail

    csrr t0, mstatus
    li   t1, 0x1800
    and  t2, t0, t1
    bnez t2, fail

    li   t0, 2
    csrw mscratch, t0

    li   t0, 0            # MPP = U
    csrw mstatus, t0
    la   t0, user_read_mie
    csrw mepc, t0
    mret

handle_user_read_mie:
    csrr t0, mcause
    li   t1, 2
    bne  t0, t1, fail

    csrr t0, mepc
    la   t1, user_read_mie
    bne  t0, t1, fail

    csrr t0, mstatus
    li   t1, 0x1800
    and  t2, t0, t1
    bnez t2, fail

    li   t0, 3
    csrw mscratch, t0

    li   t0, 0x800        # MPP = S
    csrw mstatus, t0
    la   t0, supervisor_write_mscratch
    csrw mepc, t0
    mret

handle_supervisor_write_mscratch:
    csrr t0, mcause
    li   t1, 2
    bne  t0, t1, fail

    csrr t0, mepc
    la   t1, supervisor_write_mscratch
    bne  t0, t1, fail

    csrr t0, mstatus
    li   t1, 0x1800
    and  t2, t0, t1
    li   t3, 0x800
    bne  t2, t3, fail

    li   t0, 4
    csrw mscratch, t0

    li   t0, 0x800        # MPP = S
    csrw mstatus, t0
    la   t0, supervisor_read_mepc
    csrw mepc, t0
    mret

handle_supervisor_read_mepc:
    csrr t0, mcause
    li   t1, 2
    bne  t0, t1, fail

    csrr t0, mepc
    la   t1, supervisor_read_mepc
    bne  t0, t1, fail

    csrr t0, mstatus
    li   t1, 0x1800
    and  t2, t0, t1
    li   t3, 0x800
    bne  t2, t3, fail

    li   t0, 5
    csrw mscratch, t0

    li   t0, 0x1800       # MPP = M
    csrw mstatus, t0
    la   t0, machine_prepare_csrrs_cycle
    csrw mepc, t0
    mret

handle_machine_csrrs_cycle:
    csrr t0, mcause
    li   t1, 2
    bne  t0, t1, fail

    csrr t0, mepc
    la   t1, machine_csrrs_cycle
    bne  t0, t1, fail

    csrr t0, mstatus
    li   t1, 0x1800
    and  t2, t0, t1
    bne  t2, t1, fail

    li   t0, 6
    csrw mscratch, t0

    li   t0, 0x1800       # MPP = M
    csrw mstatus, t0
    la   t0, machine_prepare_csrrc_instret
    csrw mepc, t0
    mret

handle_machine_csrrc_instret:
    csrr t0, mcause
    li   t1, 2
    bne  t0, t1, fail

    csrr t0, mepc
    la   t1, machine_csrrc_instret
    bne  t0, t1, fail

    csrr t0, mstatus
    li   t1, 0x1800
    and  t2, t0, t1
    bne  t2, t1, fail

    li   t0, 7
    csrw mscratch, t0

    li   t0, 0x1800       # MPP = M
    csrw mstatus, t0
    la   t0, machine_csrrci_misa
    csrw mepc, t0
    mret

handle_machine_csrrci_misa:
    csrr t0, mcause
    li   t1, 2
    bne  t0, t1, fail

    csrr t0, mepc
    la   t1, machine_csrrci_misa
    bne  t0, t1, fail

    csrr t0, mstatus
    li   t1, 0x1800
    and  t2, t0, t1
    bne  t2, t1, fail

    csrr t0, misa
    bne  t0, s1, fail

    li   t0, 0x1800       # MPP = M
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
