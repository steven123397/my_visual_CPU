#pragma once

#include <cstdint>

#include "arch/core_state.h"
#include "arch/csr_file.h"
#include "mem/address_space.h"
#include "mem/simple_l1_cache.h"
#include "trap.h"

class Bus;

class CPU {
public:
    CPU();
    CPU(const CPU&) = delete;
    CPU& operator=(const CPU&) = delete;
    CPU(CPU&&) = delete;
    CPU& operator=(CPU&&) = delete;

    CoreState& core();
    const CoreState& core() const;

    CsrFile& csr();
    const CsrFile& csr() const;

    TrapController& trap();
    const TrapController& trap() const;

    AddressSpace& address_space();
    const AddressSpace& address_space() const;
    SimpleL1DataCache& l1_data_cache();
    const SimpleL1DataCache& l1_data_cache() const;

private:
    CoreState core_{};
    CsrFile csr_{};
    TrapController trap_;
    AddressSpace address_space_;
    SimpleL1DataCache l1_data_cache_{};
};

void cpu_init(CPU& cpu, uint64_t entry);
void cpu_step(CPU& cpu, Bus& bus);
uint64_t csr_read(const CPU& cpu, uint32_t addr);
void csr_write(CPU& cpu, uint32_t addr, uint64_t val);
