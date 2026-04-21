#pragma once

#include <vector>

#include "../arch/core_state.h"
#include "load_store_queue.h"
#include "physical_register_file.h"
#include "pipeline_types.h"
#include "rename_map.h"
#include "reorder_buffer.h"

enum class PipelineStallReason : uint8_t {
    None,
    DecodeBackpressure,
    SourceOperandsNotReady,
    BlockedByUnresolvedStore,
    BlockedByOverlappingStore,
    SerializingSystemWaitForRobHead,
    VectorStateBusy,
    NonRamLoadWaitForRobHead,
    MemoryPathBusy,
};

class PipelineCoreState {
public:
    void reset(uint64_t pc);
    void flush(uint64_t pc);
    void begin_cycle(bool interrupt_serviceable);
    void commit_next_state();
    bool pipeline_empty() const;
    void reset_ooo_state(const CoreState& core);
    void rollback_to_committed_state(const CoreState& core);
    void note_stall(PipelineStallReason reason);

    uint64_t allocate_sequence();
    void record_retire(const RetireTraceEntry& entry);
    void record_memory(const ExecutionMemoryObservation& observation);
    void record_trap(const ExecutionTrapObservation& observation);
    uint64_t last_sequence_id() const;
    uint64_t last_retired_sequence() const;
    const std::vector<RetireTraceEntry>& retire_trace() const;
    ExecutionProfileSnapshot execution_profile() const;
    RenameMap& rename_map();
    const RenameMap& rename_map() const;
    ReorderBuffer& rob();
    const ReorderBuffer& rob() const;
    LoadStoreQueue& lsq();
    const LoadStoreQueue& lsq() const;
    PhysicalRegisterFile& phys_regs();
    const PhysicalRegisterFile& phys_regs() const;

    uint64_t fetch_pc{0};
    IfIdReg if_id{};
    IdExReg id_ex{};
    ExMemReg ex_mem{};
    MemWbReg mem_wb{};
    uint8_t ex_mem_cycles_remaining{0};
    IfIdReg next_if_id{};
    IdExReg next_id_ex{};
    ExMemReg next_ex_mem{};
    MemWbReg next_mem_wb{};
    uint8_t next_ex_mem_cycles_remaining{0};
    TrapRequest pending_fetch_fault{};
    uint64_t pending_fetch_fault_pc{0};
    bool stalled{false};
    PipelineStallReason stall_reason{PipelineStallReason::None};
    bool trap_flush{false};
    bool replay_flush{false};
    bool committed{false};
    bool interrupt_serviceable_at_cycle_start{false};
    bool redirect_pending{false};
    uint64_t redirect_target{0};
    LsqLoadStatus lsq_observed_load_status{};

private:
    void rebuild_committed_phys_state(const CoreState& core);

    PipelineSequenceState sequence_state_{};
    uint64_t last_retired_sequence_{0};
    RenameMap rename_map_{};
    ReorderBuffer rob_{};
    LoadStoreQueue lsq_{};
    PhysicalRegisterFile phys_regs_{};
};
