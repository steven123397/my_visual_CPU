#include "dbt_reference_fallback_execution.h"

#include <cstdio>
#include <sstream>

#include "functional_backend.h"

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

DbtReferenceFallbackExecutionResult reject_result(
    const DbtReferenceFallbackExecutionRequest& request,
    const char* reason) {
    return DbtReferenceFallbackExecutionResult{
        .ok = false,
        .kind = request.kind,
        .source = request.source,
        .pc = request.pc,
        .end_pc = request.end_pc,
        .next_pc = request.pc,
        .reference_step_count = request.reference_step_count,
        .reference_backend = request.reference_backend,
        .reject_reason = reason,
        .dry_run_only = false,
        .requires_helper_bridge = request.requires_helper_bridge,
        .trap_or_fault_placeholder = request.trap_or_fault_placeholder,
        .reject_kind = request.reject_kind,
        .helper_replay_kind = request.helper_replay_kind,
    };
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

DbtReferenceFallbackExecutionResult execute_dbt_reference_fallback(
    CPU& cpu,
    Bus& bus,
    const DbtReferenceFallbackExecutionRequest& request) {
    if (!request.ok || !request.will_call_reference_backend ||
        request.reference_step_count == 0) {
        return reject_result(request, "invalid-reference-fallback-execution-request");
    }

    const uint64_t before_pc = cpu.core().pc();
    const uint64_t before_instret = cpu.core().instret();
    FunctionalBackend backend(cpu, bus);
    for (uint64_t i = 0; i < request.reference_step_count; ++i) {
        backend.step();
    }

    const uint64_t next_pc = cpu.core().pc();
    const bool mutated = next_pc != before_pc || cpu.core().instret() != before_instret;
    return DbtReferenceFallbackExecutionResult{
        .ok = true,
        .kind = request.kind,
        .source = request.source,
        .pc = request.pc,
        .end_pc = request.end_pc,
        .next_pc = next_pc,
        .reference_step_count = request.reference_step_count,
        .reference_steps_executed = request.reference_step_count,
        .reference_backend = request.reference_backend.empty()
                                 ? "functional-reference"
                                 : request.reference_backend,
        .reject_reason = request.reject_reason,
        .dry_run_only = false,
        .executed_helper = false,
        .executed_reference_step = true,
        .mutated_cpu_state = mutated,
        .executed_guest_code = true,
        .requires_helper_bridge = request.requires_helper_bridge,
        .trap_or_fault_placeholder = request.trap_or_fault_placeholder,
        .reject_kind = request.reject_kind,
        .helper_replay_kind = request.helper_replay_kind,
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

std::string format_dbt_reference_fallback_execution_result(
    const DbtReferenceFallbackExecutionResult& result) {
    std::ostringstream out;
    out << "fallback-exec-result:"
        << " kind=" << dbt_reference_fallback_execution_kind_name(result.kind)
        << " ok=" << bool_name(result.ok)
        << " source=" << dbt_jit_dry_run_source_name(result.source)
        << " pc=" << hex_u64(result.pc)
        << " end=" << hex_u64(result.end_pc)
        << " next-pc=" << hex_u64(result.next_pc)
        << " backend=" << (result.reference_backend.empty() ? "none" : result.reference_backend)
        << " reference-steps=" << result.reference_step_count
        << " executed-steps=" << result.reference_steps_executed
        << " helper-bridge=" << bool_name(result.requires_helper_bridge)
        << " trap-or-fault=" << bool_name(result.trap_or_fault_placeholder)
        << " helper=" << dbt_helper_replay_kind_name(result.helper_replay_kind)
        << " reject=" << dbt_reject_kind_name(result.reject_kind)
        << " reason=" << (result.reject_reason.empty() ? "none" : result.reject_reason)
        << " dry-run-only=" << bool_name(result.dry_run_only)
        << " executed-helper=" << bool_name(result.executed_helper)
        << " executed-reference-step=" << bool_name(result.executed_reference_step)
        << " mutates-state=" << bool_name(result.mutated_cpu_state)
        << " guest-exec=" << bool_name(result.executed_guest_code);
    return out.str();
}
