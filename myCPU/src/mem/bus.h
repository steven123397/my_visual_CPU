#pragma once

#include <stdint.h>
#include <cstddef>
#include <vector>

#include "../debug/debug_snapshot.h"
#include "../devices/device.h"
#include "../platform/platform_events.h"
#include "dma_transaction.h"
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
    bool try_load_observed(uint64_t addr, int size, uint64_t& value, const char* source, const char* kind);
    bool try_store_observed(uint64_t addr, uint64_t value, int size, const char* source, const char* kind);
    DmaTransferResult dma_read(const DmaTransaction& transaction, void* data);
    DmaTransferResult dma_write(const DmaTransaction& transaction, const void* data);
    bool dma_load_bytes(uint64_t addr, void* data, size_t size, const char* initiator = "legacy-dma");
    bool dma_store_bytes(
        uint64_t addr,
        const void* data,
        size_t size,
        const char* initiator = "legacy-dma");
    PlatformEvents peek_events() const;
    PlatformEvents tick();
    const DebugBusAccess& last_access() const;
    const DebugBusAccess& last_guest_access() const;

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
        const char* detail,
        const char* source,
        const char* kind);
    void record_unmapped(
        bool write,
        uint64_t addr,
        uint64_t value,
        int size,
        const char* source,
        const char* kind);

    std::vector<Device*> devices_;
    DebugBusAccess last_access_{};
    DebugBusAccess last_guest_access_{};
};
