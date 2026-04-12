#pragma once

#include <cstdint>

enum class PhysicalRegionKind : uint8_t {
    Ram,
    Mmio,
    Unmapped,
};

struct PhysicalRegionInfo {
    PhysicalRegionKind kind{PhysicalRegionKind::Unmapped};
    bool cacheable{false};
    bool dma_visible{false};
    bool has_side_effect{false};
    bool supports_burst{false};
    const char* label{"unmapped"};
};

struct PhysicalSpanInfo {
    bool ok{false};
    PhysicalRegionInfo region{};
    uint64_t first_addr{0};
    uint64_t size{0};
};

inline constexpr PhysicalRegionInfo make_unmapped_region_info() {
    return {};
}

