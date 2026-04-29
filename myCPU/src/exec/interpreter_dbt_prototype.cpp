#include "interpreter_dbt_prototype.h"

#include <algorithm>
#include <utility>

#include "../cpu.h"
#include "execution_profile.h"
#include "functional_backend.h"
#include "../isa/execution_context.h"
#include "../isa/instruction_semantics.h"
#include "../mem/bus.h"
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

InterpreterDbtPrototypePlan plan_fallback(uint64_t start_pc,
                                          uint64_t end_pc,
                                          uint64_t inlineable,
                                          uint64_t pc,
                                          std::string reason) {
    return InterpreterDbtPrototypePlan{
        .ok = false,
        .start_pc = start_pc,
        .end_pc = end_pc,
        .inlineable_instructions = inlineable,
        .fallback_pc = pc,
        .fallback_reason = std::move(reason),
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

}  // namespace

InterpreterDbtPrototypePlan plan_interpreter_dbt_prototype_block(CPU& cpu,
                                                                 Bus& bus,
                                                                 uint64_t start_pc,
                                                                 uint64_t end_pc) {
    uint64_t inlineable = 0;
    for (uint64_t pc = start_pc; pc <= end_pc;) {
        const FetchDecodeResult fetched = fetch_decode(cpu, bus, pc);
        if (!fetched.ok) {
            return plan_fallback(start_pc, end_pc, inlineable, pc, fetched.fallback_reason);
        }

        const Insn& insn = fetched.insn;
        if (!InstructionSemantics::supports(insn)) {
            return plan_fallback(start_pc, end_pc, inlineable, pc, "fallback-required");
        }
        if (is_control_flow_instruction(insn)) {
            return plan_fallback(start_pc, end_pc, inlineable, pc, "fallback-required");
        }

        ExecutionContext ctx(cpu, bus);
        const InsnEffects effects = InstructionSemantics::execute(insn, ctx);
        if (effects.trap.valid || !effects.retired || requires_fallback(effects)) {
            return plan_fallback(start_pc, end_pc, inlineable, pc, "fallback-required");
        }
        if (requires_helper(effects)) {
            return plan_fallback(start_pc, end_pc, inlineable, pc, "helper-required");
        }

        inlineable += 1;
        pc += instruction_size(insn);
    }

    return InterpreterDbtPrototypePlan{
        .ok = true,
        .start_pc = start_pc,
        .end_pc = end_pc,
        .inlineable_instructions = inlineable,
    };
}

InterpreterDbtPrototypePlan plan_interpreter_dbt_prototype_hot_path(
    CPU& cpu,
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
        InterpreterDbtPrototypePlan plan =
            plan_fallback(candidate.start_pc, candidate.end_pc, 0, candidate.start_pc, "insufficient-repetition");
        plan.candidate_executions = candidate.executions;
        plan.candidate_retired_instructions = candidate.retired_instructions;
        return plan;
    }
    if (candidate.retired_instructions == 0) {
        InterpreterDbtPrototypePlan plan =
            plan_fallback(candidate.start_pc, candidate.end_pc, 0, candidate.start_pc, "empty-hot-path");
        plan.candidate_executions = candidate.executions;
        plan.candidate_retired_instructions = candidate.retired_instructions;
        return plan;
    }

    InterpreterDbtPrototypePlan plan =
        plan_interpreter_dbt_prototype_block(cpu, bus, candidate.start_pc, candidate.end_pc);
    plan.candidate_executions = candidate.executions;
    plan.candidate_retired_instructions = candidate.retired_instructions;
    return plan;
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
        if (!InstructionSemantics::supports(insn)) {
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
