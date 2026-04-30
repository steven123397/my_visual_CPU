#include "dbt_block_plan.h"

#include <algorithm>
#include <utility>
#include <vector>

#include "../cpu.h"
#include "../isa/execution_context.h"
#include "../isa/instruction_semantics.h"
#include "../mem/bus.h"
#include "execution_profile.h"

extern "C" {
#include "../decode.h"
}

namespace {

struct FetchDecodeResult {
    bool ok{false};
    Insn insn{};
    std::string fallback_reason{};
};

uint8_t instruction_size(const Insn& insn) {
    return insn.size != 0 ? insn.size : 4;
}

bool requires_helper(const InsnEffects& effects) {
    return effects.mem.kind != MemoryRequest::Kind::None ||
           effects.atomic.kind != AtomicRequest::Kind::None ||
           effects.vector.kind != VectorRequest::Kind::None ||
           effects.csr_write.enable;
}

bool requires_fallback(const InsnEffects& effects) {
    return effects.control.redirect_pc ||
           effects.control.halt ||
           effects.control.flush_tlb ||
           effects.control.trap_return != TrapReturnKind::None;
}

bool is_control_flow_instruction(const Insn& insn) {
    return insn.opcode == 0x63U || insn.opcode == 0x67U || insn.opcode == 0x6FU;
}

const char* memory_boundary_kind(MemoryRequest::Kind kind) {
    switch (kind) {
    case MemoryRequest::Kind::Load:
        return "memory-load";
    case MemoryRequest::Kind::Store:
        return "memory-store";
    case MemoryRequest::Kind::None:
        return "";
    }
    return "";
}

DbtBoundaryKind memory_boundary_enum(MemoryRequest::Kind kind) {
    switch (kind) {
    case MemoryRequest::Kind::Load:
        return DbtBoundaryKind::MemoryLoad;
    case MemoryRequest::Kind::Store:
        return DbtBoundaryKind::MemoryStore;
    case MemoryRequest::Kind::None:
        return DbtBoundaryKind::None;
    }
    return DbtBoundaryKind::None;
}

const char* helper_boundary_kind(const InsnEffects& effects) {
    if (effects.mem.kind != MemoryRequest::Kind::None) {
        return memory_boundary_kind(effects.mem.kind);
    }
    if (effects.atomic.kind != AtomicRequest::Kind::None) {
        return "atomic";
    }
    if (effects.vector.kind != VectorRequest::Kind::None) {
        return "vector";
    }
    if (effects.csr_write.enable) {
        return "csr-write";
    }
    return "helper";
}

DbtBoundaryKind helper_boundary_enum(const InsnEffects& effects) {
    if (effects.mem.kind != MemoryRequest::Kind::None) {
        return memory_boundary_enum(effects.mem.kind);
    }
    if (effects.atomic.kind != AtomicRequest::Kind::None) {
        return DbtBoundaryKind::Atomic;
    }
    if (effects.vector.kind != VectorRequest::Kind::None) {
        return DbtBoundaryKind::Vector;
    }
    if (effects.csr_write.enable) {
        return DbtBoundaryKind::CsrWrite;
    }
    return DbtBoundaryKind::Fallback;
}

DbtHelperKind helper_kind_from_effects(const InsnEffects& effects) {
    if (effects.mem.kind == MemoryRequest::Kind::Load) {
        return DbtHelperKind::MemoryLoad;
    }
    if (effects.mem.kind == MemoryRequest::Kind::Store) {
        return DbtHelperKind::MemoryStore;
    }
    if (effects.csr_write.enable) {
        return DbtHelperKind::CsrWrite;
    }
    if (effects.atomic.kind != AtomicRequest::Kind::None) {
        return DbtHelperKind::Atomic;
    }
    if (effects.vector.kind != VectorRequest::Kind::None) {
        return DbtHelperKind::Vector;
    }
    return DbtHelperKind::None;
}

DbtAtomicHelperOp atomic_helper_op_from_effect(const AtomicRequest& request) {
    switch (request.kind) {
    case AtomicRequest::Kind::None:
        return DbtAtomicHelperOp::None;
    case AtomicRequest::Kind::LoadReserved:
        return DbtAtomicHelperOp::LoadReserved;
    case AtomicRequest::Kind::StoreConditional:
        return DbtAtomicHelperOp::StoreConditional;
    case AtomicRequest::Kind::Swap:
        return DbtAtomicHelperOp::Swap;
    case AtomicRequest::Kind::Add:
        return DbtAtomicHelperOp::Add;
    case AtomicRequest::Kind::Xor:
        return DbtAtomicHelperOp::Xor;
    case AtomicRequest::Kind::And:
        return DbtAtomicHelperOp::And;
    case AtomicRequest::Kind::Or:
        return DbtAtomicHelperOp::Or;
    case AtomicRequest::Kind::Min:
        return DbtAtomicHelperOp::Min;
    case AtomicRequest::Kind::Max:
        return DbtAtomicHelperOp::Max;
    case AtomicRequest::Kind::MinUnsigned:
        return DbtAtomicHelperOp::MinUnsigned;
    case AtomicRequest::Kind::MaxUnsigned:
        return DbtAtomicHelperOp::MaxUnsigned;
    }
    return DbtAtomicHelperOp::None;
}

DbtVectorHelperOp vector_helper_op_from_effect(const VectorRequest& request) {
    switch (request.kind) {
    case VectorRequest::Kind::None:
        return DbtVectorHelperOp::None;
    case VectorRequest::Kind::SetConfig:
        return DbtVectorHelperOp::SetConfig;
    case VectorRequest::Kind::Load:
        return DbtVectorHelperOp::Load;
    case VectorRequest::Kind::Store:
        return DbtVectorHelperOp::Store;
    case VectorRequest::Kind::Add:
        return DbtVectorHelperOp::Add;
    case VectorRequest::Kind::Mul:
        return DbtVectorHelperOp::Mul;
    case VectorRequest::Kind::Max:
        return DbtVectorHelperOp::Max;
    case VectorRequest::Kind::Dot:
        return DbtVectorHelperOp::Dot;
    }
    return DbtVectorHelperOp::None;
}

const char* fallback_boundary_kind(const InsnEffects& effects) {
    if (effects.trap.valid) {
        return "trap";
    }
    if (effects.control.flush_tlb) {
        return "tlb-flush";
    }
    if (effects.control.trap_return != TrapReturnKind::None) {
        return "trap-return";
    }
    if (effects.control.halt) {
        return "halt";
    }
    if (effects.control.redirect_pc) {
        return "control-flow";
    }
    if (!effects.retired) {
        return "not-retired";
    }
    return "fallback";
}

DbtBoundaryKind fallback_boundary_enum(const InsnEffects& effects) {
    if (effects.trap.valid) {
        return DbtBoundaryKind::Trap;
    }
    if (effects.control.flush_tlb) {
        return DbtBoundaryKind::TlbFlush;
    }
    if (effects.control.trap_return != TrapReturnKind::None) {
        return DbtBoundaryKind::TrapReturn;
    }
    if (effects.control.halt) {
        return DbtBoundaryKind::Halt;
    }
    if (effects.control.redirect_pc) {
        return DbtBoundaryKind::ControlFlow;
    }
    if (!effects.retired) {
        return DbtBoundaryKind::NotRetired;
    }
    return DbtBoundaryKind::Fallback;
}

FetchDecodeResult fetch_decode(CPU& cpu, Bus& bus, uint64_t pc) {
    const AddressSpace::AccessResult first_half = cpu.address_space().fetch16_result(bus, pc);
    if (!first_half.ok) {
        return FetchDecodeResult{
            .fallback_reason = "fetch-fault",
        };
    }

    uint32_t raw = static_cast<uint16_t>(first_half.value);
    if ((raw & 0x3U) == 0x3U) {
        const AddressSpace::AccessResult full_word = cpu.address_space().fetch32_result(bus, pc);
        if (!full_word.ok) {
            return FetchDecodeResult{
                .fallback_reason = "fetch-fault",
            };
        }
        raw = static_cast<uint32_t>(full_word.value);
    }

    Insn insn{};
    decode(raw, &insn);
    insn.raw = raw;
    return FetchDecodeResult{
        .ok = true,
        .insn = insn,
    };
}

DbtHelperPlan make_helper_plan(uint64_t pc, const Insn& insn, const InsnEffects& effects) {
    DbtHelperPlan helper{
        .required = true,
        .kind = helper_kind_from_effects(effects),
        .pc = pc,
        .raw = insn.raw,
    };

    if (effects.mem.kind != MemoryRequest::Kind::None) {
        helper.rd = effects.mem.rd;
        helper.addr = effects.mem.addr;
        helper.size = static_cast<uint8_t>(effects.mem.size);
        helper.sign_extend = effects.mem.sign_extend;
        helper.commit_at_boundary = effects.mem.commit_at_boundary;
        helper.non_speculative = effects.mem.non_speculative;
        helper.value = effects.mem.store_value;
        return helper;
    }
    if (effects.csr_write.enable) {
        helper.rd = effects.rd_write.rd;
        helper.csr_addr = effects.csr_write.addr;
        helper.value = effects.csr_write.value;
        return helper;
    }
    if (effects.atomic.kind != AtomicRequest::Kind::None) {
        helper.rd = effects.atomic.rd;
        helper.addr = effects.atomic.addr;
        helper.size = static_cast<uint8_t>(effects.atomic.size);
        helper.commit_at_boundary = effects.atomic.commit_at_boundary;
        helper.non_speculative = effects.atomic.non_speculative;
        helper.value = effects.atomic.store_value;
        helper.atomic_op = atomic_helper_op_from_effect(effects.atomic);
        helper.atomic_aq = effects.atomic.aq;
        helper.atomic_rl = effects.atomic.rl;
        return helper;
    }
    if (effects.vector.kind != VectorRequest::Kind::None) {
        helper.rd = effects.vector.vd;
        helper.addr = effects.vector.addr;
        helper.size = effects.vector.sew_bytes;
        helper.vector_op = vector_helper_op_from_effect(effects.vector);
        helper.vector_vs1 = effects.vector.vs1;
        helper.vector_vs2 = effects.vector.vs2;
        helper.vector_sew_bytes = effects.vector.sew_bytes;
        helper.vector_vl = effects.vector.vl;
        return helper;
    }

    helper.required = false;
    return helper;
}

DbtBlockPlan plan_fallback(uint64_t start_pc,
                           uint64_t end_pc,
                           uint64_t inlineable,
                           uint64_t pc,
                           uint32_t raw,
                           std::string reason,
                           std::string boundary_kind = {},
                           DbtBoundaryKind boundary = DbtBoundaryKind::None,
                           DbtHelperPlan helper_plan = {},
                           std::vector<DbtDryRunIrOp> dry_run_ir = {}) {
    return DbtBlockPlan{
        .ok = false,
        .start_pc = start_pc,
        .end_pc = end_pc,
        .inlineable_instructions = inlineable,
        .fallback_pc = pc,
        .fallback_raw = raw,
        .fallback_reason = std::move(reason),
        .boundary_kind = std::move(boundary_kind),
        .boundary = boundary,
        .helper_plan = helper_plan,
        .dry_run_ir = std::move(dry_run_ir),
    };
}

bool hotter_translation_candidate(const ExecutionHotPathEntry& lhs,
                                  const ExecutionHotPathEntry& rhs) {
    if (lhs.executions != rhs.executions) {
        return lhs.executions > rhs.executions;
    }
    if (lhs.retired_instructions != rhs.retired_instructions) {
        return lhs.retired_instructions > rhs.retired_instructions;
    }
    if (lhs.start_pc != rhs.start_pc) {
        return lhs.start_pc < rhs.start_pc;
    }
    return lhs.end_pc < rhs.end_pc;
}

DbtDryRunIrOp make_dry_run_ir_op(uint64_t pc,
                                 const Insn& insn,
                                 const InsnEffects& effects) {
    const uint8_t size = instruction_size(insn);
    return DbtDryRunIrOp{
        .kind = DbtDryRunIrKind::ArchitectedEffect,
        .pc = pc,
        .raw = insn.raw,
        .size = size,
        .next_pc = pc + size,
        .rd_write = effects.rd_write.enable,
        .rd = effects.rd_write.rd,
    };
}

bool ranges_overlap(uint64_t lhs_addr, uint64_t lhs_size, uint64_t rhs_addr, uint64_t rhs_size) {
    if (lhs_size == 0 || rhs_size == 0) {
        return false;
    }
    const uint64_t lhs_end = lhs_addr + lhs_size;
    const uint64_t rhs_end = rhs_addr + rhs_size;
    return lhs_addr < rhs_end && rhs_addr < lhs_end;
}

}  // namespace

