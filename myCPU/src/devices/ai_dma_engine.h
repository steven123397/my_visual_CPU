#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

#include "ai_scratchpad.h"
#include "ai_submission_queue.h"
#include "../mem/dma_transaction.h"

class Bus;

enum class AiDmaTransferKind : uint8_t {
    Load,
    Store,
};

struct AiDmaTimingConfig {
    uint32_t setup_cycles{2};
    uint32_t bytes_per_cycle{16};
};

struct AiDmaRequest {
    AiDmaTransferKind kind{AiDmaTransferKind::Load};
    uint64_t system_addr{0};
    AiScratchpadSpace space{AiScratchpadSpace::Scratchpad};
    uint32_t scratchpad_offset{0};
    uint32_t size{0};
    const char* initiator{"ai-accelerator"};
};

struct AiDmaCounters {
    uint64_t total_cycles{0};
    uint64_t load_cycles{0};
    uint64_t store_cycles{0};
    uint64_t load_bytes{0};
    uint64_t store_bytes{0};
    uint64_t load_transfers{0};
    uint64_t store_transfers{0};
};

struct AiDmaTickResult {
    bool completed{false};
    bool faulted{false};
    uint32_t fault_code{AI_ACCEL_FAULT_NONE};
    size_t bytes_moved{0};
    AiDmaTransferKind kind{AiDmaTransferKind::Load};
    DmaTransferResult dma_result{};
};

class AiDmaEngine {
public:
    explicit AiDmaEngine(AiScratchpad& scratchpad, AiDmaTimingConfig timing = {});

    void reset();
    bool busy() const;
    const AiDmaTimingConfig& timing() const;
    const AiDmaCounters& counters() const;
    uint64_t remaining_cycles() const;

    bool start(const AiDmaRequest& request, uint32_t& fault_code, std::string& error);
    AiDmaTickResult tick(Bus& bus);

private:
    uint64_t transfer_cycles(uint32_t bytes) const;

    struct ActiveTransfer {
        AiDmaRequest request{};
        uint64_t remaining_cycles{0};
        std::vector<uint8_t> buffer{};
    };

    AiScratchpad& scratchpad_;
    AiDmaTimingConfig timing_{};
    AiDmaCounters counters_{};
    ActiveTransfer active_{};
    bool active_valid_{false};
};
