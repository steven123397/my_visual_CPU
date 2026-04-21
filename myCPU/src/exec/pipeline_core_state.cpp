#include "pipeline_core_state.h"

void PipelineCoreState::reset(uint64_t pc) {
    flush(pc);
    stalled = false;
    stall_reason = PipelineStallReason::None;
    trap_flush = false;
    replay_flush = false;
    committed = false;
    interrupt_serviceable_at_cycle_start = false;
    sequence_state_.clear();
    last_retired_sequence_ = 0;
}

void PipelineCoreState::flush(uint64_t pc) {
    if_id = {};
    id_ex = {};
    ex_mem = {};
    mem_wb = {};
    ex_mem_cycles_remaining = 0;
    next_if_id = {};
    next_id_ex = {};
    next_ex_mem = {};
    next_mem_wb = {};
    next_ex_mem_cycles_remaining = 0;
    fetch_pc = pc;
    pending_fetch_fault = {};
    pending_fetch_fault_pc = 0;
    redirect_pending = false;
    redirect_target = 0;
    lsq_observed_load_status = {};
    stall_reason = PipelineStallReason::None;
}

void PipelineCoreState::begin_cycle(bool interrupt_serviceable) {
    next_if_id = {};
    next_id_ex = {};
    next_ex_mem = {};
    next_mem_wb = {};
    next_ex_mem_cycles_remaining = 0;
    stalled = false;
    stall_reason = PipelineStallReason::None;
    trap_flush = false;
    replay_flush = false;
    committed = false;
    interrupt_serviceable_at_cycle_start = interrupt_serviceable;
    redirect_pending = false;
    redirect_target = 0;
    lsq_observed_load_status = {};
}

void PipelineCoreState::note_stall(PipelineStallReason reason) {
    stalled = true;
    if (stall_reason == PipelineStallReason::None) {
        stall_reason = reason;
    }
}

void PipelineCoreState::commit_next_state() {
    if_id = next_if_id;
    id_ex = next_id_ex;
    ex_mem = next_ex_mem;
    mem_wb = next_mem_wb;
    ex_mem_cycles_remaining = next_ex_mem_cycles_remaining;
}

bool PipelineCoreState::pipeline_empty() const {
    return !if_id.slot.valid && !id_ex.slot.valid && !ex_mem.slot.valid && !mem_wb.slot.valid &&
           rob_.size() == 0 && lsq_.size() == 0;
}

void PipelineCoreState::reset_ooo_state(const CoreState& core) {
    rename_map_ = RenameMap{};
    rob_.clear();
    lsq_.clear();
    phys_regs_.reset();
    rebuild_committed_phys_state(core);
}

void PipelineCoreState::rollback_to_committed_state(const CoreState& core) {
    rename_map_.rollback(rename_map_.committed_checkpoint());
    rob_.clear();
    lsq_.clear();
    rebuild_committed_phys_state(core);
}

uint64_t PipelineCoreState::allocate_sequence() {
    return sequence_state_.allocate();
}

void PipelineCoreState::record_retire(const RetireTraceEntry& entry) {
    sequence_state_.record(entry);
    if (entry.sequence_id != 0) {
        last_retired_sequence_ = entry.sequence_id;
    }
}

void PipelineCoreState::record_memory(const ExecutionMemoryObservation& observation) {
    sequence_state_.record_memory(observation);
}

void PipelineCoreState::record_trap(const ExecutionTrapObservation& observation) {
    sequence_state_.record_trap(observation);
}

uint64_t PipelineCoreState::last_sequence_id() const {
    return sequence_state_.last_sequence_id();
}

uint64_t PipelineCoreState::last_retired_sequence() const {
    return last_retired_sequence_;
}

const std::vector<RetireTraceEntry>& PipelineCoreState::retire_trace() const {
    return sequence_state_.retire_trace();
}

ExecutionProfileSnapshot PipelineCoreState::execution_profile() const {
    return sequence_state_.profile_snapshot();
}

RenameMap& PipelineCoreState::rename_map() {
    return rename_map_;
}

const RenameMap& PipelineCoreState::rename_map() const {
    return rename_map_;
}

ReorderBuffer& PipelineCoreState::rob() {
    return rob_;
}

const ReorderBuffer& PipelineCoreState::rob() const {
    return rob_;
}

LoadStoreQueue& PipelineCoreState::lsq() {
    return lsq_;
}

const LoadStoreQueue& PipelineCoreState::lsq() const {
    return lsq_;
}

PhysicalRegisterFile& PipelineCoreState::phys_regs() {
    return phys_regs_;
}

const PhysicalRegisterFile& PipelineCoreState::phys_regs() const {
    return phys_regs_;
}

void PipelineCoreState::rebuild_committed_phys_state(const CoreState& core) {
    phys_regs_.reset();
    for (uint8_t arch = 0; arch < 32; ++arch) {
        const uint32_t phys = rename_map_.architectural_source(arch);
        phys_regs_.write(phys, core.read_gpr(arch));
    }
}
