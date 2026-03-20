#pragma once

#include <cstdint>

#include "../memory.h"

class Uart16550 {
public:
    bool contains(uint64_t addr) const;
    uint64_t load(uint64_t addr, int size);
    void store(uint64_t addr, uint64_t value, int size);
};
