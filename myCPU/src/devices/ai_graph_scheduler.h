#pragma once

#include <cstdint>
#include <string>

#include "ai_graph_package.h"
#include "ai_scratchpad.h"
#include "ai_submission_queue.h"

struct AiGraphSchedulerTiming {
    uint32_t ops_per_cycle{32};
    uint32_t tile_setup_cycles{1};
    bool allow_dma_compute_overlap{false};
};

struct AiOpProfileSummary {
    uint16_t op_index{0};
    AiOpCode opcode{AiOpCode::Invalid};
    uint64_t retired_ops{0};
    uint64_t compute_cycles{0};
    uint64_t stall_cycles{0};
    uint64_t tile_count{0};
};

struct AiGraphExecutionResult {
    uint32_t fault{AI_ACCEL_FAULT_NONE};
    uint32_t fault_detail{0};
    uint64_t retired_ops{0};
    uint64_t compute_cycles{0};
    uint64_t stall_cycles{0};
    uint64_t tile_count{0};
    uint32_t scratchpad_peak_bytes{0};
    std::vector<AiOpProfileSummary> op_summaries{};
};

class AiGraphScheduler {
public:
    explicit AiGraphScheduler(AiScratchpad& scratchpad, AiGraphSchedulerTiming timing = {});

    const AiGraphSchedulerTiming& timing() const;

    bool execute(const AiGraphPackage& package,
                 AiGraphExecutionResult& result,
                 std::string& error) const;

private:
    AiScratchpad& scratchpad_;
    AiGraphSchedulerTiming timing_{};
};
