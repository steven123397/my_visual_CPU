# sv39_exec_privilege.S - 验证 Sv39 指令取值仍受 U/S execute 边界约束

.section .text
.global _start
_start:
    li   s0, 0x10000000   # UART_BASE
    li   s1, '0'
    li   s2, 'a'

    # Root page table at 0x80100000
    li   t0, 0x80100000

    # Root[0]: identity-map low 1GB so UART remains reachable in S-mode.
    li   t1, 0x00000
    slli t1, t1, 10
    ori  t1, t1, 0xF
    sd   t1, 0(t0)

    # Root[2] -> level-1 table at 0x80101000.
    li   t1, 0x80101
    slli t1, t1, 10
    ori  t1, t1, 0x1
    li   t2, 2
    slli t2, t2, 3
    add  t3, t0, t2
    sd   t1, 0(t3)

    # Level-1[0] -> level-0 table at 0x80102000.
    li   t0, 0x80101000
    li   t1, 0x80102
    slli t1, t1, 10
    ori  t1, t1, 0x1
    sd   t1, 0(t0)

    li   t0, 0x80102000

    # Level-0[0]: supervisor executable code page at 0x80000000.
    li   t1, 0x80000
    slli t1, t1, 10
    ori  t1, t1, 0xF
    sd   t1, 0(t0)

    # Level-0[1]: user executable page at 0x80001000.
    li   t1, 0x80001
    slli t1, t1, 10
    ori  t1, t1, 0x19     # V=1, X=1, U=1
    li   t2, 8
    add  t3, t0, t2
    sd   t1, 0(t3)

    # Level-0[2]: supervisor-only data page at 0x80002000.
    li   t1, 0x80103
    slli t1, t1, 10
    ori  t1, t1, 0x7
    li   t2, 16
    add  t3, t0, t2
    sd   t1, 0(t3)

    la   t0, machine_trap
    csrw mtvec, t0

    la   t0, supervisor_trap
    csrw stvec, t0

    # Delegate user-ecall and page faults to S-mode.
    li   t0, (1 << 8) | (1 << 12) | (1 << 13) | (1 << 15)
    csrw medeleg, t0

    # satp.MODE=Sv39, root PPN = 0x80100
    li   t0, 8
    slli t0, t0, 60
    li   t1, 0x80100
    or   t0, t0, t1
    csrw satp, t0
    sfence.vma

    li   t0, 0x800         # MPP = S
    csrw mstatus, t0
    la   t0, supervisor_main
    csrw mepc, t0
    mret

supervisor_main:
    li   s1, '1'
    li   s2, 'b'
    li   t0, (1 << 18)     # SSTATUS.SUM = 1
    csrrs x0, sstatus, t0

    la   t0, user_exec_entry
supervisor_fault_jump:
    jalr x0, t0, 0
    j    fail

after_supervisor_fault:
    li   s1, '2'
    li   s2, 'c'
    la   t0, user_exec_entry
    csrw sepc, t0
    sret

supervisor_success:
    li   t0, 'X'
    sb   t0, 0(s0)
    li   t0, 'P'
    sb   t0, 0(s0)
    li   t0, 'V'
    sb   t0, 0(s0)

    li   a7, 93
    li   a0, 0
success_exit:
    ebreak

supervisor_exec_target:
    j    fail

supervisor_trap:
    csrr t0, scause
    li   t2, 12
    beq  t0, t2, handle_instruction_page_fault
    li   t2, 13
    beq  t0, t2, handle_load_page_fault
    li   t2, 15
    beq  t0, t2, handle_store_page_fault
    li   t2, 8
    beq  t0, t2, handle_user_ecall
    j    fail

handle_instruction_page_fault:
    csrr t0, sepc
    la   t2, user_exec_entry
    beq  t0, t2, handle_supervisor_fault
    la   t2, supervisor_exec_target
    beq  t0, t2, handle_user_exec_fault
    j    fail

handle_supervisor_fault:
    li   s1, 'S'
    li   s2, 'd'
    csrr t1, stval
    la   t2, user_exec_entry
    bne  t1, t2, fail

    la   t0, after_supervisor_fault
    csrw sepc, t0
    sret

handle_user_exec_fault:
    li   s1, 'U'
    li   s2, 'e'
    csrr t1, stval
    la   t2, supervisor_exec_target
    bne  t1, t2, fail

    la   t0, user_after_exec_fault
    csrw sepc, t0
    sret

handle_load_page_fault:
    csrr t0, sepc
    la   t2, user_load_fault_point
    bne  t0, t2, fail

    li   s1, 'L'
    li   s2, 'i'
    csrr t1, stval
    li   t2, 0x80002000
    bne  t1, t2, fail

    la   t0, after_user_load_fault
    csrw sepc, t0
    sret

handle_store_page_fault:
    csrr t0, sepc
    la   t2, user_store_fault_point
    bne  t0, t2, fail

    li   s1, 'S'
    li   s2, 'j'
    csrr t1, stval
    li   t2, 0x80002000
    bne  t1, t2, fail

    la   t0, after_user_store_fault
    csrw sepc, t0
    sret

handle_user_ecall:
    li   s2, 'k'
    csrr t0, sepc
    la   t2, user_ecall_point
    bne  t0, t2, fail
    li   t2, 'U'
    bne  s1, t2, fail

    csrr t3, sstatus
    li   t4, 0x100        # SPP = 1, return to S-mode.
    or   t3, t3, t4
    csrw sstatus, t3

    la   t0, supervisor_success
    csrw sepc, t0
    sret

machine_trap:
    csrr t0, mcause
    li   t1, 3
    bne  t0, t1, fail

    csrr t0, mepc
    la   t1, success_exit
    bne  t0, t1, fail

    li   a7, 93
    li   a0, 0
    ecall

fail:
    sb   s1, 0(s0)
    sb   s2, 0(s0)
    li   t0, 'X'
    sb   t0, 0(s0)
    li   a7, 93
    li   a0, 1
    ecall

.org 0x1000
user_exec_entry:
    li   s1, 'u'
    li   s2, 'g'
    la   t0, supervisor_exec_target
user_fault_jump:
    jalr x0, t0, 0
    j    fail

user_after_exec_fault:
    li   s1, 'l'
    li   s2, 'f'
    li   t0, 0x80002000
user_load_fault_point:
    lw   t1, 0(t0)
    j    fail

after_user_load_fault:
    li   s1, 's'
    li   s2, 'g'
    li   t0, 0x80002000
    li   t1, 0x12345678
user_store_fault_point:
    sw   t1, 0(t0)
    j    fail

after_user_store_fault:
    li   s1, 'U'
    li   s2, 'h'
    li   a7, 1
user_ecall_point:
    ecall
    j    fail
