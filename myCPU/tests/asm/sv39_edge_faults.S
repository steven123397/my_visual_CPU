# sv39_edge_faults.S - 验证 non-canonical 地址与跨页访问的 Sv39 fault 边界

.section .text
.global _start
_start:
    li   s0, 0x10000000   # UART_BASE

    # Root page table at 0x80100000
    li   t0, 0x80100000

    # Root[0]: identity-map low 1GB so UART remains reachable in S-mode
    li   t1, 0x00000
    slli t1, t1, 10
    ori  t1, t1, 0xF
    sd   t1, 0(t0)

    # Root[2] -> level-1 table at 0x80101000
    li   t1, 0x80101
    slli t1, t1, 10
    ori  t1, t1, 0x1
    li   t2, 2
    slli t2, t2, 3
    add  t3, t0, t2
    sd   t1, 0(t3)

    # Level-1[0] -> level-0 table at 0x80102000
    li   t0, 0x80101000
    li   t1, 0x80102
    slli t1, t1, 10
    ori  t1, t1, 0x1
    sd   t1, 0(t0)

    # Level-0[0] maps only virtual page 0x80000000-0x80000fff
    li   t0, 0x80102000
    li   t1, 0x80000
    slli t1, t1, 10
    ori  t1, t1, 0xF
    sd   t1, 0(t0)

    la   t0, machine_trap
    csrw mtvec, t0

    la   t0, supervisor_trap
    csrw stvec, t0

    li   t0, (1 << 13)
    csrw medeleg, t0

    # satp.MODE=Sv39, root PPN = 0x80100
    li   t0, 8
    slli t0, t0, 60
    li   t1, 0x80100
    or   t0, t0, t1
    csrw satp, t0
    .word 0x12008073       # sfence.vma x1, x0

    li   t0, 0x800         # MPP = S
    csrw mstatus, t0
    la   t0, supervisor_main
    csrw mepc, t0
    mret

supervisor_main:
    li   s1, 0
    li   s2, 'N'

    li   t0, 0x0000008080000000
canonical_fault_point:
    ld   t1, 0(t0)
    j    fail

after_canonical_fault:
    addi s1, s1, 1

    li   t0, 0x80000ffc
cross_page_fault_point:
    ld   t1, 0(t0)
    j    fail

after_cross_page_fault:
    addi s1, s1, 1

    li   t0, 2
    bne  s1, t0, fail

    li   t0, 'E'
    sb   t0, 0(s0)
    li   t0, 'D'
    sb   t0, 0(s0)
    li   t0, 'G'
    sb   t0, 0(s0)

    li   a7, 93
    li   a0, 0
    ecall

supervisor_trap:
    li   s2, 'T'
    csrr t0, scause
    li   t2, 13
    bne  t0, t2, fail

    csrr t0, sepc
    la   t2, canonical_fault_point
    beq  t0, t2, handle_canonical_fault

    la   t2, cross_page_fault_point
    beq  t0, t2, handle_cross_page_fault

    j    fail

handle_canonical_fault:
    li   s2, 'C'
    csrr t1, stval
    li   t2, 0x0000008080000000
    bne  t1, t2, fail

    la   t0, after_canonical_fault
    csrw sepc, t0
    sret

handle_cross_page_fault:
    li   s2, 'P'
    csrr t1, stval
    li   t2, 0x80001000
    bne  t1, t2, fail

    la   t0, after_cross_page_fault
    csrw sepc, t0
    sret

machine_trap:
    j    fail

fail:
    addi t1, s1, '0'
    sb   t1, 0(s0)
    sb   s2, 0(s0)
    li   t0, 'X'
    sb   t0, 0(s0)
    li   a7, 93
    li   a0, 1
    ecall
