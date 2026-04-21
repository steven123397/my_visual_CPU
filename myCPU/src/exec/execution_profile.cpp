#include "execution_profile.h"

#include <algorithm>

namespace {

bool is_control_flow_raw(uint32_t raw) {
    const uint32_t opcode = raw & 0x7FU;
    return opcode == 0x63U || opcode == 0x67U || opcode == 0x6FU;
}

bool is_syscall_raw(uint32_t raw) {
    return raw == 0x00000073U;
}

bool is_system_boundary_raw(uint32_t raw) {
    return (raw & 0x7FU) == 0x73U && ((raw >> 12) & 0x7U) == 0;
}

const char* physical_region_kind_name(PhysicalRegionKind kind) {
    switch (kind) {
    case PhysicalRegionKind::Ram:
        return "ram";
    case PhysicalRegionKind::Mmio:
        return "mmio";
    case PhysicalRegionKind::Unmapped:
        return "unmapped";
    default:
        return "unknown";
    }
}

template <typename T, typename Compare>
std::vector<T> top_entries(std::vector<T> entries, Compare compare, size_t limit) {
    std::sort(entries.begin(), entries.end(), compare);
    if (entries.size() > limit) {
        entries.resize(limit);
    }
    return entries;
}

}  // namespace

void ExecutionProfile::clear() {
    total_retirements_ = 0;
    total_traps_ = 0;
    total_memory_observations_ = 0;
    active_path_ = {};
    hot_paths_.clear();
    branches_.clear();
    syscalls_.clear();
    traps_.clear();
    memory_regions_.clear();
}

void ExecutionProfile::record_retire(const ExecutionRetireObservation& observation) {
    if (observation.pc == 0 && observation.raw == 0 && !observation.trap && !observation.redirect) {
        return;
    }

    ++total_retirements_;

    if (!active_path_.open) {
        start_path(observation.pc);
    } else if (observation.pc != active_path_.end_pc + 4) {
        finalize_path();
        start_path(observation.pc);
    }

    active_path_.end_pc = observation.pc;
    active_path_.retired_instructions += 1;

    if (is_control_flow_raw(observation.raw)) {
        BranchStats& branch = branches_[observation.pc];
        branch.raw = observation.raw;
        branch.executions += 1;
        if (observation.redirect) {
            branch.redirects += 1;
        }
    }

    if (is_syscall_raw(observation.raw)) {
        SyscallStats& syscall = syscalls_[observation.pc];
        syscall.raw = observation.raw;
        syscall.count += 1;
    }

    if (observation.trap || observation.redirect || is_control_flow_raw(observation.raw) ||
        is_system_boundary_raw(observation.raw)) {
        finalize_path();
    }
}

void ExecutionProfile::record_trap(const ExecutionTrapObservation& observation) {
    ++total_traps_;
    traps_[TrapKey{
        .pc = observation.pc,
        .raw = observation.raw,
        .cause = observation.cause,
        .privilege = observation.privilege,
        .interrupt = observation.interrupt,
    }] += 1;
    finalize_path();
}

void ExecutionProfile::record_memory(const ExecutionMemoryObservation& observation) {
    if (!observation.valid || observation.bytes == 0) {
        return;
    }

    ++total_memory_observations_;
    MemoryStats& stats = memory_regions_[MemoryKey{
        .kind = observation.region.kind,
        .cacheable = observation.region.cacheable,
        .dma_visible = observation.region.dma_visible,
        .has_side_effect = observation.region.has_side_effect,
        .supports_burst = observation.region.supports_burst,
        .label = observation.region.label != nullptr ? observation.region.label : "unmapped",
    }];
    stats.accesses += 1;
    stats.bytes += observation.bytes;
    if (observation.write) {
        stats.writes += 1;
    } else {
        stats.reads += 1;
    }
    if (observation.fault) {
        stats.faults += 1;
    }
}

