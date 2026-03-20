#pragma once

#include <cstdint>

#include "device.h"
#include "../memory.h"

class Clint : public Device {
public:
    Clint();

    uint64_t load(uint64_t addr, int size) override;
    void store(uint64_t addr, uint64_t value, int size) override;

    bool tick();

private:
    uint64_t mtime_{0};
    uint64_t mtimecmp_{UINT64_MAX};
};
