#pragma once

#include <cstdint>

#include "device.h"
#include "../memory.h"

class Uart16550 : public Device {
public:
    Uart16550();

    uint64_t load(uint64_t addr, int size) override;
    void store(uint64_t addr, uint64_t value, int size) override;
};
