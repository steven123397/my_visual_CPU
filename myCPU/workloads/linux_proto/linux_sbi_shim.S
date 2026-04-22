#include "platform_mmio.h"

#define CSR_MSTATUS 0x300
#define CSR_MIDELEG 0x303
#define CSR_MIE 0x304
#define CSR_MTVEC 0x305
#define CSR_MCOUNTEREN 0x306
#define CSR_MEDELEG 0x302
#define CSR_MSCRATCH 0x340
#define CSR_MEPC 0x341
#define CSR_MCAUSE 0x342
#define CSR_PMPCFG0 0x3a0
#define CSR_PMPADDR0 0x3b0

#define MSTATUS_MPP_MASK (3 << 11)
#define MSTATUS_MPP_SUPERVISOR (1 << 11)
#define MEDELEG_SUPERVISOR_MASK 0xb1ff
#define MIDELEG_SUPERVISOR_MASK 0x222
#define MCOUNTEREN_TIME_CYCLE_INSTRET 0x7
#define PMP_RWX_NAPOT 0xf

#define SBI_SUCCESS 0
#define SBI_ERR_NOT_SUPPORTED -2
#define SBI_SPEC_VERSION_0_2 2
#define SBI_IMPL_ID_MYCPU 1
#define SBI_IMPL_VERSION_0 0

#define SBI_EXT_BASE 0x10
#define SBI_EXT_TIME 0x54494d45

#define SBI_EXT_BASE_GET_SPEC_VERSION 0
#define SBI_EXT_BASE_GET_IMP_ID 1
#define SBI_EXT_BASE_GET_IMP_VERSION 2
#define SBI_EXT_BASE_PROBE_EXT 3
#define SBI_EXT_BASE_GET_MVENDORID 4
#define SBI_EXT_BASE_GET_MARCHID 5
#define SBI_EXT_BASE_GET_MIMPID 6
#define SBI_EXT_TIME_SET_TIMER 0

#define CAUSE_ECALL_S 9

    .section .text
    .globl _start
_start:
    la t0, trap_entry
    csrw CSR_MTVEC, t0

    li t0, -1
    csrw CSR_PMPADDR0, t0
    li t0, PMP_RWX_NAPOT
    csrw CSR_PMPCFG0, t0

    li t0, MEDELEG_SUPERVISOR_MASK
    csrw CSR_MEDELEG, t0
    li t0, MIDELEG_SUPERVISOR_MASK
    csrw CSR_MIDELEG, t0
    li t0, MCOUNTEREN_TIME_CYCLE_INSTRET
    csrw CSR_MCOUNTEREN, t0
    csrw CSR_MIE, zero

    csrr t0, CSR_MSTATUS
    li t1, MSTATUS_MPP_MASK
    not t1, t1
    and t0, t0, t1
    li t1, MSTATUS_MPP_SUPERVISOR
    or t0, t0, t1
    csrw CSR_MSTATUS, t0

    la t0, trap_stack_top
    csrw CSR_MSCRATCH, t0

    csrw CSR_MEPC, a2
    mret

trap_entry:
    csrrw sp, CSR_MSCRATCH, sp

    csrr t0, CSR_MCAUSE
    li t1, CAUSE_ECALL_S
    bne t0, t1, trap_park

    li t0, SBI_EXT_BASE
    beq a7, t0, handle_base
    li t0, SBI_EXT_TIME
    beq a7, t0, handle_time
    j sbi_unsupported

handle_base:
    li t0, SBI_EXT_BASE_GET_SPEC_VERSION
    beq a6, t0, sbi_get_spec_version
    li t0, SBI_EXT_BASE_GET_IMP_ID
    beq a6, t0, sbi_get_imp_id
    li t0, SBI_EXT_BASE_GET_IMP_VERSION
    beq a6, t0, sbi_get_imp_version
    li t0, SBI_EXT_BASE_PROBE_EXT
    beq a6, t0, sbi_probe_extension
    li t0, SBI_EXT_BASE_GET_MVENDORID
    beq a6, t0, sbi_get_zero_value
    li t0, SBI_EXT_BASE_GET_MARCHID
    beq a6, t0, sbi_get_zero_value
    li t0, SBI_EXT_BASE_GET_MIMPID
    beq a6, t0, sbi_get_zero_value
    j sbi_unsupported

handle_time:
    li t0, SBI_EXT_TIME_SET_TIMER
    bne a6, t0, sbi_unsupported
    li t0, CLINT_BASE + CLINT_REG_MTIMECMP
    sd a0, 0(t0)
    li a0, SBI_SUCCESS
    li a1, 0
    j sbi_return

sbi_get_spec_version:
    li a0, SBI_SUCCESS
    li a1, SBI_SPEC_VERSION_0_2
    j sbi_return

sbi_get_imp_id:
    li a0, SBI_SUCCESS
    li a1, SBI_IMPL_ID_MYCPU
    j sbi_return

sbi_get_imp_version:
    li a0, SBI_SUCCESS
    li a1, SBI_IMPL_VERSION_0
    j sbi_return

sbi_probe_extension:
    li t0, SBI_EXT_BASE
    beq a0, t0, sbi_probe_supported
    li t0, SBI_EXT_TIME
    beq a0, t0, sbi_probe_supported
    li a0, SBI_SUCCESS
    li a1, 0
    j sbi_return

sbi_probe_supported:
    li a0, SBI_SUCCESS
    li a1, 1
    j sbi_return

sbi_get_zero_value:
    li a0, SBI_SUCCESS
    li a1, 0
    j sbi_return

sbi_unsupported:
    li a0, SBI_ERR_NOT_SUPPORTED
    li a1, 0

sbi_return:
    csrr t0, CSR_MEPC
    addi t0, t0, 4
    csrw CSR_MEPC, t0
    csrrw sp, CSR_MSCRATCH, sp
    mret

trap_park:
    j trap_park

    .section .bss
    .align 12
trap_stack:
    .space 4096
trap_stack_top:
