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

    csrr t0, misa
    andi t0, t0, 1
    beqz t0, fail

    csrr t0, mhartid
    bnez t0, fail

    .word 0x10500073       # wfi

    la   s1, lrsc_slot
    li   t0, -1
    sw   t0, 0(s1)
    li   t1, 0x12345678

    .word ENCODE_AMO(0x02, 0, 0, 0, 9, 0x2, 5)   # lr.w t0, (s1)
    li   t2, -1
    bne  t0, t2, fail

    .word ENCODE_AMO(0x03, 0, 0, 6, 9, 0x2, 7)   # sc.w t2, t1, (s1)
    bnez t2, fail

    lw   t3, 0(s1)
    li   t4, 0x12345678
    bne  t3, t4, fail

    la   s2, amoadd_slot
    li   t0, 7
    sw   t0, 0(s2)
    li   t1, 5

    .word ENCODE_AMO(0x00, 0, 0, 6, 18, 0x2, 5)  # amoadd.w t0, t1, (s2)
    li   t2, 7
    bne  t0, t2, fail

    lw   t3, 0(s2)
    li   t4, 12
    bne  t3, t4, fail

    la   s3, amomaxu_slot
    li   t0, 5
    sd   t0, 0(s3)
    li   t1, 8

    .word ENCODE_AMO(0x1c, 0, 0, 6, 19, 0x3, 5)  # amomaxu.d t0, t1, (s3)
    li   t2, 5
    bne  t0, t2, fail

    ld   t3, 0(s3)
    li   t4, 8
    bne  t3, t4, fail

    li   t0, 'A'
    sb   t0, 0(s0)
    li   t0, 'T'
    sb   t0, 0(s0)
    li   t0, 'M'
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
.align 3
lrsc_slot:
    .word 0
amoadd_slot:
    .word 0
.align 3
amomaxu_slot:
    .dword 0
