#pragma once

#include <cstdint>

#include "device.h"
#include "../platform/address_map.h"

class Plic;

class Uart16550 : public Device {
public:
    explicit Uart16550(Plic& plic);

    uint64_t load(uint64_t addr, int size) override;
    void store(uint64_t addr, uint64_t value, int size) override;

private:
    void update_interrupt_line();

    Plic& plic_;
    uint8_t ier_{0};
};
