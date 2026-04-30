#pragma once

#include <cstdint>
#include <string>

#include "dbt_executable_cache.h"

struct DbtRuntimeInvalidationEvent {
    DbtInvalidationEventKind kind{DbtInvalidationEventKind::GuestStore};
    uint64_t addr{0};
    uint64_t size{0};
};

struct DbtRuntimeInvalidationHookResult {
    bool ok{false};
    DbtInvalidationEventKind event_kind{DbtInvalidationEventKind::GuestStore};
    bool enforced{false};
    bool invalidated{false};
    bool stale_dispatch_prevented{false};
    uint64_t entries_removed{0};
    uint64_t entries_examined{0};
    std::string reason{};
    bool dry_run_only{true};
    bool mutates_cpu_state{false};
    bool generated_host_code{false};
    bool executed_guest_code{false};
};

DbtRuntimeInvalidationHookResult apply_dbt_runtime_invalidation_hook(
    DbtExecutableCacheDryRun& cache,
    const DbtRuntimeInvalidationEvent& event);

const char* dbt_invalidation_event_kind_name(DbtInvalidationEventKind kind);
std::string format_dbt_runtime_invalidation_hook_result(
    const DbtRuntimeInvalidationHookResult& result);
