#pragma once

#include <stdint.h>
#include <vector>

#include "../devices/clint.h"
#include "../devices/device.h"
#include "ram.h"

struct BusTickResult {
    bool timer_interrupt_pending{false};
};

class Bus {
public:
    Bus(Ram& ram, Clint& clint);

    void attach(Device& device);

    uint64_t load(uint64_t addr, int size);
    void store(uint64_t addr, uint64_t value, int size);
    BusTickResult tick();

private:
    Device* find_device(uint64_t addr);

    Ram& ram_;
    Clint& clint_;
    std::vector<Device*> devices_;
};
