# sv39_mprv.S - 验证 M-mode 下 mstatus.MPRV 对 Sv39 数据访存的影响

.section .text
.global _start
_start:
    li   s0, 0x10000000   # UART_BASE
    li   s2, '?'

    # Seed physical memory used by the bypass path.
    li   t0, 0x80002000
    li   t1, 0x11112222
    sw   t1, 0(t0)

    li   t0, 0x80003000
    li   t1, 0x33334444
    sw   t1, 0(t0)

    # Seed translated backing pages.
    li   t0, 0x80103000
    li   t1, 0x55667788
    sw   t1, 0(t0)

    li   t0, 0x80104000
    li   t1, 0x99AABBCC
    sw   t1, 0(t0)

    # Root page table at 0x80100000
    li   t0, 0x80100000

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

    # Level-0[2]: user page at 0x80002000 -> 0x80103000 (U=1, R=1, W=1)
    li   t0, 0x80102000
    li   t1, 0x80103
    slli t1, t1, 10
    ori  t1, t1, 0x17
    li   t2, 16
    add  t3, t0, t2
    sd   t1, 0(t3)

    # Level-0[3]: supervisor-only page at 0x80003000 -> 0x80104000 (R=1, W=1)
    li   t1, 0x80104
    slli t1, t1, 10
    ori  t1, t1, 0x7
    li   t2, 24
    add  t3, t0, t2
    sd   t1, 0(t3)

    la   t0, machine_trap
    csrw mtvec, t0

    # satp.MODE=Sv39, root PPN = 0x80100
    li   t0, 8
    slli t0, t0, 60
    li   t1, 0x80100
    or   t0, t0, t1
    csrw satp, t0
    sfence.vma

    # MPRV=0 baseline: M-mode load should bypass translation and read the
    # physical value at 0x80002000 rather than the translated user page.
    li   s1, '0'
    li   t0, 0x80002000
    lw   t1, 0(t0)
    li   t2, 0x11112222
    bne  t1, t2, fail

    # MPRV=1, MPP=S: load from a user page should fault until SUM=1.
    li   s1, '1'
    li   t0, ((1 << 17) | (1 << 11))   # MPRV=1, MPP=S
    csrw mstatus, t0
    li   t0, 0x80002000
sum_fault_point:
    lw   t1, 0(t0)
    j    fail

after_sum_fault:
    li   s1, '2'
    li   t0, ((1 << 17) | (1 << 11) | (1 << 18))   # MPRV=1, MPP=S, SUM=1
    csrw mstatus, t0
    li   t0, 0x80002000
    li   s2, 'a'
    lw   t1, 0(t0)
    li   s2, 'b'
    li   t2, 0x55667788
    bne  t1, t2, fail

    li   s2, 'c'
    li   t2, 0x1EADBEEF
    sw   t2, 0(t0)
    li   s2, 'd'
    lw   t1, 0(t0)
    bne  t1, t2, fail

    li   t3, 0
    csrw mstatus, t3
    li   t3, 0x80103000
    li   s2, 'e'
    lw   t4, 0(t3)
    bne  t4, t2, fail

    # MPRV=1, MPP=U: supervisor-only page should fault for user accesses.
    li   s1, '3'
    li   t0, (1 << 17)   # MPRV=1, MPP=U
    csrw mstatus, t0
    li   t0, 0x80003000
user_fault_point:
    lw   t1, 0(t0)
    j    fail

after_user_fault:
    li   t0, 0
    csrw mstatus, t0

    li   t0, 'M'
    sb   t0, 0(s0)
    li   t0, 'P'
    sb   t0, 0(s0)
    li   t0, 'R'
    sb   t0, 0(s0)
    li   t0, 'V'
    sb   t0, 0(s0)

    li   a7, 93
    li   a0, 0
    ecall

machine_trap:
    csrr t0, mcause
    li   t2, 13
    bne  t0, t2, fail

    csrr t0, mepc
    la   t2, sum_fault_point
    beq  t0, t2, handle_sum_fault

    la   t2, user_fault_point
    beq  t0, t2, handle_user_fault

    j    fail

handle_sum_fault:
    csrr t1, mtval
    li   t2, 0x80002000
    bne  t1, t2, fail

    la   t0, after_sum_fault
    csrw mepc, t0
    mret

handle_user_fault:
    csrr t1, mtval
    li   t2, 0x80003000
    bne  t1, t2, fail

    la   t0, after_user_fault
    csrw mepc, t0
    mret

fail:
    li   t0, 'X'
    sb   s1, 0(s0)
    sb   s2, 0(s0)
    sb   t0, 0(s0)
    li   a7, 93
    li   a0, 1
    ecall
