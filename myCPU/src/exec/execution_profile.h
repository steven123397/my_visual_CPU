#pragma once

#include <cstdint>
#include <list>
#include <map>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

#include "../arch/core_state.h"
#include "../isa/atomic_contract.h"
#include "../mem/memory_region.h"
#include "pipeline_commit_boundary.h"

struct ExecutionRetireObservation {
    uint64_t pc{0};
    uint32_t raw{0};
    bool trap{false};
    bool redirect{false};
    bool cycle_valid{false};
    uint64_t cycle{0};
    bool target_pc_valid{false};
    uint64_t target_pc{0};
};

struct ExecutionTrapObservation {
    uint64_t pc{0};
    uint32_t raw{0};
    uint64_t cause{0};
    PrivilegeMode privilege{PrivilegeMode::Machine};
    bool interrupt{false};
};

struct ExecutionMemoryObservation {
    bool valid{false};
    bool pc_valid{false};
    uint64_t pc{0};
    uint32_t raw{0};
    PhysicalRegionInfo region{};
    bool write{false};
    bool fault{false};
    bool paddr_valid{false};
    uint64_t paddr{0};
    uint64_t bytes{0};
};

struct ExecutionHotPathEntry {
    uint64_t start_pc{0};
    uint64_t end_pc{0};
    uint64_t executions{0};
    uint64_t retired_instructions{0};
};

struct ExecutionBranchEntry {
    uint64_t pc{0};
    uint32_t raw{0};
    uint64_t executions{0};
    uint64_t redirects{0};
};

struct ExecutionBranchTargetEntry {
    uint64_t pc{0};
    uint32_t raw{0};
    uint64_t target_pc{0};
    uint64_t executions{0};
    uint64_t redirects{0};
};

struct ExecutionSyscallEntry {
    uint64_t pc{0};
    uint32_t raw{0};
    uint64_t count{0};
};

struct ExecutionTrapEntry {
    uint64_t pc{0};
    uint32_t raw{0};
    uint64_t cause{0};
    PrivilegeMode privilege{PrivilegeMode::Machine};
    bool interrupt{false};
    uint64_t count{0};
};

struct ExecutionMemoryRegionEntry {
    std::string label{};
    std::string kind{};
    bool cacheable{false};
    bool dma_visible{false};
    bool has_side_effect{false};
    bool supports_burst{false};
    uint64_t accesses{0};
    uint64_t reads{0};
    uint64_t writes{0};
    uint64_t faults{0};
    uint64_t bytes{0};
    uint64_t shadow_cache_line_accesses{0};
    uint64_t shadow_cache_hits{0};
    uint64_t shadow_cache_misses{0};
    uint64_t shadow_cache_evictions{0};
    uint64_t shadow_cache_bypasses{0};
};

struct ExecutionPcCostEntry {
    uint64_t pc{0};
    uint32_t raw{0};
    uint64_t retirements{0};
    uint64_t cycles{0};
    uint64_t memory_observations{0};
    uint64_t memory_reads{0};
    uint64_t memory_writes{0};
    uint64_t memory_faults{0};
    uint64_t memory_bytes{0};
};

struct ExecutionShadowCacheSnapshot {
    uint64_t line_size_bytes{0};
    uint64_t capacity_lines{0};
    uint64_t resident_lines{0};
    uint64_t line_accesses{0};
    uint64_t hits{0};
    uint64_t misses{0};
    uint64_t evictions{0};
    uint64_t bypasses{0};
};

struct ExecutionProfileSnapshot {
    uint64_t total_retirements{0};
    uint64_t total_traps{0};
    uint64_t total_memory_observations{0};
    std::vector<ExecutionHotPathEntry> hot_paths{};
    std::vector<ExecutionBranchEntry> branches{};
    std::vector<ExecutionBranchTargetEntry> branch_targets{};
    std::vector<ExecutionSyscallEntry> syscalls{};
    std::vector<ExecutionTrapEntry> traps{};
    ExecutionShadowCacheSnapshot shadow_cache{};
    std::vector<ExecutionMemoryRegionEntry> memory_regions{};
    std::vector<ExecutionPcCostEntry> pc_costs{};
};

class Bus;

ExecutionMemoryObservation fault_memory_observation(uint64_t pc,
                                                    uint32_t raw,
                                                    bool write,
                                                    uint64_t bytes);
PhysicalRegionInfo observed_region(const Bus& bus, uint64_t paddr, uint64_t bytes);
std::optional<ExecutionMemoryObservation> make_atomic_memory_observation(
    const Bus& bus,
    const AtomicRequest& request,
    uint64_t pc,
    uint32_t raw,
    const CommitBoundaryResult& result);

class ExecutionProfile {
public:
    void clear();
    void record_retire(const ExecutionRetireObservation& observation);
    void record_trap(const ExecutionTrapObservation& observation);
    void record_memory(const ExecutionMemoryObservation& observation);
    ExecutionProfileSnapshot snapshot() const;

private:
    struct HotPathKey {
        uint64_t start_pc{0};
        uint64_t end_pc{0};

