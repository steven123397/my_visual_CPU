#include "dbt_runtime_invalidation.h"

#include <sstream>

namespace {

const char* bool_name(bool value) {
    return value ? "true" : "false";
}

}  // namespace

DbtRuntimeInvalidationHookResult apply_dbt_runtime_invalidation_hook(
    DbtExecutableCacheDryRun& cache,
    const DbtRuntimeInvalidationEvent& event) {
    const DbtExecutableCacheInvalidationResult enforced =
        cache.enforce_invalidation(event.kind, event.addr, event.size);

    return DbtRuntimeInvalidationHookResult{
        .ok = true,
        .event_kind = event.kind,
        .enforced = true,
        .invalidated = enforced.invalidated,
        .stale_dispatch_prevented = enforced.stale_dispatch_prevented,
        .entries_removed = enforced.entries_removed,
        .entries_examined = enforced.entries_examined,
        .reason = enforced.reason.empty() ? "none" : enforced.reason,
        .dry_run_only = true,
        .mutates_cpu_state = false,
        .generated_host_code = false,
        .executed_guest_code = false,
    };
}

const char* dbt_invalidation_event_kind_name(DbtInvalidationEventKind kind) {
    switch (kind) {
    case DbtInvalidationEventKind::PrimaryImageLoad:
        return "primary-image-load";
    case DbtInvalidationEventKind::DebugReset:
        return "debug-reset";
    case DbtInvalidationEventKind::PayloadLoad:
        return "payload-load";
    case DbtInvalidationEventKind::GuestStore:
        return "guest-store";
    case DbtInvalidationEventKind::SatpWrite:
        return "satp-write";
    case DbtInvalidationEventKind::SfenceVma:
        return "sfence-vma";
    case DbtInvalidationEventKind::RegionAttributesChanged:
        return "region-attributes-changed";
    }
    return "unknown";
}

std::string format_dbt_runtime_invalidation_hook_result(
    const DbtRuntimeInvalidationHookResult& result) {
    std::ostringstream out;
    out << "runtime-invalidation:"
        << " kind=" << dbt_invalidation_event_kind_name(result.event_kind)
        << " ok=" << bool_name(result.ok)
        << " enforced=" << bool_name(result.enforced)
        << " invalidated=" << bool_name(result.invalidated)
        << " removed=" << result.entries_removed
        << " examined=" << result.entries_examined
        << " stale-prevented=" << bool_name(result.stale_dispatch_prevented)
        << " reason=" << (result.reason.empty() ? "none" : result.reason)
        << " dry-run-only=" << bool_name(result.dry_run_only)
        << " mutates-state=" << bool_name(result.mutates_cpu_state)
        << " host-code=" << bool_name(result.generated_host_code)
        << " guest-exec=" << bool_name(result.executed_guest_code);
    return out.str();
}
