#pragma once

#include <stdint.h>
#include <vector>

#include "../devices/device.h"
#include "../platform/platform_events.h"

class Ram;

class Bus {
public:
    explicit Bus(Ram& ram);

    void attach(Device& device);

    uint64_t load(uint64_t addr, int size);
    void store(uint64_t addr, uint64_t value, int size);
    PlatformEvents tick();

private:
    Device* find_device(uint64_t addr);

    std::vector<Device*> devices_;
};
