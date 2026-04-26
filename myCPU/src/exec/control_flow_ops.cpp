#include "control_flow_ops.h"

#include "../cpu.h"
#include "../isa/effects.h"

namespace {

constexpr uint64_t CAUSE_INSN_ADDR_MISALIGNED = 0;
constexpr uint64_t CAUSE_ILLEGAL_INSN = 2;

uint8_t instruction_size(const Insn& insn) {
    return insn.size != 0 ? insn.size : 4;
}

TrapRequest illegal_instruction_trap(uint32_t raw) {
    TrapRequest trap;
    trap.valid = true;
    trap.cause = CAUSE_ILLEGAL_INSN;
    trap.tval = raw;
    return trap;
}

TrapRequest instruction_address_misaligned_trap(uint64_t target_pc) {
    TrapRequest trap;
    trap.valid = true;
    trap.cause = CAUSE_INSN_ADDR_MISALIGNED;
    trap.tval = target_pc;
    return trap;
}

bool is_instruction_aligned(uint64_t pc) {
    return (pc & 0x1ULL) == 0;
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
    const uint64_t next_pc = pc + instruction_size(insn);

    switch (insn.opcode) {
    case 0x6F: {
        const uint64_t target = pc + static_cast<uint64_t>(imm);
        if (!is_instruction_aligned(target)) {
            effects.trap = instruction_address_misaligned_trap(target);
            effects.retired = false;
            return effects;
        }
        set_rd(effects, insn.rd, next_pc);
        effects.control.redirect_pc = true;
        effects.control.target_pc = target;
        return effects;
    }
    case 0x67: {
        const uint64_t target = (rs1v + static_cast<uint64_t>(imm)) & ~1ULL;
        if (!is_instruction_aligned(target)) {
            effects.trap = instruction_address_misaligned_trap(target);
            effects.retired = false;
            return effects;
        }
        set_rd(effects, insn.rd, next_pc);
        effects.control.redirect_pc = true;
        effects.control.target_pc = target;
        return effects;
    }
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
            const uint64_t target = pc + static_cast<uint64_t>(imm);
            if (!is_instruction_aligned(target)) {
                effects.trap = instruction_address_misaligned_trap(target);
                effects.retired = false;
                return effects;
            }
            effects.control.redirect_pc = true;
            effects.control.target_pc = target;
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
