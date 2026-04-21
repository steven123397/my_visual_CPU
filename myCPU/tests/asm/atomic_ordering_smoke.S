#define ENCODE_AMO(funct5, aq, rl, rs2, rs1, funct3, rd) \
    (((funct5) << 27) | ((aq) << 26) | ((rl) << 25) | ((rs2) << 20) | \
     ((rs1) << 15) | ((funct3) << 12) | ((rd) << 7) | 0x2f)

.section .text
.option norelax
.global _start
_start:
    li   s0, 0x10000000

    la   t0, trap_handler
    csrw mtvec, t0

    la   s1, slot_a
    la   s2, slot_b
    li   t0, 1
    sw   t0, 0(s1)
    li   t0, 2
    sw   t0, 0(s2)
    li   t1, 9
    li   t2, 4

    .word ENCODE_AMO(0x02, 0, 0, 0, 9, 0x2, 5)   # lr.w t0, (s1)
    .word ENCODE_AMO(0x02, 1, 0, 0, 18, 0x2, 28) # lr.w.aq t3, (s2)

    .word ENCODE_AMO(0x03, 0, 0, 6, 9, 0x2, 29)  # sc.w t4, t1, (s1)
    li   t5, 1
    bne  t4, t5, fail
    lw   t4, 0(s1)
    li   t6, 1
    bne  t4, t6, fail

    .word ENCODE_AMO(0x03, 0, 0, 6, 18, 0x2, 29) # sc.w t4, t1, (s2)
    li   t5, 1
    bne  t4, t5, fail
    lw   t4, 0(s2)
    li   t6, 2
    bne  t4, t6, fail

    .word ENCODE_AMO(0x02, 1, 0, 0, 18, 0x2, 28) # lr.w.aq t3, (s2)
    .word ENCODE_AMO(0x03, 0, 1, 6, 18, 0x2, 29) # sc.w.rl t4, t1, (s2)
    bnez t4, fail
    lw   t4, 0(s2)
    li   t6, 9
    bne  t4, t6, fail

    .word ENCODE_AMO(0x01, 1, 1, 7, 18, 0x2, 5)  # amoswap.w.aqrl t0, t2, (s2)
    li   t5, 9
    bne  t0, t5, fail
    lw   t4, 0(s2)
    li   t6, 4
    bne  t4, t6, fail

    li   t0, 'A'
    sb   t0, 0(s0)
    li   t0, 'O'
    sb   t0, 0(s0)
    li   t0, 'R'
    sb   t0, 0(s0)

    li   a7, 93
    li   a0, 0
    ecall

trap_handler:
    j    fail

fail:
    li   t0, 'X'
    sb   t0, 0(s0)
    li   a7, 93
    li   a0, 1
    ecall

.section .data
.align 2
slot_a:
    .word 0
slot_b:
    .word 0
