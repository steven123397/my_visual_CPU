#include "dbt_reference_fallback_execution.h"

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

bool is_trap_or_fault(DbtRejectKind kind) {
    return kind == DbtRejectKind::FetchFault ||
           kind == DbtRejectKind::Trap ||
           kind == DbtRejectKind::TlbFlush ||
           kind == DbtRejectKind::TrapReturn ||
           kind == DbtRejectKind::Halt;
}

DbtReferenceFallbackExecutionKind classify_fallback(
    const DbtReferenceFallbackPlan& plan) {
    if (plan.requires_helper_bridge ||
        plan.kind == DbtReferenceFallbackKind::HelperBridgeReferenceStep) {
        return DbtReferenceFallbackExecutionKind::HelperBridgeReferenceStep;
    }
    if (is_trap_or_fault(plan.reject_kind)) {
        return DbtReferenceFallbackExecutionKind::TrapOrFaultReferenceStep;
    }
    if (plan.reject_reason == "jit-cache-miss" ||
        plan.reject_reason == "cache-miss") {
        return DbtReferenceFallbackExecutionKind::JitMissReferenceStep;
    }
    return DbtReferenceFallbackExecutionKind::ReferenceStep;
}

DbtReferenceFallbackExecutionRequest reject_request(const char* reason) {
    DbtReferenceFallbackExecutionRequest request{};
    request.ok = false;
    request.reject_reason = reason;
    request.dry_run_only = true;
    request.executed_helper = false;
    request.executed_reference_step = false;
    request.mutates_cpu_state = false;
    request.executed_guest_code = false;
    return request;
}

}  // namespace

DbtReferenceFallbackExecutionRequest plan_dbt_reference_fallback_execution(
    const DbtReferenceFallbackPlan& plan) {
    if (!plan.ok || !plan.reference_step_required || plan.reference_step_count == 0) {
        return reject_request("invalid-reference-fallback-plan");
    }

    const DbtReferenceFallbackExecutionKind kind = classify_fallback(plan);
    return DbtReferenceFallbackExecutionRequest{
        .ok = true,
        .kind = kind,
        .source = plan.source,
        .pc = plan.pc,
        .end_pc = plan.end_pc,
        .reference_step_count = plan.reference_step_count,
        .reference_backend = plan.reference_backend.empty()
                                 ? "functional-reference"
                                 : plan.reference_backend,
        .will_call_reference_backend = true,
        .requires_helper_bridge =
            kind == DbtReferenceFallbackExecutionKind::HelperBridgeReferenceStep,
        .trap_or_fault_placeholder =
            kind == DbtReferenceFallbackExecutionKind::TrapOrFaultReferenceStep,
        .dry_run_only = true,
        .executed_helper = false,
        .executed_reference_step = false,
        .mutates_cpu_state = false,
        .executed_guest_code = false,
        .helper_may_trap = plan.helper_may_trap,
        .reject_kind = plan.reject_kind,
        .reject_pc = plan.reject_pc,
        .reject_raw = plan.reject_raw,
        .reject_reason = plan.reject_reason.empty() ? "none" : plan.reject_reason,
        .helper_replay_kind = plan.helper_replay_kind,
    };
}

const char* dbt_reference_fallback_execution_kind_name(
    DbtReferenceFallbackExecutionKind kind) {
    switch (kind) {
    case DbtReferenceFallbackExecutionKind::None:
        return "none";
    case DbtReferenceFallbackExecutionKind::ReferenceStep:
        return "reference-step";
    case DbtReferenceFallbackExecutionKind::HelperBridgeReferenceStep:
        return "helper-bridge-reference-step";
    case DbtReferenceFallbackExecutionKind::JitMissReferenceStep:
        return "jit-miss-reference-step";
    case DbtReferenceFallbackExecutionKind::TrapOrFaultReferenceStep:
        return "trap-or-fault-reference-step";
    }
    return "unknown";
}

std::string format_dbt_reference_fallback_execution_request(
    const DbtReferenceFallbackExecutionRequest& request) {
    std::ostringstream out;
    out << "fallback-exec:"
        << " kind=" << dbt_reference_fallback_execution_kind_name(request.kind)
        << " ok=" << bool_name(request.ok)
        << " source=" << dbt_jit_dry_run_source_name(request.source)
        << " pc=" << hex_u64(request.pc)
        << " end=" << hex_u64(request.end_pc)
        << " backend=" << (request.reference_backend.empty() ? "none" : request.reference_backend)
        << " reference-steps=" << request.reference_step_count
        << " call-reference=" << bool_name(request.will_call_reference_backend)
        << " helper-bridge=" << bool_name(request.requires_helper_bridge)
        << " trap-or-fault=" << bool_name(request.trap_or_fault_placeholder)
        << " helper=" << dbt_helper_replay_kind_name(request.helper_replay_kind)
        << " reject=" << dbt_reject_kind_name(request.reject_kind)
        << " reason=" << (request.reject_reason.empty() ? "none" : request.reject_reason)
        << " dry-run-only=" << bool_name(request.dry_run_only)
        << " executed-helper=" << bool_name(request.executed_helper)
        << " executed-reference-step=" << bool_name(request.executed_reference_step)
        << " mutates-state=" << bool_name(request.mutates_cpu_state)
        << " guest-exec=" << bool_name(request.executed_guest_code);
    return out.str();
}
