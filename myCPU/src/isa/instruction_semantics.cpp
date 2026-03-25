#include "instruction_semantics.h"

#include "execution_context.h"

#include "../arch/core_state.h"
#include "../exec/control_flow_ops.h"
#include "../exec/integer_ops.h"
#include "../exec/memory_ops.h"
#include "../exec/system_ops.h"

namespace {

constexpr uint64_t CAUSE_ILLEGAL_INSN = 2;

InsnEffects illegal_instruction_effect(uint32_t raw) {
    InsnEffects effects;
    effects.trap.valid = true;
    effects.trap.cause = CAUSE_ILLEGAL_INSN;
    effects.trap.tval = raw;
    effects.retired = false;
    return effects;
}

}  // namespace

bool InstructionSemantics::supports(const Insn& insn) {
    switch (insn.opcode) {
    case 0x37:
    case 0x17:
    case 0x13:
    case 0x1B:
    case 0x33:
    case 0x3B:
    case 0x0F:
    case 0x6F:
    case 0x67:
    case 0x63:
    case 0x03:
    case 0x23:
    case 0x73:
        return true;
    default:
        return false;
    }
}

InsnEffects InstructionSemantics::execute(const Insn& insn, ExecutionContext& ctx) {
    SemanticInputs inputs;
    inputs.pc = ctx.core().pc();
    inputs.rs1v = ctx.core().read_gpr(insn.rs1);
    inputs.rs2v = ctx.core().read_gpr(insn.rs2);
    return execute(insn, ctx, inputs);
}

InsnEffects InstructionSemantics::execute(const Insn& insn, ExecutionContext& ctx, const SemanticInputs& inputs) {
    switch (insn.opcode) {
    case 0x37:
    case 0x17:
    case 0x13:
    case 0x1B:
    case 0x33:
    case 0x3B:
    case 0x0F:
        return build_integer_effects(insn, inputs.rs1v, inputs.rs2v, insn.imm, inputs.pc);
    case 0x6F:
    case 0x67:
    case 0x63:
        return build_control_flow_effects(insn, inputs.rs1v, inputs.rs2v, insn.imm, inputs.pc);
    case 0x03:
    case 0x23:
        return build_memory_effects(insn, inputs.rs1v, inputs.rs2v, insn.imm);
    case 0x73:
        return build_system_effects(insn, ctx, inputs);
    default:
        return illegal_instruction_effect(insn.raw);
    }
}
