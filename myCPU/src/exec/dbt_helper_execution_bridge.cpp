#include "dbt_helper_execution_bridge.h"

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

DbtHelperExecutionKind execution_kind_from_replay(DbtHelperReplayKind kind) {
    switch (kind) {
    case DbtHelperReplayKind::None:
        return DbtHelperExecutionKind::None;
    case DbtHelperReplayKind::ScalarMemoryLoad:
        return DbtHelperExecutionKind::ScalarMemoryLoad;
    case DbtHelperReplayKind::ScalarMemoryStore:
        return DbtHelperExecutionKind::ScalarMemoryStore;
    case DbtHelperReplayKind::CsrWrite:
        return DbtHelperExecutionKind::CsrWrite;
    case DbtHelperReplayKind::AtomicMemory:
        return DbtHelperExecutionKind::AtomicMemory;
    case DbtHelperReplayKind::VectorConfig:
        return DbtHelperExecutionKind::VectorConfig;
    case DbtHelperReplayKind::VectorMemoryLoad:
        return DbtHelperExecutionKind::VectorMemoryLoad;
    case DbtHelperReplayKind::VectorMemoryStore:
        return DbtHelperExecutionKind::VectorMemoryStore;
    case DbtHelperReplayKind::VectorAlu:
        return DbtHelperExecutionKind::VectorAlu;
    }
    return DbtHelperExecutionKind::None;
}

DbtHelperExecutionRequest reject_request(const char* reason) {
    return DbtHelperExecutionRequest{
        .ok = false,
        .reject_reason = reason,
        .dry_run_only = true,
        .executed_helper = false,
        .mutates_cpu_state = false,
    };
}

}  // namespace

DbtHelperExecutionRequest plan_dbt_helper_execution_bridge(
    const DbtHelperReplayPlan& replay) {
    if (!replay.ok) {
        return reject_request("invalid-helper-replay-plan");
    }

    const DbtHelperExecutionKind kind = execution_kind_from_replay(replay.kind);
    if (kind == DbtHelperExecutionKind::None) {
        return reject_request("unsupported-helper-replay-kind");
    }

    return DbtHelperExecutionRequest{
        .ok = true,
        .kind = kind,
        .replay_kind = replay.kind,
        .helper_kind = replay.helper_kind,
        .pc = replay.pc,
        .raw = replay.raw,
        .rd = replay.rd,
        .addr = replay.addr,
        .size = replay.size,
        .sign_extend = replay.sign_extend,
        .csr_addr = replay.csr_addr,
        .value = replay.value,
        .atomic_op = replay.atomic_op,
        .atomic_aq = replay.atomic_aq,
        .atomic_rl = replay.atomic_rl,
        .vector_op = replay.vector_op,
        .vector_vs1 = replay.vector_vs1,
        .vector_vs2 = replay.vector_vs2,
        .vector_sew_bytes = replay.vector_sew_bytes,
        .vector_vl = replay.vector_vl,
        .reads_memory = replay.reads_memory,
        .writes_memory = replay.writes_memory,
        .writes_gpr = replay.writes_gpr,
        .writes_csr = replay.writes_csr,
        .writes_vector = replay.writes_vector,
        .changes_vector_config = replay.changes_vector_config,
        .may_trap = replay.may_trap,
        .may_change_platform_state = replay.may_change_platform_state,
        .requires_commit_boundary = replay.commit_at_boundary,
        .non_speculative = replay.non_speculative,
        .serializing = replay.serializing,
        .fallback_to_reference_on_trap = replay.may_trap,
        .dry_run_only = true,
        .executed_helper = false,
        .mutates_cpu_state = false,
    };
}

const char* dbt_helper_execution_kind_name(DbtHelperExecutionKind kind) {
    switch (kind) {
    case DbtHelperExecutionKind::None:
        return "none";
    case DbtHelperExecutionKind::ScalarMemoryLoad:
        return "scalar-memory-load";
    case DbtHelperExecutionKind::ScalarMemoryStore:
        return "scalar-memory-store";
    case DbtHelperExecutionKind::CsrWrite:
        return "csr-write";
    case DbtHelperExecutionKind::AtomicMemory:
        return "atomic-memory";
    case DbtHelperExecutionKind::VectorConfig:
        return "vector-config";
    case DbtHelperExecutionKind::VectorMemoryLoad:
        return "vector-memory-load";
    case DbtHelperExecutionKind::VectorMemoryStore:
        return "vector-memory-store";
    case DbtHelperExecutionKind::VectorAlu:
        return "vector-alu";
    }
    return "unknown";
}

std::string format_dbt_helper_execution_request(
    const DbtHelperExecutionRequest& request) {
    std::ostringstream out;
    out << "helper-exec:"
        << " kind=" << dbt_helper_execution_kind_name(request.kind)
        << " ok=" << bool_name(request.ok)
        << " pc=" << hex_u64(request.pc)
        << " replay=" << dbt_helper_replay_kind_name(request.replay_kind)
        << " reason=" << (request.reject_reason.empty() ? "none" : request.reject_reason)
        << " reads-memory=" << bool_name(request.reads_memory)
        << " writes-memory=" << bool_name(request.writes_memory)
        << " writes-gpr=" << bool_name(request.writes_gpr)
        << " writes-csr=" << bool_name(request.writes_csr)
        << " writes-vector=" << bool_name(request.writes_vector)
        << " may-trap=" << bool_name(request.may_trap)
        << " reference-on-trap=" << bool_name(request.fallback_to_reference_on_trap)
        << " commit-boundary=" << bool_name(request.requires_commit_boundary)
        << " serializing=" << bool_name(request.serializing)
        << " dry-run-only=" << bool_name(request.dry_run_only)
        << " executed-helper=" << bool_name(request.executed_helper)
        << " mutates-state=" << bool_name(request.mutates_cpu_state);
    return out.str();
}