ExecutionProfileSnapshot ExecutionProfile::snapshot() const {
    ExecutionProfileSnapshot snapshot;
    snapshot.total_retirements = total_retirements_;
    snapshot.total_traps = total_traps_;
    snapshot.total_memory_observations = total_memory_observations_;

    std::map<HotPathKey, HotPathStats> visible_hot_paths = hot_paths_;
    if (active_path_.open && active_path_.retired_instructions != 0) {
        HotPathStats& visible = visible_hot_paths[HotPathKey{
            .start_pc = active_path_.start_pc,
            .end_pc = active_path_.end_pc,
        }];
        visible.executions += 1;
        visible.retired_instructions += active_path_.retired_instructions;
    }

    std::vector<ExecutionHotPathEntry> hot_paths;
    hot_paths.reserve(visible_hot_paths.size());
    for (const auto& [key, stats] : visible_hot_paths) {
        hot_paths.push_back(ExecutionHotPathEntry{
            .start_pc = key.start_pc,
            .end_pc = key.end_pc,
            .executions = stats.executions,
            .retired_instructions = stats.retired_instructions,
        });
    }
    snapshot.hot_paths = top_entries(
        std::move(hot_paths),
        [](const ExecutionHotPathEntry& lhs, const ExecutionHotPathEntry& rhs) {
            if (lhs.executions != rhs.executions) {
                return lhs.executions > rhs.executions;
            }
            if (lhs.retired_instructions != rhs.retired_instructions) {
                return lhs.retired_instructions > rhs.retired_instructions;
            }
            if (lhs.start_pc != rhs.start_pc) {
                return lhs.start_pc < rhs.start_pc;
            }
            return lhs.end_pc < rhs.end_pc;
        },
        kSnapshotEntryLimit);

    std::vector<ExecutionBranchEntry> branches;
    branches.reserve(branches_.size());
    for (const auto& [pc, stats] : branches_) {
        branches.push_back(ExecutionBranchEntry{
            .pc = pc,
            .raw = stats.raw,
            .executions = stats.executions,
            .redirects = stats.redirects,
        });
    }
    snapshot.branches = top_entries(
        std::move(branches),
        [](const ExecutionBranchEntry& lhs, const ExecutionBranchEntry& rhs) {
            if (lhs.executions != rhs.executions) {
                return lhs.executions > rhs.executions;
            }
            if (lhs.redirects != rhs.redirects) {
                return lhs.redirects > rhs.redirects;
            }
            return lhs.pc < rhs.pc;
        },
        kSnapshotEntryLimit);

    std::vector<ExecutionSyscallEntry> syscalls;
    syscalls.reserve(syscalls_.size());
    for (const auto& [pc, stats] : syscalls_) {
        syscalls.push_back(ExecutionSyscallEntry{
            .pc = pc,
            .raw = stats.raw,
            .count = stats.count,
        });
    }
    snapshot.syscalls = top_entries(
        std::move(syscalls),
        [](const ExecutionSyscallEntry& lhs, const ExecutionSyscallEntry& rhs) {
            if (lhs.count != rhs.count) {
                return lhs.count > rhs.count;
            }
            return lhs.pc < rhs.pc;
        },
        kSnapshotEntryLimit);

    std::vector<ExecutionTrapEntry> traps;
    traps.reserve(traps_.size());
    for (const auto& [key, count] : traps_) {
        traps.push_back(ExecutionTrapEntry{
            .pc = key.pc,
            .raw = key.raw,
            .cause = key.cause,
            .privilege = key.privilege,
            .interrupt = key.interrupt,
            .count = count,
        });
    }
    snapshot.traps = top_entries(
        std::move(traps),
        [](const ExecutionTrapEntry& lhs, const ExecutionTrapEntry& rhs) {
            if (lhs.count != rhs.count) {
                return lhs.count > rhs.count;
            }
            if (lhs.pc != rhs.pc) {
                return lhs.pc < rhs.pc;
            }
            return lhs.cause < rhs.cause;
        },
        kSnapshotEntryLimit);

    std::vector<ExecutionMemoryRegionEntry> memory_regions;
    memory_regions.reserve(memory_regions_.size());
    for (const auto& [key, stats] : memory_regions_) {
        memory_regions.push_back(ExecutionMemoryRegionEntry{
            .label = key.label,
            .kind = physical_region_kind_name(key.kind),
            .cacheable = key.cacheable,
            .dma_visible = key.dma_visible,
            .has_side_effect = key.has_side_effect,
            .supports_burst = key.supports_burst,
            .accesses = stats.accesses,
            .reads = stats.reads,
            .writes = stats.writes,
            .faults = stats.faults,
            .bytes = stats.bytes,
        });
    }
    snapshot.memory_regions = top_entries(
        std::move(memory_regions),
        [](const ExecutionMemoryRegionEntry& lhs, const ExecutionMemoryRegionEntry& rhs) {
            if (lhs.accesses != rhs.accesses) {
                return lhs.accesses > rhs.accesses;
            }
            if (lhs.bytes != rhs.bytes) {
                return lhs.bytes > rhs.bytes;
            }
            if (lhs.label != rhs.label) {
                return lhs.label < rhs.label;
            }
            return lhs.kind < rhs.kind;
        },
        kSnapshotEntryLimit);

    return snapshot;
}

void ExecutionProfile::start_path(uint64_t pc) {
    active_path_.open = true;
    active_path_.start_pc = pc;
    active_path_.end_pc = pc;
    active_path_.retired_instructions = 0;
}

void ExecutionProfile::finalize_path() {
    if (!active_path_.open || active_path_.retired_instructions == 0) {
        active_path_ = {};
        return;
    }

    HotPathStats& stats = hot_paths_[HotPathKey{
        .start_pc = active_path_.start_pc,
        .end_pc = active_path_.end_pc,
    }];
    stats.executions += 1;
    stats.retired_instructions += active_path_.retired_instructions;
    active_path_ = {};
}
