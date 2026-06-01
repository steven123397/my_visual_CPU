.section .text
.global _start
_start:
    li a0, 1
    la a1, message
    li a2, 6
    li a7, 64
    ecall

    li a0, 0
    li a7, 94
    ecall

1:
    j 1b

.section .rodata
message:
    .ascii "hello\n"
