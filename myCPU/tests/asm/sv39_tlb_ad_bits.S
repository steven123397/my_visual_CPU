# sv39_tlb_ad_bits.S - 验证 TLB 命中时仍会补写叶子 PTE 的 A/D 位

.section .text
.global _start
_start:
    li   s0, 0x10000000   # UART_BASE

    la   t0, machine_trap
    csrw mtvec, t0

    # Seed the backing page before enabling translation.
    li   t0, 0x80103000
    li   t1, 0x01020304
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

    li   t0, 0x80102000

    # Level-0[0]: code page at 0x80000000
    li   t1, 0x80000
    slli t1, t1, 10
    ori  t1, t1, 0xF
    sd   t1, 0(t0)

    # Level-0[1]: data page at 0x80001000 -> physical 0x80103000
    # Keep A/D clear initially so the hardware path must set them.
    li   t1, 0x80103
    slli t1, t1, 10
    ori  t1, t1, 0x7
    li   t2, 8
    add  t3, t0, t2
    sd   t1, 0(t3)

    # Identity-map the page-table pages so S-mode can inspect the leaf PTE.
    li   t4, 0x100
    li   t1, 0x80100
    slli t1, t1, 10
    ori  t1, t1, 0x7
    slli t2, t4, 3
    add  t3, t0, t2
    sd   t1, 0(t3)

    addi t4, t4, 1
    li   t1, 0x80101
    slli t1, t1, 10
    ori  t1, t1, 0x7
    slli t2, t4, 3
    add  t3, t0, t2
    sd   t1, 0(t3)

    addi t4, t4, 1
    li   t1, 0x80102
    slli t1, t1, 10
    ori  t1, t1, 0x7
    slli t2, t4, 3
    add  t3, t0, t2
    sd   t1, 0(t3)

    addi t4, t4, 1
    li   t1, 0x80103
    slli t1, t1, 10
    ori  t1, t1, 0x7
    slli t2, t4, 3
    add  t3, t0, t2
    sd   t1, 0(t3)

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
    li   t3, 0x80102008    # Leaf PTE backing 0x80001000
    ld   t1, 0(t3)
    li   t2, 0xC0
    and  t4, t1, t2
    bnez t4, fail

    li   t0, 0x80001000
    lw   t1, 0(t0)
    li   t2, 0x01020304
    bne  t1, t2, fail

    ld   t1, 0(t3)
    li   t2, 0x40
    and  t4, t1, t2
    beqz t4, fail
    li   t2, 0x80
    and  t4, t1, t2
    bnez t4, fail

    li   t1, 0x12345678
    sw   t1, 0(t0)

    ld   t2, 0(t3)
    li   t4, 0x80
    and  t2, t2, t4
    beqz t2, fail

    lw   t2, 0(t0)
    li   t4, 0x12345678
    bne  t2, t4, fail

    li   t0, 'T'
    sb   t0, 0(s0)
    li   t0, 'A'
    sb   t0, 0(s0)
    li   t0, 'D'
    sb   t0, 0(s0)

    li   a7, 93
    li   a0, 0
success_exit:
    ebreak

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
    sb   t0, 0(s0)
    li   a7, 93
    li   a0, 1
    ecall
