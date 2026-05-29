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

bool ranges_overlap(uint64_t lhs_addr, uint64_t lhs_size, uint64_t rhs_addr, uint64_t rhs_size) {
    if (lhs_size == 0 || rhs_size == 0) {
        return false;
    }
    const uint64_t lhs_end = lhs_addr + lhs_size;
    const uint64_t rhs_end = rhs_addr + rhs_size;
    return lhs_addr < rhs_end && rhs_addr < lhs_end;
}

bool range_event(DbtInvalidationEventKind kind) {
    return kind == DbtInvalidationEventKind::GuestStore ||
           kind == DbtInvalidationEventKind::PayloadLoad;
}

DbtInvalidationPlan plan_physical_span_invalidation(
    DbtInvalidationEventKind kind,
    uint64_t event_addr,
    uint64_t event_size,
    const DbtRuntimeDispatchContract& contract) {
    if (!range_event(kind)) {
        return DbtInvalidationPlan{.reason = "not-range-event"};
    }

    for (const DbtCodePhysicalSpan& span : contract.code_spans) {
        if (span.requires_global_invalidation) {
            return DbtInvalidationPlan{
                .invalidates = true,
                .reason = "code-span-unreliable",
            };
        }
        if (!span.valid) {
            return DbtInvalidationPlan{
                .invalidates = true,
                .reason = "code-span-unreliable",
            };
        }
        if (ranges_overlap(event_addr, event_size, span.paddr, span.size)) {
            return DbtInvalidationPlan{
                .invalidates = true,
                .reason = kind == DbtInvalidationEventKind::GuestStore
                              ? "guest-store-overlaps-physical-code"
                              : "payload-overlaps-physical-code",
            };
        }
    }

    return DbtInvalidationPlan{.reason = "physical-range-disjoint"};
}

std::string empty_event_reason(DbtInvalidationEventKind kind) {
    const DbtInvalidationPlan plan = plan_dbt_block_invalidation_event(kind, 0, 0, 0, 0);
    if (!plan.reason.empty()) {
        return plan.reason;
    }
    return "empty-executable-cache";
}

bool cacheable_host_executable(const DbtHostExecutable& executable) {
    return executable.ok &&
           executable.generated_host_code &&
           executable.requested_executable_memory &&
           executable.memory.allocated &&
           executable.memory.data != nullptr &&
           executable.memory.executable &&
           !executable.executed_guest_code;
}

DbtHostExecutable take_host_executable(DbtHostExecutable& executable) {
    DbtHostExecutable owned = executable;
    executable.memory = DbtExecutableMemoryBlock{
        .released = true,
    };
    return owned;
}

}  // namespace

DbtExecutableCacheDryRun::~DbtExecutableCacheDryRun() {
    release_all_host_executables();
}

bool DbtExecutableCacheDryRun::insert(const DbtRuntimeDispatchContract& contract) {
    return insert_entry(contract, nullptr);
}

bool DbtExecutableCacheDryRun::insert_entry(const DbtRuntimeDispatchContract& contract,
                                            DbtHostExecutable* executable) {
    if (!cacheable_lowered_contract(contract)) {
        stats_.rejected_insertions += 1;
        return false;
    }
    if (executable != nullptr && !cacheable_host_executable(*executable)) {
        stats_.rejected_insertions += 1;
        stats_.rejected_host_executables += 1;
        return false;
    }

    DbtExecutableCacheEntry next{
        .contract = contract,
    };
    if (executable != nullptr) {
        next.has_host_executable = true;
        next.executable = take_host_executable(*executable);
    }

    for (DbtExecutableCacheEntry& entry : entries_) {
        if (same_key(entry.contract, contract.start_pc, contract.end_pc)) {
            release_entry_host_executable(entry);
            entry = next;
            stats_.insertions += 1;
            stats_.replacements += 1;
            if (entry.has_host_executable) {
                stats_.host_executables_inserted += 1;
            }
            return true;
        }
    }

    if (entries_.size() >= kMaxEntries) {
        release_entry_host_executable(entries_.front());
        entries_.erase(entries_.begin());
        stats_.evictions += 1;
    }
    entries_.push_back(next);
    stats_.insertions += 1;
    if (next.has_host_executable) {
        stats_.host_executables_inserted += 1;
    }
    return true;
}

DbtExecutableCacheLookup DbtExecutableCacheDryRun::lookup(uint64_t start_pc, uint64_t end_pc) {
    stats_.lookups += 1;
    for (const DbtExecutableCacheEntry& entry : entries_) {
        if (same_key(entry.contract, start_pc, end_pc)) {
            stats_.hits += 1;
            return DbtExecutableCacheLookup{
                .hit = true,
                .contract = entry.contract,
                .has_host_executable = entry.has_host_executable,
                .executable = entry.has_host_executable ? &entry.executable : nullptr,
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
                                              it->contract.start_pc,
                                              it->contract.end_pc);
        const DbtInvalidationPlan physical_plan =
            plan_physical_span_invalidation(kind, event_addr, event_size, it->contract);
        const DbtInvalidationPlan effective_plan =
            plan.invalidates ? plan : physical_plan;
        if (effective_plan.invalidates) {
            if (result.reason.empty()) {
                result.reason = effective_plan.reason;
            }
            release_entry_host_executable(*it);
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
    release_all_host_executables();
    entries_.clear();
}

size_t DbtExecutableCacheDryRun::size() const {
    return entries_.size();
}

DbtExecutableCacheDryRunStats DbtExecutableCacheDryRun::stats() const {
    DbtExecutableCacheDryRunStats copy = stats_;
    copy.entries = entries_.size();
    copy.max_entries = kMaxEntries;
    return copy;
}

void DbtExecutableCacheDryRun::release_entry_host_executable(
    DbtExecutableCacheEntry& entry) {
    if (!entry.has_host_executable) {
        return;
    }
    if (entry.executable.memory.allocated && entry.executable.memory.data != nullptr) {
        release_dbt_host_executable(entry.executable);
    }
    entry.has_host_executable = false;
    stats_.host_executables_released += 1;
}

void DbtExecutableCacheDryRun::release_all_host_executables() {
    for (DbtExecutableCacheEntry& entry : entries_) {
        release_entry_host_executable(entry);
    }
}

bool DbtExecutableCacheRuntime::insert(const DbtRuntimeDispatchContract& contract,
                                       DbtHostExecutable& executable) {
    return insert_entry(contract, &executable);
}
