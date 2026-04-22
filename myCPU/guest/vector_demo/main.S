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
.global vector_demo_main

vector_demo_main:
    la   s0, output_buffer
    la   s1, dot_a
    la   a0, dot_b
    la   a1, gemm_row0
    la   a2, gemm_row1
    la   a3, gemm_col0
    la   a4, gemm_col1
    la   a5, conv_input
    la   a6, conv_kernel
    la   a7, relu_input
    la   s3, relu_zero

    # int8 dot-product
    VSETCFG 0, 8
    VLE 1, 9, 0
    VLE 2, 10, 0
    VDOT 3, 1, 2
    VSETCFG 7, 1
    VSE 3, 8, 0

    # int32 2x2 GEMM
    VSETCFG 3, 4
    VLE 4, 11, 0
    VLE 5, 13, 0
    VDOT 6, 4, 5
    VSETCFG 7, 1
    VSE 6, 8, 8

    VSETCFG 3, 4
    VLE 4, 11, 0
    VLE 5, 14, 0
    VDOT 6, 4, 5
    VSETCFG 7, 1
    VSE 6, 8, 16

    VSETCFG 3, 4
    VLE 4, 12, 0
    VLE 5, 13, 0
    VDOT 6, 4, 5
    VSETCFG 7, 1
    VSE 6, 8, 24

    VSETCFG 3, 4
    VLE 4, 12, 0
    VLE 5, 14, 0
    VDOT 6, 4, 5
    VSETCFG 7, 1
    VSE 6, 8, 32

    # int8 1D conv
    VSETCFG 0, 4
    VLE 7, 15, 0
    VLE 8, 16, 0
    VDOT 9, 7, 8
    VSETCFG 7, 1
    VSE 9, 8, 40

    VSETCFG 0, 4
    VLE 7, 15, 1
    VLE 8, 16, 0
    VDOT 9, 7, 8
    VSETCFG 7, 1
    VSE 9, 8, 48

    VSETCFG 0, 4
    VLE 7, 15, 2
    VLE 8, 16, 0
    VDOT 9, 7, 8
    VSETCFG 7, 1
    VSE 9, 8, 56

    # ReLU should use an explicit zero vector instead of relying on reset-zero v0.
    VSETCFG 1, 8
    VLE 15, 17, 0
    VLE 0, 17, 0
    VLE 31, 19, 0
    VMAX 16, 15, 31
    VSE 16, 8, 64

    la   t0, output_buffer
    la   t1, expected_output
    li   t2, 80
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
    sd   s2, 16(sp)
    mv   s2, a0
2:
    lbu  t6, 0(s2)
    beqz t6, 3f
    mv   a0, t6
    call platform_uart_putc
    addi s2, s2, 1
    j    2b
3:
    ld   s2, 16(sp)
    ld   ra, 24(sp)
    addi sp, sp, 32
    ret

.section .rodata
success_msg:
    .asciz "V2OK"
fail_msg:
    .asciz "VX"

.balign 16
dot_a:
    .byte 1, -2, 3, -4, 5, -6, 7, -8
dot_b:
    .byte -1, 2, 3, -4, 5, 6, -7, 8

.balign 16
gemm_row0:
    .word 1, 2, -3, 4
gemm_row1:
    .word -5, 6, 7, -8
gemm_col0:
    .word 1, -2, 3, 4
gemm_col1:
    .word -1, 2, 5, -6

.balign 16
conv_input:
    .byte 2, -1, 3, 4, -2, 1
conv_kernel:
    .byte 1, 0, -1, 2

.balign 16
relu_input:
    .half -5, 0, 7, -2, 123, -327, 42, -1

.balign 16
relu_zero:
    .zero 16

.balign 16
expected_output:
    .byte 0x98, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff
    .byte 0x04, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
    .byte 0xdc, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff
    .byte 0xe4, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff
    .byte 0x64, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
    .byte 0x07, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
    .byte 0xf7, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff
    .byte 0x07, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
    .byte 0x00, 0x00, 0x00, 0x00, 0x07, 0x00, 0x00, 0x00
    .byte 0x7b, 0x00, 0x00, 0x00, 0x2a, 0x00, 0x00, 0x00

.section .bss
.balign 16
output_buffer:
    .space 80
