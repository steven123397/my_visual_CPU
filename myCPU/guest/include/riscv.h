#pragma once

#include <stdint.h>

#define RISCV_INTERRUPT_BIT (1ULL << 63)
#define RISCV_SUPERVISOR_TIMER_INTERRUPT 5ULL
#define RISCV_SUPERVISOR_EXTERNAL_INTERRUPT 9ULL
#define RISCV_EXC_INSN_PAGE_FAULT 12ULL
#define RISCV_EXC_LOAD_PAGE_FAULT 13ULL
#define RISCV_EXC_STORE_PAGE_FAULT 15ULL
#define RISCV_SIP_STIP (1ULL << 5)
#define RISCV_SIE_STIE (1ULL << 5)
#define RISCV_SIE_SEIE (1ULL << 9)
#define RISCV_SSTATUS_SUM (1ULL << 18)
#define RISCV_SATP_MODE_SV39 (8ULL << 60)

static inline uint64_t riscv_read_scause(void) {
    uint64_t value;
    __asm__ volatile("csrr %0, scause" : "=r"(value));
    return value;
}

static inline uint64_t riscv_read_satp(void) {
    uint64_t value;
    __asm__ volatile("csrr %0, satp" : "=r"(value));
    return value;
}

static inline uint64_t riscv_read_sepc(void) {
    uint64_t value;
    __asm__ volatile("csrr %0, sepc" : "=r"(value));
    return value;
}

static inline uint64_t riscv_read_sstatus(void) {
    uint64_t value;
    __asm__ volatile("csrr %0, sstatus" : "=r"(value));
    return value;
}

static inline void riscv_write_sepc(uint64_t value) {
    __asm__ volatile("csrw sepc, %0" :: "r"(value) : "memory");
}

static inline uint64_t riscv_read_stval(void) {
    uint64_t value;
    __asm__ volatile("csrr %0, stval" : "=r"(value));
    return value;
}

static inline void riscv_write_satp(uint64_t value) {
    __asm__ volatile("csrw satp, %0" :: "r"(value) : "memory");
}

static inline void riscv_clear_sip_bits(uint64_t value) {
    __asm__ volatile("csrc sip, %0" :: "r"(value));
}

static inline void riscv_set_sstatus_bits(uint64_t value) {
    __asm__ volatile("csrs sstatus, %0" :: "r"(value) : "memory");
}

static inline void riscv_clear_sstatus_bits(uint64_t value) {
    __asm__ volatile("csrc sstatus, %0" :: "r"(value) : "memory");
}

static inline void riscv_sfence_vma(void) {
    __asm__ volatile("sfence.vma x0, x0" ::: "memory");
}