        bool operator<(const HotPathKey& other) const {
            if (start_pc != other.start_pc) {
                return start_pc < other.start_pc;
            }
            return end_pc < other.end_pc;
        }
    };

    struct HotPathStats {
        uint64_t executions{0};
        uint64_t retired_instructions{0};
    };

    struct BranchStats {
        uint32_t raw{0};
        uint64_t executions{0};
        uint64_t redirects{0};
    };

    struct BranchTargetKey {
        uint64_t pc{0};
        uint64_t target_pc{0};

        bool operator<(const BranchTargetKey& other) const {
            if (pc != other.pc) {
                return pc < other.pc;
            }
            return target_pc < other.target_pc;
        }
    };

    struct BranchTargetStats {
        uint32_t raw{0};
        uint64_t executions{0};
        uint64_t redirects{0};
    };

    struct SyscallStats {
        uint32_t raw{0};
        uint64_t count{0};
    };

    struct TrapKey {
        uint64_t pc{0};
        uint32_t raw{0};
        uint64_t cause{0};
        PrivilegeMode privilege{PrivilegeMode::Machine};
        bool interrupt{false};

        bool operator<(const TrapKey& other) const {
            if (pc != other.pc) {
                return pc < other.pc;
            }
            if (raw != other.raw) {
                return raw < other.raw;
            }
            if (cause != other.cause) {
                return cause < other.cause;
            }
            if (privilege != other.privilege) {
                return static_cast<uint8_t>(privilege) < static_cast<uint8_t>(other.privilege);
            }
            return interrupt < other.interrupt;
        }
    };

    struct MemoryKey {
        PhysicalRegionKind kind{PhysicalRegionKind::Unmapped};
        bool cacheable{false};
        bool dma_visible{false};
        bool has_side_effect{false};
        bool supports_burst{false};
        std::string label{};

        bool operator<(const MemoryKey& other) const {
            if (kind != other.kind) {
                return static_cast<uint8_t>(kind) < static_cast<uint8_t>(other.kind);
            }
            if (cacheable != other.cacheable) {
                return cacheable < other.cacheable;
            }
            if (dma_visible != other.dma_visible) {
                return dma_visible < other.dma_visible;
            }
            if (has_side_effect != other.has_side_effect) {
                return has_side_effect < other.has_side_effect;
            }
            if (supports_burst != other.supports_burst) {
                return supports_burst < other.supports_burst;
            }
            return label < other.label;
        }
    };

    struct MemoryStats {
        uint64_t accesses{0};
        uint64_t reads{0};
        uint64_t writes{0};
        uint64_t faults{0};
        uint64_t bytes{0};
    };

    struct PcCostStats {
        uint32_t raw{0};
        uint64_t retirements{0};
        uint64_t cycles{0};
        uint64_t memory_observations{0};
        uint64_t memory_reads{0};
        uint64_t memory_writes{0};
        uint64_t memory_faults{0};
        uint64_t memory_bytes{0};
    };

    struct ShadowCacheStats {
        uint64_t line_accesses{0};
        uint64_t hits{0};
        uint64_t misses{0};
        uint64_t evictions{0};
        uint64_t bypasses{0};
    };

    struct ShadowCacheLineState {
        MemoryKey key{};
        std::list<uint64_t>::iterator lru_position{};
    };

    struct ActivePath {
        bool open{false};
        uint64_t start_pc{0};
        uint64_t end_pc{0};
        uint64_t retired_instructions{0};
    };

    void start_path(uint64_t pc);
    void finalize_path();
    void record_pc_retire_cost(const ExecutionRetireObservation& observation);
    void record_branch_target(const ExecutionRetireObservation& observation);
    void record_shadow_cache(const MemoryKey& key, const ExecutionMemoryObservation& observation);
    void shadow_cache_touch_line(const MemoryKey& key, uint64_t line_addr);
    void shadow_cache_evict_lru();

    static constexpr size_t kSnapshotEntryLimit = 8;
    static constexpr uint64_t kShadowCacheLineSize = 64;
    static constexpr size_t kShadowCacheCapacityLines = 64;

    uint64_t total_retirements_{0};
    uint64_t total_traps_{0};
    uint64_t total_memory_observations_{0};
    bool last_retire_cycle_valid_{false};
    uint64_t last_retire_cycle_{0};
    ActivePath active_path_{};
    std::map<HotPathKey, HotPathStats> hot_paths_{};
    std::map<uint64_t, BranchStats> branches_{};
    std::map<BranchTargetKey, BranchTargetStats> branch_targets_{};
    std::map<uint64_t, SyscallStats> syscalls_{};
    std::map<TrapKey, uint64_t> traps_{};
    std::map<MemoryKey, MemoryStats> memory_regions_{};
    std::map<uint64_t, PcCostStats> pc_costs_{};
    std::map<MemoryKey, ShadowCacheStats> shadow_cache_regions_{};
    std::list<uint64_t> shadow_cache_lru_{};
    std::unordered_map<uint64_t, ShadowCacheLineState> shadow_cache_lines_{};
    ShadowCacheStats shadow_cache_summary_{};
};
