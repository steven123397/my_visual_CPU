#include "pipeline_core_state.h"

void PipelineCoreState::reset(uint64_t pc) {
    flush(pc);
    stalled = false;
    trap_flush = false;
    committed = false;
    interrupt_serviceable_at_cycle_start = false;
    sequence_state_.clear();
}

void PipelineCoreState::flush(uint64_t pc) {
    if_id = {};
    id_ex = {};
    ex_mem = {};
    mem_wb = {};
    next_if_id = {};
    next_id_ex = {};
    next_ex_mem = {};
    next_mem_wb = {};
    fetch_pc = pc;
    pending_fetch_fault = {};
    pending_fetch_fault_pc = 0;
    redirect_pending = false;
    redirect_target = 0;
}

void PipelineCoreState::begin_cycle(bool interrupt_serviceable) {
    next_if_id = {};
    next_id_ex = {};
    next_ex_mem = {};
    next_mem_wb = {};
    stalled = false;
    trap_flush = false;
    committed = false;
    interrupt_serviceable_at_cycle_start = interrupt_serviceable;
    redirect_pending = false;
    redirect_target = 0;
}

void PipelineCoreState::commit_next_state() {
    if_id = next_if_id;
    id_ex = next_id_ex;
    ex_mem = next_ex_mem;
    mem_wb = next_mem_wb;
}

bool PipelineCoreState::pipeline_empty() const {
    return !if_id.slot.valid && !id_ex.slot.valid && !ex_mem.slot.valid && !mem_wb.slot.valid;
}

uint64_t PipelineCoreState::allocate_sequence() {
    return sequence_state_.allocate();
}

void PipelineCoreState::record_retire(const RetireTraceEntry& entry) {
    sequence_state_.record(entry);
}

uint64_t PipelineCoreState::last_sequence_id() const {
    return sequence_state_.last_sequence_id();
}

const std::vector<RetireTraceEntry>& PipelineCoreState::retire_trace() const {
    return sequence_state_.retire_trace();
}
