#pragma once
#include <stdint.h>
#include <stdbool.h>

// CSR addresses
#define CSR_MSTATUS  0x300
#define CSR_MISA     0x301
#define CSR_MIE      0x304
#define CSR_MTVEC    0x305
#define CSR_MSCRATCH 0x340
#define CSR_MEPC     0x341
#define CSR_MCAUSE   0x342
#define CSR_MTVAL    0x343
#define CSR_MIP      0x344
#define CSR_CYCLE    0xC00
#define CSR_TIME     0xC01

// mstatus bits
#define MSTATUS_MIE  (1UL << 3)
#define MSTATUS_MPIE (1UL << 7)

// mie/mip bits
#define MIE_MSIE (1UL << 3)
#define MIE_MTIE (1UL << 7)
#define MIE_MEIE (1UL << 11)

typedef struct {
    uint64_t x[32];
    uint64_t pc;
    uint64_t csr[4096];
    uint64_t cycle;
    bool halted;
} CPU;

void cpu_init(CPU *cpu, uint64_t entry);
void cpu_step(CPU *cpu);
uint64_t csr_read(CPU *cpu, uint32_t addr);
void csr_write(CPU *cpu, uint32_t addr, uint64_t val);
