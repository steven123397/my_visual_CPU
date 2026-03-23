#pragma once

#include <array>
#include <cstdint>

#include "device.h"
#include "../platform/address_map.h"

class Plic : public Device {
public:
    static constexpr uint32_t UART_SOURCE_ID = PLIC_SOURCE_UART_THRE;

    Plic();

    uint64_t load(uint64_t addr, int size) override;
    void store(uint64_t addr, uint64_t value, int size) override;
    PlatformEvents tick() override;

    void set_source_level(uint32_t source_id, bool asserted);

private:
    struct ContextState {
        uint32_t enables{0};
        uint32_t threshold{0};
    };

    static constexpr uint32_t kNumSources = 1;
    static constexpr uint32_t kPendingOffset = PLIC_PENDING_OFFSET;
    static constexpr uint32_t kMachineEnableOffset = PLIC_ENABLE_OFFSET(PLIC_CONTEXT_MACHINE);
    static constexpr uint32_t kSupervisorEnableOffset = PLIC_ENABLE_OFFSET(PLIC_CONTEXT_SUPERVISOR);
    static constexpr uint32_t kMachineContextOffset = PLIC_CONTEXT_OFFSET(PLIC_CONTEXT_MACHINE);
    static constexpr uint32_t kSupervisorContextOffset = PLIC_CONTEXT_OFFSET(PLIC_CONTEXT_SUPERVISOR);

    uint32_t best_pending_source(const ContextState& context) const;
    bool context_has_pending(const ContextState& context) const;
    uint32_t claim(ContextState& context);
    void complete(uint32_t source_id);

    std::array<uint32_t, kNumSources + 1> priorities_{};
    std::array<bool, kNumSources + 1> levels_{};
    std::array<bool, kNumSources + 1> pending_{};
    std::array<bool, kNumSources + 1> claimed_{};
    ContextState machine_context_{};
    ContextState supervisor_context_{};
};