DbtBlockPlan plan_dbt_block(CPU& cpu, Bus& bus, uint64_t start_pc, uint64_t end_pc) {
    uint64_t inlineable = 0;
    std::vector<DbtDryRunIrOp> dry_run_ir;
    for (uint64_t pc = start_pc; pc <= end_pc;) {
        const FetchDecodeResult fetched = fetch_decode(cpu, bus, pc);
        if (!fetched.ok) {
            return plan_fallback(start_pc,
                                 end_pc,
                                 inlineable,
                                 pc,
                                 0,
                                 fetched.fallback_reason,
                                 "fetch-fault",
                                 DbtBoundaryKind::FetchFault,
                                 {},
                                 std::move(dry_run_ir));
        }

        const Insn& insn = fetched.insn;
        if (!InstructionSemantics::supports(insn)) {
            return plan_fallback(start_pc,
                                 end_pc,
                                 inlineable,
                                 pc,
                                 insn.raw,
                                 "fallback-required",
                                 "unsupported",
                                 DbtBoundaryKind::Unsupported,
                                 {},
                                 std::move(dry_run_ir));
        }
        if (is_control_flow_instruction(insn)) {
            return plan_fallback(start_pc,
                                 end_pc,
                                 inlineable,
                                 pc,
                                 insn.raw,
                                 "fallback-required",
                                 "control-flow",
                                 DbtBoundaryKind::ControlFlow,
                                 {},
                                 std::move(dry_run_ir));
        }

        ExecutionContext ctx(cpu, bus);
        const InsnEffects effects = InstructionSemantics::execute(insn, ctx);
        if (effects.trap.valid || !effects.retired || requires_fallback(effects)) {
            return plan_fallback(start_pc,
                                 end_pc,
                                 inlineable,
                                 pc,
                                 insn.raw,
                                 "fallback-required",
                                 fallback_boundary_kind(effects),
                                 fallback_boundary_enum(effects),
                                 {},
                                 std::move(dry_run_ir));
        }
        if (requires_helper(effects)) {
            return plan_fallback(start_pc,
                                 end_pc,
                                 inlineable,
                                 pc,
                                 insn.raw,
                                 "helper-required",
                                 helper_boundary_kind(effects),
                                 helper_boundary_enum(effects),
                                 make_helper_plan(pc, insn, effects),
                                 std::move(dry_run_ir));
        }

        dry_run_ir.push_back(make_dry_run_ir_op(pc, insn, effects));
        inlineable += 1;
        pc += instruction_size(insn);
    }

    return DbtBlockPlan{
        .ok = true,
        .start_pc = start_pc,
        .end_pc = end_pc,
        .inlineable_instructions = inlineable,
        .dry_run_ir = std::move(dry_run_ir),
    };
}

