# sv39_pagewalk_contracts.S - 验证 superpage 对齐与 PTE reserved-bit 合同

.section .text
.global _start
_start:
    li   s0, 0x10000000   # UART_BASE
    li   s1, '0'
    li   s2, '?'

    # Seed the backing page that would be reachable if the reserved-bit
    # non-leaf entry were incorrectly accepted.
    li   t0, 0x80104000
    li   t1, 0x12345678
    sw   t1, 0(t0)

    # Seed the backing page that would be reachable if a leaf high reserved bit
    # were incorrectly accepted.
    li   t0, 0x80105000
    li   t1, 0x55667788
    sw   t1, 0(t0)

    # Seed the backing page that would be reachable if a non-leaf high reserved
    # bit were incorrectly accepted.
    li   t0, 0x80107000
    li   t1, 0x99aabbcc
    sw   t1, 0(t0)

    # Root page table at 0x80100000
    li   t0, 0x80100000

    # Root[0]: identity-map low 1GB so UART remains reachable in S-mode.
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

    li   t0, 0x80101000

    # Level-1[0] -> level-0 table at 0x80102000 for the code page.
    li   t1, 0x80102
    slli t1, t1, 10
    ori  t1, t1, 0x1
    sd   t1, 0(t0)

    # Level-1[1]: misaligned 2MiB superpage leaf at 0x80200000.
    li   t1, 0x80123
    slli t1, t1, 10
    ori  t1, t1, 0x7
    li   t2, 8
    add  t3, t0, t2
    sd   t1, 0(t3)

    # Level-1[2]: non-leaf pointer with U bit set at 0x80400000.
    li   t1, 0x80103
    slli t1, t1, 10
    ori  t1, t1, 0x11      # V=1 plus reserved U bit on a non-leaf entry
    li   t2, 16
    add  t3, t0, t2
    sd   t1, 0(t3)

    # Level-1[3] -> level-0 table at 0x80106000 for reserved-bit leaf tests.
    li   t1, 0x80106
    slli t1, t1, 10
    ori  t1, t1, 0x1
    li   t2, 24
    add  t3, t0, t2
    sd   t1, 0(t3)

    # Level-1[4]: non-leaf pointer with high reserved bit set at 0x80800000.
    li   t1, 0x80108
    slli t1, t1, 10
    ori  t1, t1, 0x1
    li   t2, 1
    slli t2, t2, 63
    or   t1, t1, t2
    li   t2, 32
    add  t3, t0, t2
    sd   t1, 0(t3)

    # Level-0[0] code page at 0x80000000.
    li   t0, 0x80102000
    li   t1, 0x80000
    slli t1, t1, 10
    ori  t1, t1, 0xF
    sd   t1, 0(t0)

    # Level-0[0] under Level-1[2]: leaf that must never become reachable.
    li   t0, 0x80103000
    li   t1, 0x80104
    slli t1, t1, 10
    ori  t1, t1, 0x7
    sd   t1, 0(t0)

    # Level-0[0] under Level-1[3]: leaf with high reserved bit set.
    li   t0, 0x80106000
    li   t1, 0x80105
    slli t1, t1, 10
    ori  t1, t1, 0x7
    li   t2, 1
    slli t2, t2, 63
    or   t1, t1, t2
    sd   t1, 0(t0)

    # Level-0[0] under Level-1[4]: leaf that must never become reachable.
    li   t0, 0x80108000
    li   t1, 0x80107
    slli t1, t1, 10
    ori  t1, t1, 0x7
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
    sfence.vma

    li   t0, 0x800         # MPP = S
    csrw mstatus, t0
    la   t0, supervisor_main
    csrw mepc, t0
    mret

supervisor_main:
    li   s1, '1'
    csrr s4, satp
    csrw satp, zero
    sfence.vma
    li   t0, 0x80101008
    ld   s3, 0(t0)
    csrw satp, s4
    sfence.vma

    li   t0, 0x80200000
misaligned_superpage_fault_point:
    lw   t1, 0(t0)
    j    fail

after_misaligned_superpage_fault:
    li   s1, '2'
    li   t0, 0x80400000
reserved_non_leaf_fault_point:
    lw   t1, 0(t0)
    j    fail

after_reserved_non_leaf_fault:
    li   s1, '3'
    li   t0, 0x80600000
reserved_leaf_high_fault_point:
    lw   t1, 0(t0)
    j    fail

after_reserved_leaf_high_fault:
    li   s1, '4'
    li   t0, 0x80800000
reserved_non_leaf_high_fault_point:
    lw   t1, 0(t0)
    j    fail

after_reserved_non_leaf_high_fault:
    li   t0, 'P'
    sb   t0, 0(s0)
    li   t0, 'W'
    sb   t0, 0(s0)
    li   t0, 'K'
    sb   t0, 0(s0)

    li   a7, 93
    li   a0, 0
success_exit:
    ebreak

supervisor_trap:
    li   s2, 'T'
    csrr t0, scause
    li   t2, 13
    bne  t0, t2, fail

    csrr t0, sepc
    la   t2, misaligned_superpage_fault_point
    beq  t0, t2, handle_misaligned_superpage_fault

    la   t2, reserved_non_leaf_fault_point
    beq  t0, t2, handle_reserved_non_leaf_fault

    la   t2, reserved_leaf_high_fault_point
    beq  t0, t2, handle_reserved_leaf_high_fault

    la   t2, reserved_non_leaf_high_fault_point
    beq  t0, t2, handle_reserved_non_leaf_high_fault

    j    fail

handle_misaligned_superpage_fault:
    li   s2, 'M'
    csrr t1, stval
    li   t2, 0x80200000
    bne  t1, t2, fail

    csrr t4, satp
    csrw satp, zero
    sfence.vma
    li   t0, 0x80101008
    ld   t5, 0(t0)
    csrw satp, t4
    sfence.vma
    bne  t5, s3, fail

    la   t0, after_misaligned_superpage_fault
    csrw sepc, t0
    sret

handle_reserved_non_leaf_fault:
    li   s2, 'R'
    csrr t1, stval
    li   t2, 0x80400000
    bne  t1, t2, fail

    la   t0, after_reserved_non_leaf_fault
    csrw sepc, t0
    sret

handle_reserved_leaf_high_fault:
    li   s2, 'L'
    csrr t1, stval
    li   t2, 0x80600000
    bne  t1, t2, fail

    la   t0, after_reserved_leaf_high_fault
    csrw sepc, t0
    sret

handle_reserved_non_leaf_high_fault:
    li   s2, 'H'
    csrr t1, stval
    li   t2, 0x80800000
    bne  t1, t2, fail

    la   t0, after_reserved_non_leaf_high_fault
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
    li   t0, 'X'
    sb   s1, 0(s0)
    sb   s2, 0(s0)
    sb   t0, 0(s0)
    li   a7, 93
    li   a0, 1
    ecall
