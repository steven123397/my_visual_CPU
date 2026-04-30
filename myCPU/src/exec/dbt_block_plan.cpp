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

DbtBlockPlan plan_fallback(uint64_t start_pc,
                           uint64_t end_pc,
                           uint64_t inlineable,
                           uint64_t pc,
                           std::string reason,
                           std::string boundary_kind = {},
                           DbtBoundaryKind boundary = DbtBoundaryKind::None,
                           std::vector<DbtDryRunIrOp> dry_run_ir = {}) {
    return DbtBlockPlan{
        .ok = false,
        .start_pc = start_pc,
        .end_pc = end_pc,
        .inlineable_instructions = inlineable,
        .fallback_pc = pc,
        .fallback_reason = std::move(reason),
        .boundary_kind = std::move(boundary_kind),
        .boundary = boundary,
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
                                 fetched.fallback_reason,
                                 "fetch-fault",
                                 DbtBoundaryKind::FetchFault,
                                 std::move(dry_run_ir));
        }

        const Insn& insn = fetched.insn;
        if (!InstructionSemantics::supports(insn)) {
            return plan_fallback(start_pc,
                                 end_pc,
                                 inlineable,
                                 pc,
                                 "fallback-required",
                                 "unsupported",
                                 DbtBoundaryKind::Unsupported,
                                 std::move(dry_run_ir));
        }
        if (is_control_flow_instruction(insn)) {
            return plan_fallback(start_pc,
                                 end_pc,
                                 inlineable,
                                 pc,
                                 "fallback-required",
                                 "control-flow",
                                 DbtBoundaryKind::ControlFlow,
                                 std::move(dry_run_ir));
        }

        ExecutionContext ctx(cpu, bus);
        const InsnEffects effects = InstructionSemantics::execute(insn, ctx);
        if (effects.trap.valid || !effects.retired || requires_fallback(effects)) {
            return plan_fallback(start_pc,
                                 end_pc,
                                 inlineable,
                                 pc,
                                 "fallback-required",
                                 fallback_boundary_kind(effects),
                                 fallback_boundary_enum(effects),
                                 std::move(dry_run_ir));
        }
        if (requires_helper(effects)) {
            return plan_fallback(start_pc,
                                 end_pc,
                                 inlineable,
                                 pc,
                                 "helper-required",
                                 helper_boundary_kind(effects),
                                 helper_boundary_enum(effects),
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
        return plan_fallback(0, 0, 0, 0, "no-hot-paths");
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
            plan_fallback(candidate.start_pc, candidate.end_pc, 0, candidate.start_pc, "insufficient-repetition");
        plan.candidate_executions = candidate.executions;
        plan.candidate_retired_instructions = candidate.retired_instructions;
        return plan;
    }
    if (candidate.retired_instructions == 0) {
        DbtBlockPlan plan =
            plan_fallback(candidate.start_pc, candidate.end_pc, 0, candidate.start_pc, "empty-hot-path");
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