DbtBlockPlan plan_dbt_hot_path(CPU& cpu,
                               Bus& bus,
                               const ExecutionProfileSnapshot& profile) {
    if (profile.hot_paths.empty()) {
        return plan_fallback(0,
                             0,
                             0,
                             0,
                             0,
                             "no-hot-paths",
                             "profile-candidate",
                             DbtBoundaryKind::Fallback);
    }

    const auto top_it = std::min_element(
        profile.hot_paths.begin(),
        profile.hot_paths.end(),
        [](const ExecutionHotPathEntry& lhs, const ExecutionHotPathEntry& rhs) {
            return hotter_translation_candidate(lhs, rhs);
        });
    const ExecutionHotPathEntry& candidate = *top_it;
    if (candidate.executions < 2) {
        DbtBlockPlan plan =
            plan_fallback(candidate.start_pc,
                          candidate.end_pc,
                          0,
                          candidate.start_pc,
                          0,
                          "insufficient-repetition",
                          "profile-candidate",
                          DbtBoundaryKind::Fallback);
        plan.candidate_executions = candidate.executions;
        plan.candidate_retired_instructions = candidate.retired_instructions;
        return plan;
    }
    if (candidate.retired_instructions == 0) {
        DbtBlockPlan plan =
            plan_fallback(candidate.start_pc,
                          candidate.end_pc,
                          0,
                          candidate.start_pc,
                          0,
                          "empty-hot-path",
                          "profile-candidate",
                          DbtBoundaryKind::Fallback);
        plan.candidate_executions = candidate.executions;
        plan.candidate_retired_instructions = candidate.retired_instructions;
        return plan;
    }

    DbtBlockPlan plan = plan_dbt_block(cpu, bus, candidate.start_pc, candidate.end_pc);
    plan.candidate_executions = candidate.executions;
    plan.candidate_retired_instructions = candidate.retired_instructions;
    return plan;
}

