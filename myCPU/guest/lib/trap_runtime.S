.option norelax
.section .text

.global trap_user_runtime_arch_enter
.global trap_user_runtime_arch_resume

.equ TRAP_USER_RUNTIME_SAVED_SUPERVISOR_SP_OFFSET, 0
.equ TRAP_USER_RUNTIME_SUPERVISOR_TRAP_STACK_TOP_OFFSET, 8

trap_user_runtime_arch_enter:
    sd   sp, TRAP_USER_RUNTIME_SAVED_SUPERVISOR_SP_OFFSET(a0)
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
    addi sp, sp, -16
    sd   ra, 0(sp)
    call trap_active_user_runtime
    ld   t1, 0(sp)
    addi sp, sp, 16
    beqz a0, 1f
    ld   sp, TRAP_USER_RUNTIME_SAVED_SUPERVISOR_SP_OFFSET(a0)
    beqz sp, 1f
    sd   zero, TRAP_USER_RUNTIME_SAVED_SUPERVISOR_SP_OFFSET(a0)
    csrw sscratch, zero
    mv   ra, t1
    ret

1:
    li   a0, 1
    call platform_shutdown
