#include "dbt_block_cache.h"

#include <utility>

namespace {

bool same_key(const DbtTranslationUnit& unit, uint64_t start_pc, uint64_t end_pc) {
    return unit.start_pc == start_pc && unit.end_pc == end_pc;
}

std::string empty_event_reason(DbtInvalidationEventKind kind) {
    const DbtInvalidationPlan plan = plan_dbt_block_invalidation_event(kind, 0, 0, 0, 0);
    if (!plan.reason.empty()) {
        return plan.reason;
    }
    return "empty-cache";
}

}  // namespace

bool DbtBlockCache::insert(const DbtTranslationUnit& unit) {
    if (!unit.ok) {
        stats_.rejected_insertions += 1;
        return false;
    }

    for (DbtTranslationUnit& entry : entries_) {
        if (same_key(entry, unit.start_pc, unit.end_pc)) {
            entry = unit;
            stats_.insertions += 1;
            return true;
        }
    }

    entries_.push_back(unit);
    stats_.insertions += 1;
    return true;
}

const DbtTranslationUnit* DbtBlockCache::lookup(uint64_t start_pc, uint64_t end_pc) {
    stats_.lookups += 1;
    for (const DbtTranslationUnit& entry : entries_) {
        if (same_key(entry, start_pc, end_pc)) {
            stats_.hits += 1;
            return &entry;
        }
    }
    stats_.misses += 1;
    return nullptr;
}

DbtBlockCacheInvalidationResult DbtBlockCache::invalidate(DbtInvalidationEventKind kind,
                                                          uint64_t event_addr,
                                                          uint64_t event_size) {
    stats_.invalidation_checks += 1;
    DbtBlockCacheInvalidationResult result{
        .reason = entries_.empty() ? empty_event_reason(kind) : "",
    };

    std::string non_invalidating_reason;
    for (auto it = entries_.begin(); it != entries_.end();) {
        result.entries_examined += 1;
        const DbtInvalidationPlan plan =
            plan_dbt_block_invalidation_event(kind,
                                              event_addr,
                                              event_size,
                                              it->start_pc,
                                              it->end_pc);
        if (plan.invalidates) {
            if (result.reason.empty()) {
                result.reason = plan.reason;
            }
            it = entries_.erase(it);
            result.entries_removed += 1;
            continue;
        }
        if (non_invalidating_reason.empty()) {
            non_invalidating_reason = plan.reason;
        }
        ++it;
    }

    result.invalidated = result.entries_removed != 0;
    if (result.invalidated) {
        stats_.invalidations += 1;
        stats_.invalidated_entries += result.entries_removed;
    } else if (result.reason.empty()) {
        result.reason = std::move(non_invalidating_reason);
    }
    if (!result.invalidated) {
        stats_.non_invalidating_events += 1;
    }

    return result;
}

void DbtBlockCache::clear() {
    entries_.clear();
}

size_t DbtBlockCache::size() const {
    return entries_.size();
}

DbtBlockCacheStats DbtBlockCache::stats() const {
    DbtBlockCacheStats copy = stats_;
    copy.entries = entries_.size();
    return copy;
}