DbtInvalidationPlan plan_dbt_block_invalidation_event(DbtInvalidationEventKind kind,
                                                      uint64_t event_addr,
                                                      uint64_t event_size,
                                                      uint64_t block_start_pc,
                                                      uint64_t block_end_pc) {
    switch (kind) {
    case DbtInvalidationEventKind::PrimaryImageLoad:
        return DbtInvalidationPlan{.invalidates = true, .reason = "primary-image-load"};
    case DbtInvalidationEventKind::DebugReset:
        return DbtInvalidationPlan{.invalidates = true, .reason = "debug-reset"};
    case DbtInvalidationEventKind::SatpWrite:
        return DbtInvalidationPlan{.invalidates = true, .reason = "satp-write"};
    case DbtInvalidationEventKind::SfenceVma:
        return DbtInvalidationPlan{.invalidates = true, .reason = "sfence-vma"};
    case DbtInvalidationEventKind::RegionAttributesChanged:
        return DbtInvalidationPlan{.invalidates = true, .reason = "region-attributes-changed"};
    case DbtInvalidationEventKind::PayloadLoad:
        if (ranges_overlap(event_addr, event_size, block_start_pc, block_end_pc - block_start_pc + 4)) {
            return DbtInvalidationPlan{.invalidates = true, .reason = "payload-overlaps-block"};
        }
        return DbtInvalidationPlan{.reason = "range-disjoint"};
    case DbtInvalidationEventKind::GuestStore:
        if (ranges_overlap(event_addr, event_size, block_start_pc, block_end_pc - block_start_pc + 4)) {
            return DbtInvalidationPlan{.invalidates = true, .reason = "guest-store-overlaps-block"};
        }
        return DbtInvalidationPlan{.reason = "range-disjoint"};
    }
    return DbtInvalidationPlan{.reason = "unknown-event"};
}

