#pragma once

#include <cstdint>

#include "../memory.h"

class Clint {
public:
    bool contains(uint64_t addr) const;
    uint64_t load(uint64_t addr, int size) const;
    void store(uint64_t addr, uint64_t value, int size);

    void tick();
    bool timer_pending() const;

private:
    uint64_t mtime_{0};
    uint64_t mtimecmp_{UINT64_MAX};
};
