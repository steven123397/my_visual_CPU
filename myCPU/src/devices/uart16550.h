#pragma once

#include <cstdint>
#include <deque>
#include <string>
#include <string_view>

#include "device.h"
#include "../platform/address_map.h"

class Plic;

class Uart16550 : public Device {
public:
    explicit Uart16550(Plic& plic);

    uint64_t load(uint64_t addr, int size) override;
    void store(uint64_t addr, uint64_t value, int size) override;
    PlatformEvents tick() override;
    const char* debug_name() const override {
        return "uart";
    }

    uint8_t ier() const;
    bool thre_interrupt_asserted() const;
    size_t output_size() const;
    const std::string& output() const;
    uint8_t mcr() const;
    void inject_input(std::string_view text);
    void set_mirror_stdout(bool enabled);

private:
    bool divisor_latch_enabled() const;
    bool tx_interrupt_enabled() const;
    bool rx_interrupt_enabled() const;
    bool rx_interrupt_pending() const;
    bool tx_ready() const;
    void update_interrupt_line();

    Plic& plic_;
    uint8_t ier_{0};
    uint8_t lcr_{0};
    uint8_t mcr_{0};
    uint8_t dll_{0};
    uint8_t dlm_{0};
    uint8_t fcr_{0};
    bool tx_holding_empty_{true};
    bool tx_shift_empty_{true};
    bool tx_drain_pending_{false};
    bool tx_interrupt_pending_{false};
    std::deque<uint8_t> input_{};
    std::string output_{};
    bool mirror_stdout_{true};
};
