.option norelax
.section .text

.global trap_user_runtime_arch_call
.global trap_user_runtime_arch_enter
.global trap_user_runtime_arch_resume

.equ TRAP_USER_RUNTIME_SAVED_SUPERVISOR_SP_OFFSET, 0
.equ TRAP_USER_RUNTIME_SAVED_SUPERVISOR_RA_OFFSET, 8
.equ TRAP_USER_RUNTIME_SUPERVISOR_TRAP_STACK_TOP_OFFSET, 16
.equ TRAP_USER_RUNTIME_ARCH_CALL_FRAME_SIZE, 128

trap_user_runtime_arch_call:
    addi sp, sp, -TRAP_USER_RUNTIME_ARCH_CALL_FRAME_SIZE
    sd   ra, 0(sp)
    sd   gp, 8(sp)
    sd   tp, 16(sp)
    sd   s0, 24(sp)
    sd   s1, 32(sp)
    sd   s2, 40(sp)
    sd   s3, 48(sp)
    sd   s4, 56(sp)
    sd   s5, 64(sp)
    sd   s6, 72(sp)
    sd   s7, 80(sp)
    sd   s8, 88(sp)
    sd   s9, 96(sp)
    sd   s10, 104(sp)
    sd   s11, 112(sp)
    call trap_user_runtime_arch_enter
    ld   ra, 0(sp)
    ld   gp, 8(sp)
    ld   tp, 16(sp)
    ld   s0, 24(sp)
    ld   s1, 32(sp)
    ld   s2, 40(sp)
    ld   s3, 48(sp)
    ld   s4, 56(sp)
    ld   s5, 64(sp)
    ld   s6, 72(sp)
    ld   s7, 80(sp)
    ld   s8, 88(sp)
    ld   s9, 96(sp)
    ld   s10, 104(sp)
    ld   s11, 112(sp)
    addi sp, sp, TRAP_USER_RUNTIME_ARCH_CALL_FRAME_SIZE
    ret

trap_user_runtime_arch_enter:
    sd   sp, TRAP_USER_RUNTIME_SAVED_SUPERVISOR_SP_OFFSET(a0)
    sd   ra, TRAP_USER_RUNTIME_SAVED_SUPERVISOR_RA_OFFSET(a0)
    ld   t0, TRAP_USER_RUNTIME_SUPERVISOR_TRAP_STACK_TOP_OFFSET(a0)
    beqz t0, 1f
    mv   t1, a1
    mv   a0, a2
    mv   sp, a3
    csrw sscratch, t0
    li   t0, (1 << 8)
    csrc sstatus, t0
    li   t0, (1 << 5)
    csrs sstatus, t0
    csrw sepc, t1
    sret

trap_user_runtime_arch_resume:
    call trap_active_user_runtime
    beqz a0, 1f
    ld   t0, TRAP_USER_RUNTIME_SAVED_SUPERVISOR_SP_OFFSET(a0)
    beqz t0, 1f
    ld   t1, TRAP_USER_RUNTIME_SAVED_SUPERVISOR_RA_OFFSET(a0)
    beqz t1, 1f
    sd   zero, TRAP_USER_RUNTIME_SAVED_SUPERVISOR_SP_OFFSET(a0)
    sd   zero, TRAP_USER_RUNTIME_SAVED_SUPERVISOR_RA_OFFSET(a0)
    mv   sp, t0
    csrw sscratch, zero
    mv   ra, t1
    ret

1:
    li   a0, 1
    call platform_shutdown
