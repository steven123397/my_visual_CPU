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
    last_retire_cycle_valid_ = false;
    last_retire_cycle_ = 0;
    active_path_ = {};
    hot_paths_.clear();
    branches_.clear();
    branch_targets_.clear();
    syscalls_.clear();
    traps_.clear();
    memory_regions_.clear();
    pc_costs_.clear();
    shadow_cache_regions_.clear();
    shadow_cache_lru_.clear();
    shadow_cache_lines_.clear();
    shadow_cache_summary_ = {};
}

void ExecutionProfile::record_retire(const ExecutionRetireObservation& observation) {
    if (observation.pc == 0 && observation.raw == 0 && !observation.trap && !observation.redirect) {
        return;
    }

    ++total_retirements_;
    record_pc_retire_cost(observation);

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
        record_branch_target(observation);
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
    const MemoryKey key{
        .kind = observation.region.kind,
        .cacheable = observation.region.cacheable,
        .dma_visible = observation.region.dma_visible,
        .has_side_effect = observation.region.has_side_effect,
        .supports_burst = observation.region.supports_burst,
        .label = observation.region.label != nullptr ? observation.region.label : "unmapped",
    };

    MemoryStats& stats = memory_regions_[key];
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

    if (observation.pc_valid) {
        PcCostStats& pc_cost = pc_costs_[observation.pc];
        pc_cost.raw = observation.raw;
        pc_cost.memory_observations += 1;
        pc_cost.memory_bytes += observation.bytes;
        if (observation.write) {
            pc_cost.memory_writes += 1;
        } else {
            pc_cost.memory_reads += 1;
        }
        if (observation.fault) {
            pc_cost.memory_faults += 1;
        }
    }

    record_shadow_cache(key, observation);
}

