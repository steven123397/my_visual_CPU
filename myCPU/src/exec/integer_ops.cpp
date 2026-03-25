#include "integer_ops.h"

#include <limits>

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

bool is_valid_shift_immediate_encoding(const Insn& insn) {
    const uint8_t upper = insn.funct7 & 0x7EU;
    switch (insn.funct3) {
    case 1:
        return upper == 0x00;
    case 5:
        return upper == 0x00 || upper == 0x20;
    default:
        return true;
    }
}

bool is_valid_shift_word_immediate_encoding(const Insn& insn) {
    switch (insn.funct3) {
    case 1:
        return insn.funct7 == 0x00;
    case 5:
        return insn.funct7 == 0x00 || insn.funct7 == 0x20;
    default:
        return true;
    }
}

bool is_valid_integer_reg_encoding(uint8_t funct3, uint8_t funct7) {
    switch (funct3) {
    case 0:
        return funct7 == 0x00 || funct7 == 0x20;
    case 1:
    case 2:
    case 3:
    case 4:
    case 6:
    case 7:
        return funct7 == 0x00;
    case 5:
        return funct7 == 0x00 || funct7 == 0x20;
    default:
        return false;
    }
}

bool is_valid_integer_word_reg_encoding(uint8_t funct3, uint8_t funct7) {
    switch (funct3) {
    case 0:
        return funct7 == 0x00 || funct7 == 0x20;
    case 1:
        return funct7 == 0x00;
    case 5:
        return funct7 == 0x00 || funct7 == 0x20;
    default:
        return false;
    }
}

uint64_t sign_extend_word(int32_t value) {
    return static_cast<uint64_t>(static_cast<int64_t>(value));
}

uint64_t div_signed(uint64_t lhs, uint64_t rhs) {
    if (rhs == 0) {
        return ~0ULL;
    }

    const int64_t dividend = static_cast<int64_t>(lhs);
    const int64_t divisor = static_cast<int64_t>(rhs);
    if (dividend == std::numeric_limits<int64_t>::min() && divisor == -1) {
        return lhs;
    }

    return static_cast<uint64_t>(dividend / divisor);
}

uint64_t rem_signed(uint64_t lhs, uint64_t rhs) {
    if (rhs == 0) {
        return lhs;
    }

    const int64_t dividend = static_cast<int64_t>(lhs);
    const int64_t divisor = static_cast<int64_t>(rhs);
    if (dividend == std::numeric_limits<int64_t>::min() && divisor == -1) {
        return 0;
    }

    return static_cast<uint64_t>(dividend % divisor);
}

uint64_t div_signed_word(uint64_t lhs, uint64_t rhs) {
    const int32_t dividend = static_cast<int32_t>(lhs);
    const int32_t divisor = static_cast<int32_t>(rhs);
    if (divisor == 0) {
        return UINT64_MAX;
    }
    if (dividend == std::numeric_limits<int32_t>::min() && divisor == -1) {
        return sign_extend_word(dividend);
    }

    return sign_extend_word(static_cast<int32_t>(dividend / divisor));
}

uint64_t rem_signed_word(uint64_t lhs, uint64_t rhs) {
    const int32_t dividend = static_cast<int32_t>(lhs);
    const int32_t divisor = static_cast<int32_t>(rhs);
    if (divisor == 0) {
        return sign_extend_word(dividend);
    }
    if (dividend == std::numeric_limits<int32_t>::min() && divisor == -1) {
        return 0;
    }

    return sign_extend_word(static_cast<int32_t>(dividend % divisor));
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
            if (!is_valid_shift_immediate_encoding(insn)) {
                effects.trap = illegal_instruction_trap(insn.raw);
                effects.retired = false;
                return effects;
            }
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
            if (!is_valid_shift_immediate_encoding(insn)) {
                effects.trap = illegal_instruction_trap(insn.raw);
                effects.retired = false;
                return effects;
            }
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
            if (!is_valid_shift_word_immediate_encoding(insn)) {
                effects.trap = illegal_instruction_trap(insn.raw);
                effects.retired = false;
                return effects;
            }
            val = static_cast<uint64_t>(static_cast<int64_t>(static_cast<int32_t>(static_cast<uint32_t>(rs1v) << shamt)));
            break;
        case 5:
            if (!is_valid_shift_word_immediate_encoding(insn)) {
                effects.trap = illegal_instruction_trap(insn.raw);
                effects.retired = false;
                return effects;
            }
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
                val = div_signed(rs1v, rs2v);
                break;
            case 5:
                val = rs2v ? rs1v / rs2v : ~0ULL;
                break;
            case 6:
                val = rem_signed(rs1v, rs2v);
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
            if (!is_valid_integer_reg_encoding(insn.funct3, insn.funct7)) {
                effects.trap = illegal_instruction_trap(insn.raw);
                effects.retired = false;
                return effects;
            }
            switch (insn.funct3) {
            case 0:
                val = (insn.funct7 == 0x20U) ? (rs1v - rs2v) : (rs1v + rs2v);
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
                val = (insn.funct7 == 0x20U) ? static_cast<uint64_t>(static_cast<int64_t>(rs1v) >> shamt) : (rs1v >> shamt);
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
                val = div_signed_word(rs1v, rs2v);
                break;
            case 5:
                val = rs2v ? static_cast<uint64_t>(static_cast<int64_t>(static_cast<int32_t>(static_cast<uint32_t>(rs1v) / static_cast<uint32_t>(rs2v)))) : UINT64_MAX;
                break;
            case 6:
                val = rem_signed_word(rs1v, rs2v);
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
            if (!is_valid_integer_word_reg_encoding(insn.funct3, insn.funct7)) {
                effects.trap = illegal_instruction_trap(insn.raw);
                effects.retired = false;
                return effects;
            }
            switch (insn.funct3) {
            case 0:
                val = static_cast<uint64_t>(static_cast<int64_t>(static_cast<int32_t>((insn.funct7 == 0x20U) ? (rs1v - rs2v) : (rs1v + rs2v))));
                break;
            case 1:
                val = static_cast<uint64_t>(static_cast<int64_t>(static_cast<int32_t>(static_cast<uint32_t>(rs1v) << shamt)));
                break;
            case 5:
                val = (insn.funct7 == 0x20U)
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
