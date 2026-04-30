#include "dbt_executable_cache.h"

#include <utility>

namespace {

bool same_key(const DbtRuntimeDispatchContract& contract,
              uint64_t start_pc,
              uint64_t end_pc) {
    return contract.start_pc == start_pc && contract.end_pc == end_pc;
}

bool cacheable_lowered_contract(const DbtRuntimeDispatchContract& contract) {
    return contract.ok &&
           contract.kind == DbtRuntimeDispatchKind::LoweredBlock &&
           contract.can_enter_lowered_block &&
           !contract.requires_helper_bridge &&
           !contract.reference_step_required &&
           contract.dry_run_only &&
           !contract.generated_host_code &&
           !contract.requested_executable_memory &&
           !contract.executed_guest_code;
}

std::string empty_event_reason(DbtInvalidationEventKind kind) {
    const DbtInvalidationPlan plan = plan_dbt_block_invalidation_event(kind, 0, 0, 0, 0);
    if (!plan.reason.empty()) {
        return plan.reason;
    }
    return "empty-executable-cache";
}

}  // namespace

bool DbtExecutableCacheDryRun::insert(const DbtRuntimeDispatchContract& contract) {
    if (!cacheable_lowered_contract(contract)) {
        stats_.rejected_insertions += 1;
        return false;
    }

    for (DbtRuntimeDispatchContract& entry : entries_) {
        if (same_key(entry, contract.start_pc, contract.end_pc)) {
            entry = contract;
            stats_.insertions += 1;
            stats_.replacements += 1;
            return true;
        }
    }

    entries_.push_back(contract);
    stats_.insertions += 1;
    return true;
}

DbtExecutableCacheLookup DbtExecutableCacheDryRun::lookup(uint64_t start_pc, uint64_t end_pc) {
    stats_.lookups += 1;
    for (const DbtRuntimeDispatchContract& entry : entries_) {
        if (same_key(entry, start_pc, end_pc)) {
            stats_.hits += 1;
            return DbtExecutableCacheLookup{
                .hit = true,
                .contract = entry,
                .reason = "hit",
            };
        }
    }

    stats_.misses += 1;
    return DbtExecutableCacheLookup{
        .hit = false,
        .reason = "miss",
    };
}

DbtExecutableCacheInvalidationResult DbtExecutableCacheDryRun::enforce_invalidation(
    DbtInvalidationEventKind kind,
    uint64_t event_addr,
    uint64_t event_size) {
    stats_.invalidation_enforcements += 1;
    DbtExecutableCacheInvalidationResult result{
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
    result.stale_dispatch_prevented = result.invalidated;
    if (result.invalidated) {
        stats_.invalidations += 1;
        stats_.invalidated_entries += result.entries_removed;
        stats_.stale_dispatches_prevented += result.entries_removed;
    } else {
        stats_.non_invalidating_events += 1;
        if (result.reason.empty()) {
            result.reason = std::move(non_invalidating_reason);
        }
    }

    return result;
}

void DbtExecutableCacheDryRun::clear() {
    entries_.clear();
}

size_t DbtExecutableCacheDryRun::size() const {
    return entries_.size();
}

DbtExecutableCacheDryRunStats DbtExecutableCacheDryRun::stats() const {
    DbtExecutableCacheDryRunStats copy = stats_;
    copy.entries = entries_.size();
    return copy;
}
