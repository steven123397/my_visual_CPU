#include "interpreter_dbt_prototype.h"

#include <utility>

#include "../cpu.h"
#include "../isa/execution_context.h"
#include "../isa/instruction_semantics.h"
#include "../mem/bus.h"
#include "functional_backend.h"
#include "pipeline_commit_boundary.h"

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

InterpreterDbtPrototypeResult fallback(uint64_t start_pc,
                                       uint64_t end_pc,
                                       uint64_t retired,
                                       uint64_t pc,
                                       std::string reason) {
    return InterpreterDbtPrototypeResult{
        .ok = false,
        .start_pc = start_pc,
        .end_pc = end_pc,
        .retired_instructions = retired,
        .fallback_pc = pc,
        .fallback_reason = std::move(reason),
    };
}

}  // namespace

InterpreterDbtPrototypePlan plan_interpreter_dbt_prototype_block(CPU& cpu,
                                                                 Bus& bus,
                                                                 uint64_t start_pc,
                                                                 uint64_t end_pc) {
    return plan_dbt_block(cpu, bus, start_pc, end_pc);
}

InterpreterDbtPrototypePlan plan_interpreter_dbt_prototype_hot_path(
    CPU& cpu,
    Bus& bus,
    const ExecutionProfileSnapshot& profile) {
    return plan_dbt_hot_path(cpu, bus, profile);
}

InterpreterDbtPrototypeResult run_interpreter_dbt_prototype_block(CPU& cpu,
                                                                  Bus& bus,
                                                                  uint64_t start_pc,
                                                                  uint64_t end_pc) {
    const InterpreterDbtPrototypePlan plan =
        plan_interpreter_dbt_prototype_block(cpu, bus, start_pc, end_pc);
    if (!plan.ok) {
        return fallback(start_pc, end_pc, 0, plan.fallback_pc, plan.fallback_reason);
    }

    uint64_t retired = 0;
    for (uint64_t pc = start_pc; pc <= end_pc;) {
        if (cpu.core().pc() != pc) {
            return fallback(start_pc, end_pc, retired, pc, "pc-diverged");
        }

        const FetchDecodeResult fetched = fetch_decode(cpu, bus, pc);
        if (!fetched.ok) {
            return fallback(start_pc, end_pc, retired, pc, fetched.fallback_reason);
        }

        const Insn& insn = fetched.insn;
        if (!InstructionSemantics::supports(insn) || is_control_flow_instruction(insn)) {
            return fallback(start_pc, end_pc, retired, pc, "fallback-required");
        }

        ExecutionContext ctx(cpu, bus);
        const InsnEffects effects = InstructionSemantics::execute(insn, ctx);
        if (effects.trap.valid || !effects.retired || requires_fallback(effects)) {
            return fallback(start_pc, end_pc, retired, pc, "fallback-required");
        }
        if (requires_helper(effects)) {
            return fallback(start_pc, end_pc, retired, pc, "helper-required");
        }

        const uint64_t next_pc = pc + instruction_size(insn);
        const CommitBoundaryResult result =
            apply_commit_boundary(cpu,
                                  bus,
                                  CommitBoundaryInput{
                                      .pc = pc,
                                      .next_pc = next_pc,
                                      .effects = effects,
                                  });
        if (!result.retired || result.trap_taken || result.next_pc != next_pc) {
            return fallback(start_pc, end_pc, retired, pc, "fallback-required");
        }

        cpu.core().advance_cycle();
        retired += 1;
        pc = next_pc;
    }

    return InterpreterDbtPrototypeResult{
        .ok = true,
        .start_pc = start_pc,
        .end_pc = end_pc,
        .retired_instructions = retired,
    };
}

InterpreterDbtPrototypeResult run_interpreter_dbt_prototype_with_functional_fallback(
    CPU& cpu,
    Bus& bus,
    uint64_t start_pc,
    uint64_t end_pc) {
    const InterpreterDbtPrototypePlan plan =
        plan_interpreter_dbt_prototype_block(cpu, bus, start_pc, end_pc);
    if (plan.ok) {
        return run_interpreter_dbt_prototype_block(cpu, bus, start_pc, end_pc);
    }

    FunctionalBackend fallback_backend(cpu, bus);
    const uint64_t instret_before = cpu.core().instret();
    const uint64_t replay_steps = plan.inlineable_instructions + 1;
    for (uint64_t i = 0; i < replay_steps; ++i) {
        fallback_backend.step();
        if (cpu.core().halted()) {
            break;
        }
    }

    return InterpreterDbtPrototypeResult{
        .ok = true,
        .used_fallback = true,
        .start_pc = start_pc,
        .end_pc = end_pc,
        .retired_instructions = cpu.core().instret() - instret_before,
        .fallback_pc = plan.fallback_pc,
        .fallback_reason = plan.fallback_reason,
    };
}

InterpreterDbtInvalidationPlan plan_interpreter_dbt_invalidation_event(
    InterpreterDbtInvalidationEventKind kind,
    uint64_t event_addr,
    uint64_t event_size,
    uint64_t block_start_pc,
    uint64_t block_end_pc) {
    return plan_dbt_block_invalidation_event(kind,
                                             event_addr,
                                             event_size,
                                             block_start_pc,
                                             block_end_pc);
}
