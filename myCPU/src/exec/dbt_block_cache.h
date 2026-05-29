#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

#include "dbt_block_plan.h"
#include "dbt_ir.h"

struct DbtBlockCacheStats {
    size_t entries{0};
    size_t max_entries{0};
    uint64_t lookups{0};
    uint64_t hits{0};
    uint64_t misses{0};
    uint64_t insertions{0};
    uint64_t evictions{0};
    uint64_t rejected_insertions{0};
    uint64_t invalidation_checks{0};
    uint64_t invalidations{0};
    uint64_t non_invalidating_events{0};
    uint64_t invalidated_entries{0};
};

struct DbtBlockCacheInvalidationResult {
    bool invalidated{false};
    uint64_t entries_removed{0};
    uint64_t entries_examined{0};
    std::string reason{};
};

class DbtBlockCache {
public:
    static constexpr size_t kMaxEntries = 64;

    bool insert(const DbtTranslationUnit& unit);
    const DbtTranslationUnit* lookup(uint64_t start_pc, uint64_t end_pc);
    DbtBlockCacheInvalidationResult invalidate(DbtInvalidationEventKind kind,
                                               uint64_t event_addr,
                                               uint64_t event_size);
    void clear();
    size_t size() const;
    DbtBlockCacheStats stats() const;

private:
    std::vector<DbtTranslationUnit> entries_{};
    DbtBlockCacheStats stats_{};
};
