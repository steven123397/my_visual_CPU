# sv39_sum_mxr.S - 验证 sstatus.MXR / SUM 对 Sv39 权限检查的影响

.section .text
.global _start
_start:
    li   s0, 0x10000000   # UART_BASE

    # Seed backing pages through physical access before enabling translation.
    li   t0, 0x80103000
    li   t1, 0x11223344
    sw   t1, 0(t0)

    li   t0, 0x80104000
    li   t1, 0x55667788
    sw   t1, 0(t0)

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

    # Level-0[0]: executable/readable code page for the test itself
    li   t0, 0x80102000
    li   t1, 0x80000
    slli t1, t1, 10
    ori  t1, t1, 0xF
    sd   t1, 0(t0)

    # Level-0[1]: executable-only page, readable only when MXR=1
    li   t1, 0x80103
    slli t1, t1, 10
    ori  t1, t1, 0x9
    li   t2, 8
    add  t3, t0, t2
    sd   t1, 0(t3)

    # Level-0[2]: user data page, supervisor loads/stores need SUM=1
    li   t1, 0x80104
    slli t1, t1, 10
    ori  t1, t1, 0x17
    li   t2, 16
    add  t3, t0, t2
    sd   t1, 0(t3)

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
    sfence.vma

    li   t0, 0x800         # MPP = S
    csrw mstatus, t0
    la   t0, supervisor_main
    csrw mepc, t0
    mret

supervisor_main:
    li   s1, '0'
    li   s2, 'a'
    li   t0, 0x80001000
mxr_fault_point:
    lw   t1, 0(t0)
    j    fail

after_mxr_fault:
    li   s1, '1'
    li   s2, 'b'
    li   t0, 0x80001000
    li   t2, (1 << 19)     # SSTATUS.MXR
    csrrs x0, sstatus, t2
    csrr t3, sstatus
    and  t3, t3, t2
    beqz t3, fail

    li   s2, 'c'
    lw   t1, 0(t0)
    li   s2, 'd'
    li   t2, 0x11223344
    bne  t1, t2, fail

    li   s2, 'e'
    li   t2, (1 << 19)
    csrrc x0, sstatus, t2

    li   s1, '2'
    li   s2, 'f'
    li   t0, 0x80002000
sum_fault_point:
    lw   t1, 0(t0)
    j    fail

after_sum_fault:
    li   s1, '3'
    li   s2, 'g'
    li   t0, 0x80002000
    li   t2, (1 << 18)     # SSTATUS.SUM
    csrrs x0, sstatus, t2
    csrr t3, sstatus
    and  t3, t3, t2
    beqz t3, fail

    li   s2, 'h'
    lw   t1, 0(t0)
    li   s2, 'i'
    li   t2, 0x55667788
    bne  t1, t2, fail

    li   s2, 'j'
    li   t2, 0x1234ABCD
    sw   t2, 0(t0)
    lw   t1, 0(t0)
    bne  t1, t2, fail

    li   t0, 'S'
    sb   t0, 0(s0)
    li   t0, 'M'
    sb   t0, 0(s0)
    li   t0, 'X'
    sb   t0, 0(s0)

    li   a7, 93
    li   a0, 0
    ecall

supervisor_trap:
    csrr t0, scause
    li   t2, 13
    bne  t0, t2, fail

    csrr t0, sepc
    la   t2, mxr_fault_point
    beq  t0, t2, handle_mxr_fault

    la   t2, sum_fault_point
    beq  t0, t2, handle_sum_fault

    j    fail

handle_mxr_fault:
    csrr t1, stval
    li   t2, 0x80001000
    bne  t1, t2, fail

    la   t0, after_mxr_fault
    csrw sepc, t0
    sret

handle_sum_fault:
    csrr t1, stval
    li   t2, 0x80002000
    bne  t1, t2, fail

    la   t0, after_sum_fault
    csrw sepc, t0
    sret

machine_trap:
    j    fail

fail:
    sb   s1, 0(s0)
    sb   s2, 0(s0)
    li   t0, 'X'
    sb   t0, 0(s0)
    li   a7, 93
    li   a0, 1
    ecall
