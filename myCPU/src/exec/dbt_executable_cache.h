#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

#include "dbt_block_plan.h"
#include "dbt_host_emitter.h"
#include "dbt_runtime_dispatch.h"

struct DbtExecutableCacheDryRunStats {
    size_t entries{0};
    uint64_t lookups{0};
    uint64_t hits{0};
    uint64_t misses{0};
    uint64_t insertions{0};
    uint64_t replacements{0};
    uint64_t rejected_insertions{0};
    uint64_t invalidation_enforcements{0};
    uint64_t invalidations{0};
    uint64_t non_invalidating_events{0};
    uint64_t invalidated_entries{0};
    uint64_t stale_dispatches_prevented{0};
    uint64_t host_executables_inserted{0};
    uint64_t host_executables_released{0};
    uint64_t rejected_host_executables{0};
};

struct DbtExecutableCacheLookup {
    bool hit{false};
    DbtRuntimeDispatchContract contract{};
    bool has_host_executable{false};
    const DbtHostExecutable* executable{nullptr};
    std::string reason{};
};

struct DbtExecutableCacheInvalidationResult {
    bool invalidated{false};
    bool stale_dispatch_prevented{false};
    uint64_t entries_removed{0};
    uint64_t entries_examined{0};
    std::string reason{};
};

struct DbtExecutableCacheEntry {
    DbtRuntimeDispatchContract contract{};
    bool has_host_executable{false};
    DbtHostExecutable executable{};
};

class DbtExecutableCacheDryRun {
public:
    DbtExecutableCacheDryRun() = default;
    DbtExecutableCacheDryRun(const DbtExecutableCacheDryRun&) = delete;
    DbtExecutableCacheDryRun& operator=(const DbtExecutableCacheDryRun&) = delete;
    DbtExecutableCacheDryRun(DbtExecutableCacheDryRun&&) = delete;
    DbtExecutableCacheDryRun& operator=(DbtExecutableCacheDryRun&&) = delete;
    ~DbtExecutableCacheDryRun();

    bool insert(const DbtRuntimeDispatchContract& contract);
    DbtExecutableCacheLookup lookup(uint64_t start_pc, uint64_t end_pc);
    DbtExecutableCacheInvalidationResult enforce_invalidation(DbtInvalidationEventKind kind,
                                                              uint64_t event_addr,
                                                              uint64_t event_size);
    void clear();
    size_t size() const;
    DbtExecutableCacheDryRunStats stats() const;

protected:
    bool insert_entry(const DbtRuntimeDispatchContract& contract,
                      DbtHostExecutable* executable);

private:
    void release_entry_host_executable(DbtExecutableCacheEntry& entry);
    void release_all_host_executables();

    std::vector<DbtExecutableCacheEntry> entries_{};
    DbtExecutableCacheDryRunStats stats_{};
};

class DbtExecutableCacheRuntime : public DbtExecutableCacheDryRun {
public:
    using DbtExecutableCacheDryRun::insert;

    bool insert(const DbtRuntimeDispatchContract& contract,
                DbtHostExecutable& executable);
};
