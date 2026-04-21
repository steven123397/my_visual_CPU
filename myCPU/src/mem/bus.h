#pragma once

#include <stdint.h>
#include <cstddef>
#include <vector>

#include "../debug/debug_snapshot.h"
#include "../devices/device.h"
#include "../platform/platform_events.h"
#include "memory_region.h"

class Ram;

class Bus {
public:
    explicit Bus(Ram& ram);

    void attach(Device& device);

    PhysicalRegionInfo describe_region(uint64_t addr, int size) const;
    PhysicalSpanInfo describe_span(uint64_t addr, uint64_t bytes) const;
    bool try_load(uint64_t addr, int size, uint64_t& value);
    bool try_store(uint64_t addr, uint64_t value, int size);
    bool dma_load_bytes(uint64_t addr, void* data, size_t size);
    bool dma_store_bytes(uint64_t addr, const void* data, size_t size);
    PlatformEvents peek_events() const;
    PlatformEvents tick();
    const DebugBusAccess& last_access() const;

private:
    Device* find_device(uint64_t addr, int size);
    const Device* find_device(uint64_t addr, int size) const;
    void record_access(
        const Device& device,
        bool success,
        bool write,
        uint64_t addr,
        uint64_t value,
        int size,
        const char* detail);

    std::vector<Device*> devices_;
    DebugBusAccess last_access_{};
};
