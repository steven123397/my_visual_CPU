#include "dbt_runtime_dispatch.h"

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

DbtRuntimeDispatchContract base_contract(const DbtJitDryRunResult& result) {
    return DbtRuntimeDispatchContract{
        .source = result.source,
        .start_pc = result.start_pc,
        .end_pc = result.end_pc,
        .used_cache = result.used_cache,
        .inserted_cache = result.inserted_cache,
        .planned = result.planned,
        .translated = result.translated,
        .lowered = result.lowered,
        .dry_run_only = true,
        .mutates_cpu_state = false,
        .generated_host_code = result.generated_host_code,
        .requested_executable_memory = result.requested_executable_memory,
        .executed_guest_code = result.executed_guest_code,
        .lowered_instruction_count = result.lowering.instructions.size(),
        .reject_kind = result.translation.reject_kind,
        .reject_pc = result.translation.reject_pc,
        .reject_raw = result.translation.reject_raw,
        .reject_reason = result.translation.reject_reason.empty() ? "none" : result.translation.reject_reason,
        .helper_replay_kind = result.helper_replay.kind,
    };
}

}  // namespace

DbtRuntimeDispatchContract plan_dbt_runtime_dispatch_contract(
    const DbtJitDryRunResult& result) {
    DbtRuntimeDispatchContract contract = base_contract(result);

    switch (result.action) {
    case DbtJitDryRunAction::LoweredReady:
        if (!result.ok || !result.lowering.ok) {
            contract.ok = false;
            contract.kind = DbtRuntimeDispatchKind::ReferenceStep;
            contract.reference_step_required = true;
            return contract;
        }
        contract.ok = true;
        contract.kind = DbtRuntimeDispatchKind::LoweredBlock;
        contract.can_enter_lowered_block = true;
        return contract;
    case DbtJitDryRunAction::HelperBridgeRequired:
        if (!result.helper_replay.ok) {
            contract.ok = false;
            contract.kind = DbtRuntimeDispatchKind::ReferenceStep;
            contract.reference_step_required = true;
            return contract;
        }
        contract.ok = true;
        contract.kind = DbtRuntimeDispatchKind::HelperBridgeToReference;
        contract.requires_helper_bridge = true;
        contract.reference_step_required = true;
        contract.helper_commit_at_boundary = result.helper_replay.commit_at_boundary;
        contract.helper_serializing = result.helper_replay.serializing;
        contract.helper_may_trap = result.helper_replay.may_trap;
        contract.helper_may_change_platform_state =
            result.helper_replay.may_change_platform_state;
        return contract;
    case DbtJitDryRunAction::ReferenceFallback:
        contract.ok = true;
        contract.kind = DbtRuntimeDispatchKind::ReferenceStep;
        contract.reference_step_required = true;
        return contract;
    case DbtJitDryRunAction::None:
        contract.ok = false;
        contract.kind = DbtRuntimeDispatchKind::None;
        return contract;
    }

    contract.ok = false;
    contract.kind = DbtRuntimeDispatchKind::None;
    return contract;
}

const char* dbt_runtime_dispatch_kind_name(DbtRuntimeDispatchKind kind) {
    switch (kind) {
    case DbtRuntimeDispatchKind::None:
        return "none";
    case DbtRuntimeDispatchKind::LoweredBlock:
        return "lowered-block";
    case DbtRuntimeDispatchKind::HelperBridgeToReference:
        return "helper-bridge-to-reference";
    case DbtRuntimeDispatchKind::ReferenceStep:
        return "reference-step";
    }
    return "unknown";
}

std::string format_dbt_runtime_dispatch_contract(
    const DbtRuntimeDispatchContract& contract) {
    std::ostringstream out;
    out << "runtime-dispatch:"
        << " kind=" << dbt_runtime_dispatch_kind_name(contract.kind)
        << " ok=" << bool_name(contract.ok)
        << " source=" << dbt_jit_dry_run_source_name(contract.source)
        << " start=" << hex_u64(contract.start_pc)
        << " end=" << hex_u64(contract.end_pc)
        << " cache=" << (contract.used_cache ? "hit" : (contract.inserted_cache ? "miss-inserted" : "miss"))
        << " lowered-block=" << bool_name(contract.can_enter_lowered_block)
        << " helper-bridge=" << bool_name(contract.requires_helper_bridge)
        << " reference-step=" << bool_name(contract.reference_step_required)
        << " dry-run-only=" << bool_name(contract.dry_run_only)
        << " mutates-state=" << bool_name(contract.mutates_cpu_state)
        << " lowered-ops=" << contract.lowered_instruction_count
        << " reject=" << dbt_reject_kind_name(contract.reject_kind)
        << " reason=" << contract.reject_reason
        << " helper=" << dbt_helper_replay_kind_name(contract.helper_replay_kind)
        << " helper-serializing=" << bool_name(contract.helper_serializing)
        << " helper-may-trap=" << bool_name(contract.helper_may_trap)
        << " helper-platform-state=" << bool_name(contract.helper_may_change_platform_state)
        << " host-code=" << bool_name(contract.generated_host_code)
        << " exec-mem=" << bool_name(contract.requested_executable_memory)
        << " guest-exec=" << bool_name(contract.executed_guest_code);
    return out.str();
}
