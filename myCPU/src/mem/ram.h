#pragma once

#include <cstddef>
#include <cstdint>

#include "../devices/device.h"

extern "C" {
#include "../memory.h"
}

class Ram : public Device {
public:
    Ram();
    ~Ram();

    Ram(const Ram&) = delete;
    Ram& operator=(const Ram&) = delete;

    uint64_t load(uint64_t addr, int size) override;
    void store(uint64_t addr, uint64_t value, int size) override;
    void write_bytes(uint64_t addr, const void* data, size_t size);
    void fill(uint64_t addr, uint8_t value, size_t size);
    void swap(Ram& other);
    const char* debug_name() const override {
        return "ram";
    }
    bool is_mmio() const override {
        return false;
    }
    PhysicalRegionInfo region_info() const override {
        return {
            .kind = PhysicalRegionKind::Ram,
            .cacheable = true,
            .dma_visible = true,
            .has_side_effect = false,
            .supports_burst = true,
            .label = debug_name(),
        };
    }

private:
    Memory mem_{};
};
