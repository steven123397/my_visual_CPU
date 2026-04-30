#include "dbt_reference_fallback.h"

#include <cstdio>
#include <sstream>

namespace {

const char* bool_name(bool value) {
    return value ? "true" : "false";
}

std::string hex_u64(uint64_t value) {
    char buffer[32];
    std::snprintf(buffer, sizeof(buffer), "0x%llx", static_cast<unsigned long long>(value));
    return buffer;
}

DbtReferenceFallbackPlan base_plan(const DbtRuntimeDispatchContract& contract) {
    return DbtReferenceFallbackPlan{
        .source = contract.source,
        .pc = contract.reject_pc != 0 ? contract.reject_pc : contract.start_pc,
        .end_pc = contract.end_pc,
        .reference_backend = "functional-reference",
        .dry_run_only = true,
        .executed_helper = false,
        .executed_reference_step = false,
        .mutates_cpu_state = false,
        .generated_host_code = contract.generated_host_code,
        .requested_executable_memory = contract.requested_executable_memory,
        .executed_guest_code = contract.executed_guest_code,
        .helper_commit_at_boundary = contract.helper_commit_at_boundary,
        .helper_serializing = contract.helper_serializing,
        .helper_may_trap = contract.helper_may_trap,
        .helper_may_change_platform_state = contract.helper_may_change_platform_state,
        .reject_kind = contract.reject_kind,
        .reject_pc = contract.reject_pc,
        .reject_raw = contract.reject_raw,
        .reject_reason = contract.reject_reason.empty() ? "none" : contract.reject_reason,
        .helper_replay_kind = contract.helper_replay_kind,
    };
}

}  // namespace

DbtReferenceFallbackPlan plan_dbt_reference_fallback_step(
    const DbtRuntimeDispatchContract& contract) {
    DbtReferenceFallbackPlan plan = base_plan(contract);

    if (!contract.ok) {
        plan.ok = false;
        plan.kind = DbtReferenceFallbackKind::None;
        if (plan.reject_reason == "none") {
            plan.reject_reason = "invalid-runtime-dispatch-contract";
        }
        return plan;
    }

    switch (contract.kind) {
    case DbtRuntimeDispatchKind::ReferenceStep:
        if (!contract.reference_step_required) {
            plan.ok = false;
            plan.kind = DbtReferenceFallbackKind::None;
            plan.reject_reason = "reference-step-not-required";
            return plan;
        }
        plan.ok = true;
        plan.kind = DbtReferenceFallbackKind::ReferenceStep;
        plan.reference_step_required = true;
        plan.reference_step_count = 1;
        return plan;
    case DbtRuntimeDispatchKind::HelperBridgeToReference:
        if (!contract.reference_step_required || !contract.requires_helper_bridge) {
            plan.ok = false;
            plan.kind = DbtReferenceFallbackKind::None;
            plan.reject_reason = "helper-bridge-reference-step-not-required";
            return plan;
        }
        plan.ok = true;
        plan.kind = DbtReferenceFallbackKind::HelperBridgeReferenceStep;
        plan.reference_step_required = true;
        plan.requires_helper_bridge = true;
        plan.reference_step_count = 1;
        return plan;
    case DbtRuntimeDispatchKind::LoweredBlock:
        plan.ok = false;
        plan.kind = DbtReferenceFallbackKind::None;
        plan.reference_backend.clear();
        plan.reject_reason = "lowered-block-does-not-require-reference-fallback";
        return plan;
    case DbtRuntimeDispatchKind::None:
        plan.ok = false;
        plan.kind = DbtReferenceFallbackKind::None;
        if (plan.reject_reason == "none") {
            plan.reject_reason = "no-runtime-dispatch";
        }
        return plan;
    }

    plan.ok = false;
    plan.kind = DbtReferenceFallbackKind::None;
    plan.reject_reason = "unknown-runtime-dispatch";
    return plan;
}

const char* dbt_reference_fallback_kind_name(DbtReferenceFallbackKind kind) {
    switch (kind) {
    case DbtReferenceFallbackKind::None:
        return "none";
    case DbtReferenceFallbackKind::ReferenceStep:
        return "reference-step";
    case DbtReferenceFallbackKind::HelperBridgeReferenceStep:
        return "helper-bridge-reference-step";
    }
    return "unknown";
}

std::string format_dbt_reference_fallback_plan(const DbtReferenceFallbackPlan& plan) {
    std::ostringstream out;
    out << "reference-fallback:"
        << " kind=" << dbt_reference_fallback_kind_name(plan.kind)
        << " ok=" << bool_name(plan.ok)
        << " source=" << dbt_jit_dry_run_source_name(plan.source)
        << " pc=" << hex_u64(plan.pc)
        << " end=" << hex_u64(plan.end_pc)
        << " backend=" << (plan.reference_backend.empty() ? "none" : plan.reference_backend)
        << " reference-step=" << bool_name(plan.reference_step_required)
        << " reference-steps=" << plan.reference_step_count
        << " helper-bridge=" << bool_name(plan.requires_helper_bridge)
        << " helper=" << dbt_helper_replay_kind_name(plan.helper_replay_kind)
        << " reject=" << dbt_reject_kind_name(plan.reject_kind)
        << " reason=" << plan.reject_reason
        << " dry-run-only=" << bool_name(plan.dry_run_only)
        << " executed-helper=" << bool_name(plan.executed_helper)
        << " executed-reference-step=" << bool_name(plan.executed_reference_step)
        << " mutates-state=" << bool_name(plan.mutates_cpu_state)
        << " host-code=" << bool_name(plan.generated_host_code)
        << " exec-mem=" << bool_name(plan.requested_executable_memory)
        << " guest-exec=" << bool_name(plan.executed_guest_code);
    return out.str();
}
