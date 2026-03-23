#include "../../include/platform_mmio.h"
.option norelax

.equ TRAP_FRAME_SIZE, 240

.section .text.start
.global _start

_start:
    la   t0, machine_trap
    csrw mtvec, t0

    la   t0, supervisor_trap
    csrw stvec, t0

    li   t0, ((1 << 8) | (1 << 12) | (1 << 13) | (1 << 15))
    csrw medeleg, t0

    li   t0, ((1 << 9) | (1 << 5))
    csrw mideleg, t0
    csrw sie, t0

    li   t0, 0x802        # MPP = S, SIE = 1
    csrw mstatus, t0

    la   t0, supervisor_entry
    csrw mepc, t0
    mret

supervisor_entry:
    lla  sp, boot_stack_top
    csrw sscratch, zero
    call kernel_main
    li   a0, 0
    call platform_shutdown

.global riscv_enter_user_mode
.global supervisor_resume_after_user
riscv_enter_user_mode:
    mv   t0, a0
    lla  t1, supervisor_saved_sp
    sd   sp, 0(t1)
    mv   a0, a1
    mv   sp, a2
    lla  t1, supervisor_trap_stack_top
    csrw sscratch, t1
    li   t1, (1 << 8)
    csrc sstatus, t1
    li   t1, (1 << 5)
    csrs sstatus, t1
    csrw sepc, t0
    sret

supervisor_resume_after_user:
    lla  t0, supervisor_saved_sp
    ld   sp, 0(t0)
    csrw sscratch, zero
    ret

supervisor_trap:
    csrrw sp, sscratch, sp
    bnez sp, 1f
    csrrw sp, sscratch, sp
1:
    addi sp, sp, -TRAP_FRAME_SIZE
    sd   ra, 0(sp)
    sd   gp, 8(sp)
    sd   tp, 16(sp)
    sd   t0, 24(sp)
    sd   t1, 32(sp)
    sd   t2, 40(sp)
    sd   s0, 48(sp)
    sd   s1, 56(sp)
    sd   a0, 64(sp)
    sd   a1, 72(sp)
    sd   a2, 80(sp)
    sd   a3, 88(sp)
    sd   a4, 96(sp)
    sd   a5, 104(sp)
    sd   a6, 112(sp)
    sd   a7, 120(sp)
    sd   s2, 128(sp)
    sd   s3, 136(sp)
    sd   s4, 144(sp)
    sd   s5, 152(sp)
    sd   s6, 160(sp)
    sd   s7, 168(sp)
    sd   s8, 176(sp)
    sd   s9, 184(sp)
    sd   s10, 192(sp)
    sd   s11, 200(sp)
    sd   t3, 208(sp)
    sd   t4, 216(sp)
    sd   t5, 224(sp)
    sd   t6, 232(sp)

    call supervisor_trap_dispatch

    ld   ra, 0(sp)
    ld   gp, 8(sp)
    ld   tp, 16(sp)
    ld   t1, 32(sp)
    ld   t2, 40(sp)
    ld   s0, 48(sp)
    ld   s1, 56(sp)
    ld   a0, 64(sp)
    ld   a1, 72(sp)
    ld   a2, 80(sp)
    ld   a3, 88(sp)
    ld   a4, 96(sp)
    ld   a5, 104(sp)
    ld   a6, 112(sp)
    ld   a7, 120(sp)
    ld   s2, 128(sp)
    ld   s3, 136(sp)
    ld   s4, 144(sp)
    ld   s5, 152(sp)
    ld   s6, 160(sp)
    ld   s7, 168(sp)
    ld   s8, 176(sp)
    ld   s9, 184(sp)
    ld   s10, 192(sp)
    ld   s11, 200(sp)
    ld   t3, 208(sp)
    ld   t4, 216(sp)
    csrr t0, sstatus
    andi t0, t0, (1 << 8)
    ld   t6, 232(sp)
    beqz t0, 2f
    ld   t5, 224(sp)
    ld   t0, 24(sp)
    addi sp, sp, TRAP_FRAME_SIZE
    sret

2:
    ld   t5, 224(sp)
    ld   t0, 24(sp)
    addi sp, sp, TRAP_FRAME_SIZE
    csrrw sp, sscratch, sp
    sret

supervisor_trap_fail:
    li   a0, 'X'
    call platform_uart_putc
    li   a0, 1
    call platform_shutdown

machine_trap:
    li   a0, 'X'
    call platform_uart_putc
    li   a0, 1
    call platform_shutdown

.section .text.user
.balign 4096
.global user_test_entry
.global user_test_ecall
user_test_entry:
    addi sp, sp, -16
    sd   a0, 0(sp)
    ld   t3, 0(sp)
    bne  t3, a0, user_test_fail
    li   t0, 0x5A5A1234
    sw   t0, 8(a0)
    lw   t1, 8(a0)
    li   t2, 0x5A5A1234
    bne  t1, t2, user_test_fail
1:
    lw   t1, 12(a0)
    beqz t1, 1b
    addi sp, sp, 16
user_test_ecall:
    ecall

user_test_fail:
    ecall

.section .bss
.balign 16
supervisor_saved_sp:
    .dword 0
supervisor_trap_stack:
    .space 4096
supervisor_trap_stack_top:
boot_stack:
    .space 4096
boot_stack_top:
