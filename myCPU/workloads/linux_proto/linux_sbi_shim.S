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
#define CSR_MTVAL 0x343
#define CSR_STVEC 0x105
#define CSR_SSCRATCH 0x140
#define CSR_SEPC 0x141
#define CSR_SCAUSE 0x142
#define CSR_STVAL 0x143
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

    la t0, supervisor_trap_entry
    csrw CSR_STVEC, t0
    la t0, supervisor_trap_stack_top
    csrw CSR_SSCRATCH, t0

    csrw CSR_MEPC, a2
    mret

trap_entry:
    csrrw sp, CSR_MSCRATCH, sp

    csrr t0, CSR_MCAUSE
    li t1, CAUSE_ECALL_S
    bne t0, t1, unexpected_machine_trap

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

unexpected_machine_trap:
    la a0, machine_trap_banner
    jal uart_puts
    csrr a0, CSR_MCAUSE
    jal uart_put_hex_prefixed
    la a0, trap_epc_prefix
    jal uart_puts
    csrr a0, CSR_MEPC
    jal uart_put_hex_prefixed
    la a0, trap_tval_prefix
    jal uart_puts
    csrr a0, CSR_MTVAL
    jal uart_put_hex_prefixed
    li a0, '\n'
    jal uart_putc

trap_park:
    j trap_park

supervisor_trap_entry:
    csrrw sp, CSR_SSCRATCH, sp
    la a0, supervisor_trap_banner
    jal uart_puts
    csrr a0, CSR_SCAUSE
    jal uart_put_hex_prefixed
    la a0, trap_epc_prefix
    jal uart_puts
    csrr a0, CSR_SEPC
    jal uart_put_hex_prefixed
    la a0, trap_tval_prefix
    jal uart_puts
    csrr a0, CSR_STVAL
    jal uart_put_hex_prefixed
    li a0, '\n'
    jal uart_putc
    j trap_park

uart_putc:
    li t0, UART_BASE
1:
    lbu t1, UART_REG_LSR(t0)
    andi t1, t1, UART_LSR_THRE
    beqz t1, 1b
    sb a0, UART_REG_THR(t0)
    ret

uart_puts:
    addi sp, sp, -16
    sd ra, 8(sp)
1:
    lbu t0, 0(a0)
    beqz t0, 2f
    addi a0, a0, 1
    mv a1, a0
    mv a0, t0
    jal uart_putc
    mv a0, a1
    j 1b
2:
    ld ra, 8(sp)
    addi sp, sp, 16
    ret

uart_put_hex_prefixed:
    addi sp, sp, -16
    sd ra, 8(sp)
    sd a0, 0(sp)
    li a0, '0'
    jal uart_putc
    li a0, 'x'
    jal uart_putc
    ld a0, 0(sp)
    jal uart_put_hex64
    ld ra, 8(sp)
    addi sp, sp, 16
    ret

uart_put_hex64:
    addi sp, sp, -32
    sd ra, 24(sp)
    sd a0, 16(sp)
    li t2, 16
1:
    ld t0, 16(sp)
    srli t1, t0, 60
    la t3, hex_digits
    add t3, t3, t1
    lbu a0, 0(t3)
    jal uart_putc
    ld t0, 16(sp)
    slli t0, t0, 4
    sd t0, 16(sp)
    addi t2, t2, -1
    bnez t2, 1b
    ld ra, 24(sp)
    addi sp, sp, 32
    ret

    .section .bss
    .align 12
trap_stack:
    .space 4096
trap_stack_top:

    .align 12
supervisor_trap_stack:
    .space 4096
supervisor_trap_stack_top:

    .section .rodata
machine_trap_banner:
    .asciz "[linux-sbi-shim] machine trap mcause="
supervisor_trap_banner:
    .asciz "[linux-sbi-shim] supervisor trap scause="
trap_epc_prefix:
    .asciz " epc="
trap_tval_prefix:
    .asciz " tval="
hex_digits:
    .ascii "0123456789abcdef"
