# sv39_tlb_flush.S - 验证 Sv39 TLB 命中保持旧映射并由 sfence.vma 刷新

.section .text
.global _start
_start:
    li   s0, 0x10000000   # UART_BASE

    # Seed two physical backing pages with different values.
    li   t0, 0x80103000
    li   t1, 0x11111111
    sw   t1, 0(t0)

    li   t0, 0x80104000
    li   t1, 0x22222222
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

    # Level-0[1]: test page at 0x80001000 -> physical 0x80103000
    li   t1, 0x80103
    slli t1, t1, 10
    ori  t1, t1, 0x7
    li   t2, 8
    add  t3, t0, t2
    sd   t1, 0(t3)

    # Map the page-table and backing-data pages identity so S-mode can update them.
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

    addi t4, t4, 1
    li   t1, 0x80104
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
    li   t0, 0x80001000
    lw   t1, 0(t0)
    li   t2, 0x11111111
    bne  t1, t2, fail

    # Rewrite the level-0 leaf PTE to point at the second backing page.
    li   t0, 0x80102008
    li   t1, 0x80104
    slli t1, t1, 10
    ori  t1, t1, 0x7
    sd   t1, 0(t0)

    # Without sfence.vma the stale cached translation should still read page 1.
    li   t0, 0x80001000
    lw   t1, 0(t0)
    li   t2, 0x11111111
    bne  t1, t2, fail

    sfence.vma

    # After sfence.vma the new page-table mapping must be observed.
    lw   t1, 0(t0)
    li   t2, 0x22222222
    bne  t1, t2, fail

    li   t0, 'T'
    sb   t0, 0(s0)
    li   t0, 'L'
    sb   t0, 0(s0)
    li   t0, 'B'
    sb   t0, 0(s0)

    li   a7, 93
    li   a0, 0
    ecall

fail:
    li   t0, 'X'
    sb   t0, 0(s0)
    li   a7, 93
    li   a0, 1
    ecall
