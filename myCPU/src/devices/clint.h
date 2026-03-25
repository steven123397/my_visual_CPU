#pragma once

#include <cstdint>

#include "device.h"
#include "../platform/address_map.h"

class Clint : public Device {
public:
    Clint();

    uint64_t load(uint64_t addr, int size) override;
    void store(uint64_t addr, uint64_t value, int size) override;
    PlatformEvents tick() override;
    uint64_t mtime() const;
    uint64_t mtimecmp() const;
    bool timer_interrupt_pending() const;
    const char* debug_name() const override {
        return "clint";
    }

private:
    static uint64_t mask_for_size(int size);
    static bool access_in_range(uint64_t offset, uint64_t reg_offset, int size);
    static uint64_t load_register_slice(uint64_t reg, uint64_t reg_offset, uint64_t offset, int size);
    static void store_register_slice(uint64_t& reg, uint64_t reg_offset, uint64_t offset, uint64_t value, int size);

    uint64_t mtime_{0};
    uint64_t mtimecmp_{UINT64_MAX};
};
