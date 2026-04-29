#include "pipeline_sequence.h"

#include <utility>

uint64_t PipelineSequenceState::allocate() {
    last_sequence_id_ += 1;
    return last_sequence_id_;
}

void PipelineSequenceState::clear() {
    last_sequence_id_ = 0;
    retire_trace_.clear();
    profile_.clear();
}

void PipelineSequenceState::record(const RetireTraceEntry& entry) {
    if (entry.sequence_id == 0) {
        return;
    }

    retire_trace_.push_back(entry);
    if (retire_trace_.size() > kRetireTraceCapacity) {
        retire_trace_.erase(
            retire_trace_.begin(),
            retire_trace_.begin() + static_cast<std::ptrdiff_t>(retire_trace_.size() - kRetireTraceCapacity));
    }

    profile_.record_retire(ExecutionRetireObservation{
        .pc = entry.pc,
        .raw = entry.raw,
        .trap = entry.trap,
        .redirect = entry.redirect,
        .cycle_valid = entry.cycle_valid,
        .cycle = entry.cycle,
        .target_pc_valid = entry.target_pc_valid,
        .target_pc = entry.target_pc,
    });
}

void PipelineSequenceState::record_memory(const ExecutionMemoryObservation& observation) {
    profile_.record_memory(observation);
}

void PipelineSequenceState::record_trap(const ExecutionTrapObservation& observation) {
    profile_.record_trap(observation);
}

uint64_t PipelineSequenceState::last_sequence_id() const {
    return last_sequence_id_;
}

const std::vector<RetireTraceEntry>& PipelineSequenceState::retire_trace() const {
    return retire_trace_;
}

ExecutionProfileSnapshot PipelineSequenceState::profile_snapshot() const {
    return profile_.snapshot();
}
