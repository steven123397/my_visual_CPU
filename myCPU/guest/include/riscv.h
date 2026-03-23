#pragma once

#include <stdint.h>

#define RISCV_INTERRUPT_BIT (1ULL << 63)
#define RISCV_SUPERVISOR_TIMER_INTERRUPT 5ULL
#define RISCV_SUPERVISOR_EXTERNAL_INTERRUPT 9ULL
#define RISCV_SIP_STIP (1ULL << 5)
#define RISCV_SIE_STIE (1ULL << 5)
#define RISCV_SIE_SEIE (1ULL << 9)

static inline uint64_t riscv_read_scause(void) {
    uint64_t value;
    __asm__ volatile("csrr %0, scause" : "=r"(value));
    return value;
}

static inline void riscv_clear_sip_bits(uint64_t value) {
    __asm__ volatile("csrc sip, %0" :: "r"(value));
}
