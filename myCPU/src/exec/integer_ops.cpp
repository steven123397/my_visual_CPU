#include "integer_ops.h"

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

bool apply_integer_effects(CPU& cpu, const InsnEffects& effects) {
    if (effects.trap.valid) {
        cpu.trap().enter_exception(effects.trap.cause, effects.trap.tval);
        return false;
    }
    if (effects.rd_write.enable) {
        cpu.core().write_gpr(effects.rd_write.rd, effects.rd_write.value);
    }
    return effects.retired;
}

}  // namespace

InsnEffects build_integer_effects(const Insn& insn, uint64_t rs1v, uint64_t rs2v, int64_t imm, uint64_t pc) {
    InsnEffects effects;
    uint64_t val = 0;

    switch (insn.opcode) {
    case 0x37:
        set_rd(effects, insn.rd, static_cast<uint64_t>(imm));
        return effects;
    case 0x17:
        set_rd(effects, insn.rd, pc + static_cast<uint64_t>(imm));
        return effects;
    case 0x13: {
        const uint8_t shamt = static_cast<uint8_t>(insn.rs2 | ((insn.funct7 & 1U) << 5));
        switch (insn.funct3) {
        case 0:
            val = rs1v + static_cast<uint64_t>(imm);
            break;
        case 1:
            val = rs1v << shamt;
            break;
        case 2:
            val = static_cast<int64_t>(rs1v) < imm;
            break;
        case 3:
            val = rs1v < static_cast<uint64_t>(imm);
            break;
        case 4:
            val = rs1v ^ static_cast<uint64_t>(imm);
            break;
        case 5:
            val = (insn.funct7 & 0x20U) ? static_cast<uint64_t>(static_cast<int64_t>(rs1v) >> shamt) : (rs1v >> shamt);
            break;
        case 6:
            val = rs1v | static_cast<uint64_t>(imm);
            break;
        case 7:
            val = rs1v & static_cast<uint64_t>(imm);
            break;
        default:
            effects.trap = illegal_instruction_trap(insn.raw);
            effects.retired = false;
            return effects;
        }
        set_rd(effects, insn.rd, val);
        return effects;
    }
    case 0x1B: {
        const uint8_t shamt = insn.rs2;
        switch (insn.funct3) {
        case 0:
            val = static_cast<uint64_t>(static_cast<int64_t>(static_cast<int32_t>(rs1v + static_cast<uint64_t>(imm))));
            break;
        case 1:
            val = static_cast<uint64_t>(static_cast<int64_t>(static_cast<int32_t>(static_cast<uint32_t>(rs1v) << shamt)));
            break;
        case 5:
            val = (insn.funct7 & 0x20U)
                ? static_cast<uint64_t>(static_cast<int64_t>(static_cast<int32_t>(static_cast<int32_t>(rs1v) >> shamt)))
                : static_cast<uint64_t>(static_cast<int64_t>(static_cast<int32_t>(static_cast<uint32_t>(rs1v) >> shamt)));
            break;
        default:
            effects.trap = illegal_instruction_trap(insn.raw);
            effects.retired = false;
            return effects;
        }
        set_rd(effects, insn.rd, val);
        return effects;
    }
    case 0x33: {
        const uint8_t shamt = static_cast<uint8_t>(rs2v & 63U);
        if (insn.funct7 == 1) {
            switch (insn.funct3) {
            case 0:
                val = rs1v * rs2v;
                break;
            case 1:
                val = static_cast<uint64_t>((static_cast<__int128>(static_cast<int64_t>(rs1v)) * static_cast<int64_t>(rs2v)) >> 64);
                break;
            case 2:
                val = static_cast<uint64_t>((static_cast<__int128>(static_cast<int64_t>(rs1v)) * rs2v) >> 64);
                break;
            case 3:
                val = static_cast<uint64_t>((static_cast<__uint128_t>(rs1v) * rs2v) >> 64);
                break;
            case 4:
                val = rs2v ? static_cast<uint64_t>(static_cast<int64_t>(rs1v) / static_cast<int64_t>(rs2v)) : ~0ULL;
                break;
            case 5:
                val = rs2v ? rs1v / rs2v : ~0ULL;
                break;
            case 6:
                val = rs2v ? static_cast<uint64_t>(static_cast<int64_t>(rs1v) % static_cast<int64_t>(rs2v)) : rs1v;
                break;
            case 7:
                val = rs2v ? rs1v % rs2v : rs1v;
                break;
            default:
                effects.trap = illegal_instruction_trap(insn.raw);
                effects.retired = false;
                return effects;
            }
        } else {
            switch (insn.funct3) {
            case 0:
                val = (insn.funct7 & 0x20U) ? (rs1v - rs2v) : (rs1v + rs2v);
                break;
            case 1:
                val = rs1v << shamt;
                break;
            case 2:
                val = static_cast<int64_t>(rs1v) < static_cast<int64_t>(rs2v);
                break;
            case 3:
                val = rs1v < rs2v;
                break;
            case 4:
                val = rs1v ^ rs2v;
                break;
            case 5:
                val = (insn.funct7 & 0x20U) ? static_cast<uint64_t>(static_cast<int64_t>(rs1v) >> shamt) : (rs1v >> shamt);
                break;
            case 6:
                val = rs1v | rs2v;
                break;
            case 7:
                val = rs1v & rs2v;
                break;
            default:
                effects.trap = illegal_instruction_trap(insn.raw);
                effects.retired = false;
                return effects;
            }
        }
        set_rd(effects, insn.rd, val);
        return effects;
    }
    case 0x3B: {
        const uint8_t shamt = static_cast<uint8_t>(rs2v & 31U);
        if (insn.funct7 == 1) {
            switch (insn.funct3) {
            case 0:
                val = static_cast<uint64_t>(static_cast<int64_t>(static_cast<int32_t>(rs1v * rs2v)));
                break;
            case 4:
                val = rs2v ? static_cast<uint64_t>(static_cast<int64_t>(static_cast<int32_t>(rs1v) / static_cast<int32_t>(rs2v))) : UINT64_MAX;
                break;
            case 5:
                val = rs2v ? static_cast<uint64_t>(static_cast<int64_t>(static_cast<int32_t>(static_cast<uint32_t>(rs1v) / static_cast<uint32_t>(rs2v)))) : UINT64_MAX;
                break;
            case 6:
                val = rs2v ? static_cast<uint64_t>(static_cast<int64_t>(static_cast<int32_t>(rs1v) % static_cast<int32_t>(rs2v))) : static_cast<uint64_t>(static_cast<int64_t>(static_cast<int32_t>(rs1v)));
                break;
            case 7:
                val = rs2v ? static_cast<uint64_t>(static_cast<int64_t>(static_cast<int32_t>(static_cast<uint32_t>(rs1v) % static_cast<uint32_t>(rs2v)))) : static_cast<uint64_t>(static_cast<int64_t>(static_cast<int32_t>(rs1v)));
                break;
            default:
                effects.trap = illegal_instruction_trap(insn.raw);
                effects.retired = false;
                return effects;
            }
        } else {
            switch (insn.funct3) {
            case 0:
                val = static_cast<uint64_t>(static_cast<int64_t>(static_cast<int32_t>((insn.funct7 & 0x20U) ? (rs1v - rs2v) : (rs1v + rs2v))));
                break;
            case 1:
                val = static_cast<uint64_t>(static_cast<int64_t>(static_cast<int32_t>(static_cast<uint32_t>(rs1v) << shamt)));
                break;
            case 5:
                val = (insn.funct7 & 0x20U)
                    ? static_cast<uint64_t>(static_cast<int64_t>(static_cast<int32_t>(static_cast<int32_t>(rs1v) >> shamt)))
                    : static_cast<uint64_t>(static_cast<int64_t>(static_cast<int32_t>(static_cast<uint32_t>(rs1v) >> shamt)));
                break;
            default:
                effects.trap = illegal_instruction_trap(insn.raw);
                effects.retired = false;
                return effects;
            }
        }
        set_rd(effects, insn.rd, val);
        return effects;
    }
    case 0x0F:
        return effects;
    default:
        effects.trap = illegal_instruction_trap(insn.raw);
        effects.retired = false;
        return effects;
    }
}

bool execute_integer_instruction(CPU& cpu, const Insn& insn, uint64_t rs1v, uint64_t rs2v, int64_t imm, uint64_t pc) {
    switch (insn.opcode) {
    case 0x37:
    case 0x17:
    case 0x13:
    case 0x1B:
    case 0x33:
    case 0x3B:
    case 0x0F:
        return apply_integer_effects(cpu, build_integer_effects(insn, rs1v, rs2v, imm, pc));
    default:
        return false;
    }
}
