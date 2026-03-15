# hello.S - 通过 UART MMIO 输出 "Hello, RISC-V!\n"
.section .text
.global _start
_start:
    la   a0, msg
    li   a1, 0x10000000   # UART_BASE
loop:
    lb   a2, 0(a0)
    beqz a2, done
    sb   a2, 0(a1)
    addi a0, a0, 1
    j    loop
done:
    li   a7, 93           # exit syscall
    li   a0, 0
    ecall

.section .rodata
msg:
    .string "Hello, RISC-V!\n"