ExecutionProfileSnapshot ExecutionProfile::snapshot() const {
    ExecutionProfileSnapshot snapshot;
    snapshot.total_retirements = total_retirements_;
    snapshot.total_traps = total_traps_;
    snapshot.total_memory_observations = total_memory_observations_;
    snapshot.shadow_cache = ExecutionShadowCacheSnapshot{
        .line_size_bytes = kShadowCacheLineSize,
        .capacity_lines = kShadowCacheCapacityLines,
        .resident_lines = static_cast<uint64_t>(shadow_cache_lines_.size()),
        .line_accesses = shadow_cache_summary_.line_accesses,
        .hits = shadow_cache_summary_.hits,
        .misses = shadow_cache_summary_.misses,
        .evictions = shadow_cache_summary_.evictions,
        .bypasses = shadow_cache_summary_.bypasses,
    };

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

    std::vector<ExecutionBranchTargetEntry> branch_targets;
    branch_targets.reserve(branch_targets_.size());
    for (const auto& [key, stats] : branch_targets_) {
        branch_targets.push_back(ExecutionBranchTargetEntry{
            .pc = key.pc,
            .raw = stats.raw,
            .target_pc = key.target_pc,
            .executions = stats.executions,
            .redirects = stats.redirects,
        });
    }
    snapshot.branch_targets = top_entries(
        std::move(branch_targets),
        [](const ExecutionBranchTargetEntry& lhs, const ExecutionBranchTargetEntry& rhs) {
            if (lhs.executions != rhs.executions) {
                return lhs.executions > rhs.executions;
            }
            if (lhs.redirects != rhs.redirects) {
                return lhs.redirects > rhs.redirects;
            }
            if (lhs.pc != rhs.pc) {
                return lhs.pc < rhs.pc;
            }
            return lhs.target_pc < rhs.target_pc;
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
        const auto shadow_it = shadow_cache_regions_.find(key);
        const ShadowCacheStats shadow = shadow_it != shadow_cache_regions_.end()
                                            ? shadow_it->second
                                            : ShadowCacheStats{};
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
            .shadow_cache_line_accesses = shadow.line_accesses,
            .shadow_cache_hits = shadow.hits,
            .shadow_cache_misses = shadow.misses,
            .shadow_cache_evictions = shadow.evictions,
            .shadow_cache_bypasses = shadow.bypasses,
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

    std::vector<ExecutionPcCostEntry> pc_costs;
    pc_costs.reserve(pc_costs_.size());
    for (const auto& [pc, stats] : pc_costs_) {
        pc_costs.push_back(ExecutionPcCostEntry{
            .pc = pc,
            .raw = stats.raw,
            .retirements = stats.retirements,
            .cycles = stats.cycles,
            .memory_observations = stats.memory_observations,
            .memory_reads = stats.memory_reads,
            .memory_writes = stats.memory_writes,
            .memory_faults = stats.memory_faults,
            .memory_bytes = stats.memory_bytes,
        });
    }
    snapshot.pc_costs = top_entries(
        std::move(pc_costs),
        [](const ExecutionPcCostEntry& lhs, const ExecutionPcCostEntry& rhs) {
            if (lhs.cycles != rhs.cycles) {
                return lhs.cycles > rhs.cycles;
            }
            if (lhs.memory_bytes != rhs.memory_bytes) {
                return lhs.memory_bytes > rhs.memory_bytes;
            }
            if (lhs.memory_observations != rhs.memory_observations) {
                return lhs.memory_observations > rhs.memory_observations;
            }
            if (lhs.retirements != rhs.retirements) {
                return lhs.retirements > rhs.retirements;
            }
            return lhs.pc < rhs.pc;
        },
        kSnapshotEntryLimit);

    return snapshot;
}

void ExecutionProfile::record_pc_retire_cost(const ExecutionRetireObservation& observation) {
    PcCostStats& stats = pc_costs_[observation.pc];
    stats.raw = observation.raw;
    stats.retirements += 1;
    uint64_t cycles = 1;
    if (observation.cycle_valid) {
        if (last_retire_cycle_valid_) {
            cycles = observation.cycle > last_retire_cycle_
                         ? observation.cycle - last_retire_cycle_
                         : 1;
        } else {
            cycles = observation.cycle + 1;
        }
        last_retire_cycle_valid_ = true;
        last_retire_cycle_ = observation.cycle;
    }
    stats.cycles += cycles;
}

void ExecutionProfile::record_branch_target(const ExecutionRetireObservation& observation) {
    if (!observation.target_pc_valid) {
        return;
    }
    BranchTargetStats& stats = branch_targets_[BranchTargetKey{
        .pc = observation.pc,
        .target_pc = observation.target_pc,
    }];
    stats.raw = observation.raw;
    stats.executions += 1;
    if (observation.redirect) {
        stats.redirects += 1;
    }
}

void ExecutionProfile::record_shadow_cache(const MemoryKey& key,
                                           const ExecutionMemoryObservation& observation) {
    ShadowCacheStats& stats = shadow_cache_regions_[key];

    if (!observation.paddr_valid || observation.fault || !observation.region.cacheable) {
        ++stats.bypasses;
        ++shadow_cache_summary_.bypasses;
        return;
    }

    const uint64_t start_line = observation.paddr / kShadowCacheLineSize;
    const uint64_t end_line = (observation.paddr + observation.bytes - 1) / kShadowCacheLineSize;
    for (uint64_t line = start_line; line <= end_line; ++line) {
        shadow_cache_touch_line(key, line * kShadowCacheLineSize);
        ++stats.line_accesses;
        ++shadow_cache_summary_.line_accesses;
    }
}

void ExecutionProfile::shadow_cache_touch_line(const MemoryKey& key, uint64_t line_addr) {
    const auto existing = shadow_cache_lines_.find(line_addr);
    if (existing != shadow_cache_lines_.end()) {
        ++shadow_cache_summary_.hits;
        ++shadow_cache_regions_[key].hits;
        shadow_cache_lru_.splice(shadow_cache_lru_.begin(), shadow_cache_lru_, existing->second.lru_position);
        existing->second.lru_position = shadow_cache_lru_.begin();
        return;
    }

    ++shadow_cache_summary_.misses;
    ++shadow_cache_regions_[key].misses;

    if (shadow_cache_lines_.size() >= kShadowCacheCapacityLines) {
        shadow_cache_evict_lru();
    }

    shadow_cache_lru_.push_front(line_addr);
    shadow_cache_lines_[line_addr] = ShadowCacheLineState{
        .key = key,
        .lru_position = shadow_cache_lru_.begin(),
    };
}

void ExecutionProfile::shadow_cache_evict_lru() {
    if (shadow_cache_lru_.empty()) {
        return;
    }

    const uint64_t line_addr = shadow_cache_lru_.back();
    const auto line_it = shadow_cache_lines_.find(line_addr);
    if (line_it != shadow_cache_lines_.end()) {
        ++shadow_cache_summary_.evictions;
        ++shadow_cache_regions_[line_it->second.key].evictions;
        shadow_cache_lines_.erase(line_it);
    }
    shadow_cache_lru_.pop_back();
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
