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

private:
    uint64_t mtime_{0};
    uint64_t mtimecmp_{UINT64_MAX};
};
