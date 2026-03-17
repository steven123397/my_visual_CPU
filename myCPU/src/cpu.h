#pragma once

#include <cstdint>

#include "arch/core_state.h"
#include "arch/csr_file.h"

struct Memory;

class CPU {
public:
    CoreState& core();
    const CoreState& core() const;

    CsrFile& csr();
    const CsrFile& csr() const;

private:
    CoreState core_{};
    CsrFile csr_{};
};

void cpu_init(CPU& cpu, uint64_t entry);
void cpu_step(CPU& cpu, Memory* mem);
uint64_t csr_read(const CPU& cpu, uint32_t addr);
void csr_write(CPU& cpu, uint32_t addr, uint64_t val);
