#include "control_flow_ops.h"

#include "../cpu.h"
#include "../isa/effects.h"

namespace {

constexpr uint64_t CAUSE_ILLEGAL_INSN = 2;

TrapRequest illegal_instruction_trap(uint32_t raw) {
    TrapRequest trap;
    trap.valid = true;
    trap.cause = CAUSE_ILLEGAL_INSN;
    trap.tval = raw;
    return trap;
}

void set_rd(InsnEffects& effects, uint8_t rd, uint64_t value) {
    effects.rd_write.enable = true;
    effects.rd_write.rd = rd;
    effects.rd_write.value = value;
}

bool apply_control_flow_effects(CPU& cpu, const InsnEffects& effects, uint64_t& next_pc) {
    if (effects.trap.valid) {
        cpu.trap().enter_exception(effects.trap.cause, effects.trap.tval);
        return false;
    }
    if (effects.rd_write.enable) {
        cpu.core().write_gpr(effects.rd_write.rd, effects.rd_write.value);
    }
    if (effects.control.redirect_pc) {
        next_pc = effects.control.target_pc;
    }
    return effects.retired;
}

}  // namespace

InsnEffects build_control_flow_effects(const Insn& insn, uint64_t rs1v, uint64_t rs2v, int64_t imm, uint64_t pc) {
    InsnEffects effects;
    const uint64_t next_pc = pc + 4;

    switch (insn.opcode) {
    case 0x6F:
        set_rd(effects, insn.rd, next_pc);
        effects.control.redirect_pc = true;
        effects.control.target_pc = pc + static_cast<uint64_t>(imm);
        return effects;
    case 0x67:
        set_rd(effects, insn.rd, next_pc);
        effects.control.redirect_pc = true;
        effects.control.target_pc = (rs1v + static_cast<uint64_t>(imm)) & ~1ULL;
        return effects;
    case 0x63: {
        int taken = 0;
        switch (insn.funct3) {
        case 0:
            taken = rs1v == rs2v;
            break;
        case 1:
            taken = rs1v != rs2v;
            break;
        case 4:
            taken = static_cast<int64_t>(rs1v) < static_cast<int64_t>(rs2v);
            break;
        case 5:
            taken = static_cast<int64_t>(rs1v) >= static_cast<int64_t>(rs2v);
            break;
        case 6:
            taken = rs1v < rs2v;
            break;
        case 7:
            taken = rs1v >= rs2v;
            break;
        default:
            effects.trap = illegal_instruction_trap(insn.raw);
            effects.retired = false;
            return effects;
        }
        if (taken) {
            effects.control.redirect_pc = true;
            effects.control.target_pc = pc + static_cast<uint64_t>(imm);
        }
        return effects;
    }
    default:
        effects.trap = illegal_instruction_trap(insn.raw);
        effects.retired = false;
        return effects;
    }
}

bool execute_control_flow_instruction(
    CPU& cpu,
    const Insn& insn,
    uint64_t rs1v,
    uint64_t rs2v,
    int64_t imm,
    uint64_t pc,
    uint64_t& next_pc) {
    switch (insn.opcode) {
    case 0x6F:
    case 0x67:
    case 0x63:
        return apply_control_flow_effects(cpu, build_control_flow_effects(insn, rs1v, rs2v, imm, pc), next_pc);
    default:
        return false;
    }
}
