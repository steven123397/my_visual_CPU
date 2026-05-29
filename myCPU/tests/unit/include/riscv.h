#pragma once

#include <stdint.h>

#define RISCV_INTERRUPT_BIT (1ULL << 63)
#define RISCV_SUPERVISOR_TIMER_INTERRUPT 5ULL
#define RISCV_SUPERVISOR_EXTERNAL_INTERRUPT 9ULL
#define RISCV_EXC_ILLEGAL_INSTRUCTION 2ULL
#define RISCV_EXC_ECALL_FROM_U 8ULL
#define RISCV_EXC_INSN_PAGE_FAULT 12ULL
#define RISCV_EXC_LOAD_PAGE_FAULT 13ULL
#define RISCV_EXC_STORE_PAGE_FAULT 15ULL
#define RISCV_SSTATUS_SIE (1ULL << 1)
#define RISCV_SSTATUS_SPIE (1ULL << 5)
#define RISCV_SSTATUS_SPP (1ULL << 8)
#define RISCV_SSTATUS_SUM (1ULL << 18)
#define RISCV_SATP_MODE_SV39 (8ULL << 60)

uint64_t riscv_read_scause(void);
uint64_t riscv_read_satp(void);
uint64_t riscv_read_sepc(void);
uint64_t riscv_read_sstatus(void);
uint64_t riscv_read_stval(void);
void riscv_write_satp(uint64_t value);
void riscv_write_sepc(uint64_t value);
void riscv_clear_sstatus_bits(uint64_t value);
void riscv_set_sstatus_bits(uint64_t value);
void riscv_sfence_vma(void);
