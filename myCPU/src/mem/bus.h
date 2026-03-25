#pragma once

#include <stdint.h>
#include <vector>

#include "../debug/debug_snapshot.h"
#include "../devices/device.h"
#include "../platform/platform_events.h"

class Ram;

class Bus {
public:
    explicit Bus(Ram& ram);

    void attach(Device& device);

    bool try_load(uint64_t addr, int size, uint64_t& value);
    bool try_store(uint64_t addr, uint64_t value, int size);
    PlatformEvents tick();
    const DebugBusAccess& last_access() const;
    void clear_last_access();

private:
    Device* find_device(uint64_t addr, int size);

    std::vector<Device*> devices_;
    DebugBusAccess last_access_{};
};
