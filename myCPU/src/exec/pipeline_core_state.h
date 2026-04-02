#pragma once

#include <vector>

#include "pipeline_types.h"

class PipelineCoreState {
public:
    void reset(uint64_t pc);
    void flush(uint64_t pc);
    void begin_cycle(bool interrupt_serviceable);
    void commit_next_state();
    bool pipeline_empty() const;

    uint64_t allocate_sequence();
    void record_retire(const RetireTraceEntry& entry);
    uint64_t last_sequence_id() const;
    const std::vector<RetireTraceEntry>& retire_trace() const;

    uint64_t fetch_pc{0};
    IfIdReg if_id{};
    IdExReg id_ex{};
    ExMemReg ex_mem{};
    MemWbReg mem_wb{};
    IfIdReg next_if_id{};
    IdExReg next_id_ex{};
    ExMemReg next_ex_mem{};
    MemWbReg next_mem_wb{};
    TrapRequest pending_fetch_fault{};
    uint64_t pending_fetch_fault_pc{0};
    bool stalled{false};
    bool trap_flush{false};
    bool committed{false};
    bool interrupt_serviceable_at_cycle_start{false};
    bool redirect_pending{false};
    uint64_t redirect_target{0};

private:
    PipelineSequenceState sequence_state_{};
};
