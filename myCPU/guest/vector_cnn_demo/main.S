.option norelax

.macro VSETCFG sew_code, vl
    .word (((0x40 + (\sew_code)) << 25) | (((\vl) - 1) << 20) | (7 << 12) | 0x57)
.endm

.macro VLE vd, rs1, imm
    .word ((((\imm) & 0xFFF) << 20) | ((\rs1) << 15) | ((\vd) << 7) | 0x07)
.endm

.macro VSE vs2, rs1, imm
    .word (((((\imm) >> 5) & 0x7F) << 25) | ((\vs2) << 20) | ((\rs1) << 15) | (((\imm) & 0x1F) << 7) | 0x27)
.endm

.macro VDOT vd, vs1, vs2
    .word ((0x22 << 25) | ((\vs2) << 20) | ((\vs1) << 15) | ((\vd) << 7) | 0x57)
.endm

.macro VMAX vd, vs1, vs2
    .word ((0x21 << 25) | ((\vs2) << 20) | ((\vs1) << 15) | ((\vd) << 7) | 0x57)
.endm

.section .text
.global vector_cnn_demo_main

vector_cnn_demo_main:
    la   s0, output_buffer
    la   s1, conv_input
    la   s2, conv_kernel
    la   s3, relu_zero

    # Fixed int8 1D conv, store each result as int32.
    VSETCFG 0, 4
    VLE 1, 9, 0
    VLE 2, 18, 0
    VDOT 3, 1, 2
    VSETCFG 3, 1
    VSE 3, 8, 0

    VSETCFG 0, 4
    VLE 1, 9, 1
    VLE 2, 18, 0
    VDOT 3, 1, 2
    VSETCFG 3, 1
    VSE 3, 8, 4

    VSETCFG 0, 4
    VLE 1, 9, 2
    VLE 2, 18, 0
    VDOT 3, 1, 2
    VSETCFG 3, 1
    VSE 3, 8, 8

    # ReLU over the 3 int32 conv outputs.
    VSETCFG 3, 3
    VLE 4, 8, 0
    VLE 0, 8, 0
    VLE 31, 19, 0
    VMAX 5, 4, 31
    VSE 5, 8, 0

    la   t0, output_buffer
    la   t1, expected_output
    li   t2, 12
1:
    lbu  t3, 0(t0)
    lbu  t4, 0(t1)
    bne  t3, t4, fail
    addi t0, t0, 1
    addi t1, t1, 1
    addi t2, t2, -1
    bnez t2, 1b

    la   a0, success_msg
    call print_string
    li   a0, 0
    call platform_shutdown

fail:
    la   a0, fail_msg
    call print_string
    li   a0, 1
    call platform_shutdown

print_string:
    addi sp, sp, -32
    sd   ra, 24(sp)
    sd   s3, 16(sp)
    mv   s3, a0
2:
    lbu  t6, 0(s3)
    beqz t6, 3f
    mv   a0, t6
    call platform_uart_putc
    addi s3, s3, 1
    j    2b
3:
    ld   s3, 16(sp)
    ld   ra, 24(sp)
    addi sp, sp, 32
    ret

.section .rodata
success_msg:
    .asciz "V3OK"
fail_msg:
    .asciz "V3X"

.balign 16
conv_input:
    .byte 2, -1, 3, 4, -2, 1
conv_kernel:
    .byte 1, 0, -1, 2

.balign 16
relu_zero:
    .zero 16

.balign 16
expected_output:
    .word 7, 0, 7

.section .bss
.balign 16
output_buffer:
    .space 12
