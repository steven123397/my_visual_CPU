#include "pipeline_sequence.h"

#include <utility>

uint64_t PipelineSequenceState::allocate() {
    last_sequence_id_ += 1;
    return last_sequence_id_;
}

void PipelineSequenceState::clear() {
    last_sequence_id_ = 0;
    retire_trace_.clear();
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
}

uint64_t PipelineSequenceState::last_sequence_id() const {
    return last_sequence_id_;
}

const std::vector<RetireTraceEntry>& PipelineSequenceState::retire_trace() const {
    return retire_trace_;
}
