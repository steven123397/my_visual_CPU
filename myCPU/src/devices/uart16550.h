#pragma once

#include <cstdint>
#include <string>

#include "device.h"
#include "../platform/address_map.h"

class Plic;

class Uart16550 : public Device {
public:
    explicit Uart16550(Plic& plic);

    uint64_t load(uint64_t addr, int size) override;
    void store(uint64_t addr, uint64_t value, int size) override;
    const char* debug_name() const override {
        return "uart";
    }

    uint8_t ier() const;
    bool thre_interrupt_asserted() const;
    size_t output_size() const;
    const std::string& output() const;
    void set_mirror_stdout(bool enabled);

private:
    void update_interrupt_line();

    Plic& plic_;
    uint8_t ier_{0};
    std::string output_{};
    bool mirror_stdout_{true};
};
