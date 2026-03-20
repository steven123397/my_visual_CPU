#pragma once

#include <stdint.h>

#include "../devices/clint.h"
#include "../devices/uart16550.h"
#include "ram.h"

class Bus {
public:
    Bus(Ram& ram, Uart16550& uart, Clint& clint);

    uint64_t load(uint64_t addr, int size);
    void store(uint64_t addr, uint64_t value, int size);
    void load_binary(const char* path, uint64_t addr);
    void tick();
    bool timer_pending() const;

    Memory* raw_ram();
    const Memory* raw_ram() const;

private:
    Ram& ram_;
    Uart16550& uart_;
    Clint& clint_;
};
