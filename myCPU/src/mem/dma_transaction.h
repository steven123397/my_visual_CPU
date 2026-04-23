#pragma once

#include <cstddef>
#include <cstdint>
#include <string>

#include "memory_region.h"

enum class DmaDirection : uint8_t {
    Read,
    Write,
};

enum class DmaFault : uint8_t {
    None,
    InvalidArguments,
    Unmapped,
    SpanCrossesRegionBoundary,
    RegionNotDmaVisible,
    SideEffectRegion,
    BurstNotSupported,
    DeviceFault,
};

struct DmaTransaction {
    const char* initiator{"device"};
    uint64_t addr{0};
    size_t size{0};
    bool burst{false};
    DmaDirection direction{DmaDirection::Read};
};

struct DmaTransferResult {
    bool ok{false};
    DmaFault fault{DmaFault::InvalidArguments};
    DmaDirection direction{DmaDirection::Read};
    const char* initiator{"device"};
    uint64_t addr{0};
    size_t requested_bytes{0};
    size_t transferred_bytes{0};
    PhysicalRegionInfo region{};
    std::string detail{};

    bool complete() const;
    bool partial() const;
};

const char* dma_fault_name(DmaFault fault);

