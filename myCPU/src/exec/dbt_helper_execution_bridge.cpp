#include "dbt_helper_execution_bridge.h"

#include <cstdio>
#include <sstream>

#include "../isa/atomic_contract.h"
#include "memory_ops.h"

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

DbtHelperExecutionResult reject_result(const DbtHelperExecutionRequest& request,
                                       const char* reason) {
    return DbtHelperExecutionResult{
        .ok = false,
        .reject_reason = reason,
        .kind = request.kind,
        .replay_kind = request.replay_kind,
        .pc = request.pc,
        .next_pc = request.pc,
        .addr = request.addr,
        .size = request.size,
        .rd = request.rd,
        .value = request.value,
        .sign_extend = request.sign_extend,
        .dry_run_only = false,
        .executed_helper = false,
        .mutated_cpu_state = false,
        .fallback_to_reference_on_trap = request.fallback_to_reference_on_trap,
        .commit_boundary = request.requires_commit_boundary,
        .serializing = request.serializing,
    };
}

DbtHelperExecutionResult fault_result(const DbtHelperExecutionRequest& request,
                                      const TrapRequest& fault) {
    return DbtHelperExecutionResult{
        .ok = false,
        .reject_reason = "helper-trap-reference-fallback",
        .kind = request.kind,
        .replay_kind = request.replay_kind,
        .pc = request.pc,
        .next_pc = request.pc,
        .addr = request.addr,
        .size = request.size,
        .rd = request.rd,
        .value = request.value,
        .sign_extend = request.sign_extend,
        .dry_run_only = false,
        .executed_helper = true,
        .mutated_cpu_state = false,
        .retired = false,
        .trap_taken = true,
        .trap_cause = fault.cause,
        .trap_tval = fault.tval,
        .fallback_to_reference_on_trap = true,
        .commit_boundary = request.requires_commit_boundary,
        .serializing = request.serializing,
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

DbtHelperExecutionResult execute_dbt_helper_request(
    CPU& cpu,
    Bus& bus,
    const DbtHelperExecutionRequest& request) {
    if (!request.ok) {
        return reject_result(request, "invalid-helper-execution-request");
    }

    const uint64_t next_pc = request.pc + 4;
    switch (request.kind) {
    case DbtHelperExecutionKind::ScalarMemoryLoad: {
        const AddressSpace::AccessResult access =
            cpu.address_space().load_result(bus, request.addr, request.size);
        if (!access.ok) {
            return fault_result(request, access.fault);
        }

        const uint64_t loaded =
            extend_loaded_value(access.value, request.size, request.sign_extend);
        cpu.core().write_gpr(request.rd, loaded);
        cpu.core().set_pc(next_pc);
        cpu.core().advance_instret();
        return DbtHelperExecutionResult{
            .ok = true,
            .kind = request.kind,
            .replay_kind = request.replay_kind,
            .pc = request.pc,
            .next_pc = next_pc,
            .addr = request.addr,
            .size = request.size,
            .rd = request.rd,
            .value = loaded,
            .sign_extend = request.sign_extend,
            .dry_run_only = false,
            .executed_helper = true,
            .mutated_cpu_state = request.rd != 0,
            .retired = true,
            .fallback_to_reference_on_trap = request.fallback_to_reference_on_trap,
            .platform_state_changed = false,
            .commit_boundary = request.requires_commit_boundary,
            .serializing = request.serializing,
        };
    }
    case DbtHelperExecutionKind::ScalarMemoryStore: {
        const AddressSpace::AccessResult access =
            cpu.address_space().store_result(bus, request.addr, request.value, request.size);
        invalidate_reservation_for_store(cpu, bus, request.addr, request.size);
        if (!access.ok) {
            return fault_result(request, access.fault);
        }

        cpu.core().set_pc(next_pc);
        cpu.core().advance_instret();
        return DbtHelperExecutionResult{
            .ok = true,
            .kind = request.kind,
            .replay_kind = request.replay_kind,
            .pc = request.pc,
            .next_pc = next_pc,
            .addr = request.addr,
            .size = request.size,
            .rd = request.rd,
            .value = request.value,
            .sign_extend = request.sign_extend,
            .dry_run_only = false,
            .executed_helper = true,
            .mutated_cpu_state = true,
            .retired = true,
            .fallback_to_reference_on_trap = request.fallback_to_reference_on_trap,
            .platform_state_changed = true,
            .commit_boundary = request.requires_commit_boundary,
            .serializing = request.serializing,
        };
    }
    case DbtHelperExecutionKind::None:
    case DbtHelperExecutionKind::CsrWrite:
    case DbtHelperExecutionKind::AtomicMemory:
    case DbtHelperExecutionKind::VectorConfig:
    case DbtHelperExecutionKind::VectorMemoryLoad:
    case DbtHelperExecutionKind::VectorMemoryStore:
    case DbtHelperExecutionKind::VectorAlu:
        return reject_result(request, "unsupported-helper-execution-kind");
    }

    return reject_result(request, "unknown-helper-execution-kind");
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

std::string format_dbt_helper_execution_result(
    const DbtHelperExecutionResult& result) {
    std::ostringstream out;
    out << "helper-exec-result:"
        << " kind=" << dbt_helper_execution_kind_name(result.kind)
        << " ok=" << bool_name(result.ok)
        << " pc=" << hex_u64(result.pc)
        << " next-pc=" << hex_u64(result.next_pc)
        << " addr=" << hex_u64(result.addr)
        << " size=" << static_cast<uint32_t>(result.size)
        << " rd=" << static_cast<uint32_t>(result.rd)
        << " value=" << hex_u64(result.value)
        << " replay=" << dbt_helper_replay_kind_name(result.replay_kind)
        << " reason=" << (result.reject_reason.empty() ? "none" : result.reject_reason)
        << " dry-run-only=" << bool_name(result.dry_run_only)
        << " executed-helper=" << bool_name(result.executed_helper)
        << " mutates-state=" << bool_name(result.mutated_cpu_state)
        << " retired=" << bool_name(result.retired)
        << " trap=" << bool_name(result.trap_taken)
        << " trap-cause=" << hex_u64(result.trap_cause)
        << " trap-tval=" << hex_u64(result.trap_tval)
        << " reference-on-trap=" << bool_name(result.fallback_to_reference_on_trap)
        << " platform-state=" << bool_name(result.platform_state_changed)
        << " commit-boundary=" << bool_name(result.commit_boundary)
        << " serializing=" << bool_name(result.serializing);
    return out.str();
}
