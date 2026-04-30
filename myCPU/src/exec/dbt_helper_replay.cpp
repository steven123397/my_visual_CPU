#include "dbt_helper_replay.h"

namespace {

DbtHelperReplayPlan reject_replay(const char* reason) {
    return DbtHelperReplayPlan{
        .ok = false,
        .reject_reason = reason,
    };
}

DbtHelperReplayPlan base_plan(const DbtHelperPlan& helper) {
    return DbtHelperReplayPlan{
        .ok = true,
        .helper_kind = helper.kind,
        .pc = helper.pc,
        .raw = helper.raw,
        .rd = helper.rd,
        .addr = helper.addr,
        .size = helper.size,
        .sign_extend = helper.sign_extend,
        .csr_addr = helper.csr_addr,
        .value = helper.value,
        .atomic_op = helper.atomic_op,
        .atomic_aq = helper.atomic_aq,
        .atomic_rl = helper.atomic_rl,
        .vector_op = helper.vector_op,
        .vector_vs1 = helper.vector_vs1,
        .vector_vs2 = helper.vector_vs2,
        .vector_sew_bytes = helper.vector_sew_bytes,
        .vector_vl = helper.vector_vl,
        .commit_at_boundary = helper.commit_at_boundary,
        .non_speculative = helper.non_speculative,
    };
}

bool atomic_helper_writes_memory(DbtAtomicHelperOp op) {
    return op != DbtAtomicHelperOp::None && op != DbtAtomicHelperOp::LoadReserved;
}

DbtHelperReplayKind vector_replay_kind(DbtVectorHelperOp op) {
    switch (op) {
    case DbtVectorHelperOp::SetConfig:
        return DbtHelperReplayKind::VectorConfig;
    case DbtVectorHelperOp::Load:
        return DbtHelperReplayKind::VectorMemoryLoad;
    case DbtVectorHelperOp::Store:
        return DbtHelperReplayKind::VectorMemoryStore;
    case DbtVectorHelperOp::Add:
    case DbtVectorHelperOp::Mul:
    case DbtVectorHelperOp::Max:
    case DbtVectorHelperOp::Dot:
        return DbtHelperReplayKind::VectorAlu;
    case DbtVectorHelperOp::None:
        return DbtHelperReplayKind::None;
    }
    return DbtHelperReplayKind::None;
}

}  // namespace

DbtHelperReplayPlan plan_dbt_helper_replay(const DbtTranslationUnit& unit) {
    if (unit.ok) {
        return reject_replay("translation-unit-ok");
    }
    if (!unit.helper_plan.required) {
        return reject_replay("no-helper-plan");
    }

    const DbtHelperPlan& helper = unit.helper_plan;
    DbtHelperReplayPlan replay = base_plan(helper);

    switch (helper.kind) {
    case DbtHelperKind::None:
        return reject_replay("unknown-helper-kind");
    case DbtHelperKind::MemoryLoad:
        replay.kind = DbtHelperReplayKind::ScalarMemoryLoad;
        replay.reads_memory = true;
        replay.writes_gpr = helper.rd != 0;
        replay.may_trap = true;
        return replay;
    case DbtHelperKind::MemoryStore:
        replay.kind = DbtHelperReplayKind::ScalarMemoryStore;
        replay.writes_memory = true;
        replay.may_trap = true;
        replay.may_change_platform_state = true;
        replay.serializing = helper.commit_at_boundary || helper.non_speculative;
        return replay;
    case DbtHelperKind::CsrWrite:
        replay.kind = DbtHelperReplayKind::CsrWrite;
        replay.writes_gpr = helper.rd != 0;
        replay.writes_csr = true;
        replay.serializing = true;
        return replay;
    case DbtHelperKind::Atomic:
        replay.kind = DbtHelperReplayKind::AtomicMemory;
        replay.reads_memory = true;
        replay.writes_memory = atomic_helper_writes_memory(helper.atomic_op);
        replay.writes_gpr = helper.rd != 0;
        replay.may_trap = true;
        replay.may_change_platform_state = replay.writes_memory;
        replay.serializing = helper.commit_at_boundary || helper.non_speculative ||
                             helper.atomic_aq || helper.atomic_rl;
        return replay;
    case DbtHelperKind::Vector:
        replay.kind = vector_replay_kind(helper.vector_op);
        replay.reads_memory = helper.vector_op == DbtVectorHelperOp::Load;
        replay.writes_memory = helper.vector_op == DbtVectorHelperOp::Store;
        replay.writes_vector = helper.vector_op == DbtVectorHelperOp::Load ||
                               helper.vector_op == DbtVectorHelperOp::Add ||
                               helper.vector_op == DbtVectorHelperOp::Mul ||
                               helper.vector_op == DbtVectorHelperOp::Max ||
                               helper.vector_op == DbtVectorHelperOp::Dot;
        replay.changes_vector_config = helper.vector_op == DbtVectorHelperOp::SetConfig;
        replay.may_trap = replay.reads_memory || replay.writes_memory ||
                          replay.changes_vector_config || replay.kind == DbtHelperReplayKind::VectorAlu;
        replay.may_change_platform_state = replay.writes_memory;
        replay.serializing = replay.reads_memory || replay.writes_memory ||
                             replay.changes_vector_config;
        return replay;
    }

    return reject_replay("unknown-helper-kind");
}

const char* dbt_helper_replay_kind_name(DbtHelperReplayKind kind) {
    switch (kind) {
    case DbtHelperReplayKind::None:
        return "none";
    case DbtHelperReplayKind::ScalarMemoryLoad:
        return "scalar-memory-load";
    case DbtHelperReplayKind::ScalarMemoryStore:
        return "scalar-memory-store";
    case DbtHelperReplayKind::CsrWrite:
        return "csr-write";
    case DbtHelperReplayKind::AtomicMemory:
        return "atomic-memory";
    case DbtHelperReplayKind::VectorConfig:
        return "vector-config";
    case DbtHelperReplayKind::VectorMemoryLoad:
        return "vector-memory-load";
    case DbtHelperReplayKind::VectorMemoryStore:
        return "vector-memory-store";
    case DbtHelperReplayKind::VectorAlu:
        return "vector-alu";
    }
    return "unknown";
}
