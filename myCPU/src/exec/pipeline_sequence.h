#pragma once

#include <cstddef>
#include <cstdint>
#include <vector>

#include "execution_profile.h"

struct SequenceId {
    uint64_t value{0};
};

struct RetireTraceEntry {
    uint64_t sequence_id{0};
    uint64_t pc{0};
    uint32_t raw{0};
    bool trap{false};
    bool redirect{false};
};

class PipelineSequenceState {
public:
    uint64_t allocate();
    void clear();
    void record(const RetireTraceEntry& entry);
    void record_memory(const ExecutionMemoryObservation& observation);
    void record_trap(const ExecutionTrapObservation& observation);

    uint64_t last_sequence_id() const;
    const std::vector<RetireTraceEntry>& retire_trace() const;
    ExecutionProfileSnapshot profile_snapshot() const;

private:
    static constexpr size_t kRetireTraceCapacity = 16;

    uint64_t last_sequence_id_{0};
    std::vector<RetireTraceEntry> retire_trace_{};
    ExecutionProfile profile_{};
};