const char* dbt_helper_kind_name(DbtHelperKind kind) {
    switch (kind) {
    case DbtHelperKind::None:
        return "none";
    case DbtHelperKind::MemoryLoad:
        return "memory-load";
    case DbtHelperKind::MemoryStore:
        return "memory-store";
    case DbtHelperKind::CsrWrite:
        return "csr-write";
    case DbtHelperKind::Atomic:
        return "atomic";
    case DbtHelperKind::Vector:
        return "vector";
    }
    return "unknown";
}

const char* dbt_atomic_helper_op_name(DbtAtomicHelperOp op) {
    switch (op) {
    case DbtAtomicHelperOp::None:
        return "none";
    case DbtAtomicHelperOp::LoadReserved:
        return "load-reserved";
    case DbtAtomicHelperOp::StoreConditional:
        return "store-conditional";
    case DbtAtomicHelperOp::Swap:
        return "swap";
    case DbtAtomicHelperOp::Add:
        return "add";
    case DbtAtomicHelperOp::Xor:
        return "xor";
    case DbtAtomicHelperOp::And:
        return "and";
    case DbtAtomicHelperOp::Or:
        return "or";
    case DbtAtomicHelperOp::Min:
        return "min";
    case DbtAtomicHelperOp::Max:
        return "max";
    case DbtAtomicHelperOp::MinUnsigned:
        return "min-unsigned";
    case DbtAtomicHelperOp::MaxUnsigned:
        return "max-unsigned";
    }
    return "unknown";
}

const char* dbt_vector_helper_op_name(DbtVectorHelperOp op) {
    switch (op) {
    case DbtVectorHelperOp::None:
        return "none";
    case DbtVectorHelperOp::SetConfig:
        return "set-config";
    case DbtVectorHelperOp::Load:
        return "load";
    case DbtVectorHelperOp::Store:
        return "store";
    case DbtVectorHelperOp::Add:
        return "add";
    case DbtVectorHelperOp::Mul:
        return "mul";
    case DbtVectorHelperOp::Max:
        return "max";
    case DbtVectorHelperOp::Dot:
        return "dot";
    }
    return "unknown";
}
