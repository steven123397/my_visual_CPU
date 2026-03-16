#pragma once

#include <stdint.h>

#include "ram.h"

// Temporary bus adaptor over the legacy C memory/MMIO implementation.
class Bus {
public:
    explicit Bus(Ram& ram);

    uint64_t load(uint64_t addr, int size);
    void store(uint64_t addr, uint64_t value, int size);
    void load_binary(const char* path, uint64_t addr);

    Memory* raw_memory();
    const Memory* raw_memory() const;

private:
    Ram& ram_;
};
