#include "dbt_ir_eval.h"

namespace {

void write_gpr(std::array<uint64_t, 32>& gpr, uint8_t rd, uint64_t value) {
    if (rd != 0 && rd < gpr.size()) {
        gpr[rd] = value;
    }
    gpr[0] = 0;
}

uint64_t sign_extend_word(uint32_t value) {
    return static_cast<uint64_t>(static_cast<int64_t>(static_cast<int32_t>(value)));
}

DbtIrEvaluationResult reject_eval(const DbtTranslationUnit& unit, const DbtIrEvaluationInput& input) {
    return DbtIrEvaluationResult{
        .ok = false,
        .reject_kind = unit.reject_kind,
        .reject_reason = unit.reject_reason,
        .gpr = input.gpr,
        .next_pc = input.pc,
    };
}

}  // namespace

DbtIrEvaluationResult evaluate_dbt_ir_unit(const DbtTranslationUnit& unit,
                                           const DbtIrEvaluationInput& input) {
    if (!unit.ok) {
        return reject_eval(unit, input);
    }

    DbtIrEvaluationResult result{
        .ok = true,
        .gpr = input.gpr,
        .next_pc = input.pc,
    };
    result.gpr[0] = 0;

    for (const DbtIrInstruction& instruction : unit.instructions) {
        switch (instruction.opcode) {
        case DbtIrOpcode::WriteRegImm:
            write_gpr(result.gpr, instruction.rd, static_cast<uint64_t>(instruction.imm));
            result.next_pc = instruction.next_pc;
            result.retired_instructions += 1;
            break;
        case DbtIrOpcode::AddPcImm:
            write_gpr(result.gpr,
                      instruction.rd,
                      instruction.pc + static_cast<uint64_t>(instruction.imm));
            result.next_pc = instruction.next_pc;
            result.retired_instructions += 1;
            break;
        case DbtIrOpcode::AddRegImm:
            write_gpr(result.gpr,
                      instruction.rd,
                      result.gpr[instruction.rs1] + static_cast<uint64_t>(instruction.imm));
            result.next_pc = instruction.next_pc;
            result.retired_instructions += 1;
            break;
        case DbtIrOpcode::AddRegImmWord:
            write_gpr(result.gpr,
                      instruction.rd,
                      sign_extend_word(static_cast<uint32_t>(result.gpr[instruction.rs1] +
                                                             static_cast<uint64_t>(instruction.imm))));
            result.next_pc = instruction.next_pc;
            result.retired_instructions += 1;
            break;
        case DbtIrOpcode::XorRegImm:
            write_gpr(result.gpr,
                      instruction.rd,
                      result.gpr[instruction.rs1] ^ static_cast<uint64_t>(instruction.imm));
            result.next_pc = instruction.next_pc;
            result.retired_instructions += 1;
            break;
        case DbtIrOpcode::OrRegImm:
            write_gpr(result.gpr,
                      instruction.rd,
                      result.gpr[instruction.rs1] | static_cast<uint64_t>(instruction.imm));
            result.next_pc = instruction.next_pc;
            result.retired_instructions += 1;
            break;
        case DbtIrOpcode::AndRegImm:
            write_gpr(result.gpr,
                      instruction.rd,
                      result.gpr[instruction.rs1] & static_cast<uint64_t>(instruction.imm));
            result.next_pc = instruction.next_pc;
            result.retired_instructions += 1;
            break;
        case DbtIrOpcode::ShiftLeftRegImm:
            write_gpr(result.gpr,
                      instruction.rd,
                      result.gpr[instruction.rs1] << (static_cast<uint8_t>(instruction.imm) & 63U));
            result.next_pc = instruction.next_pc;
            result.retired_instructions += 1;
            break;
        case DbtIrOpcode::ShiftRightLogicalRegImm:
            write_gpr(result.gpr,
                      instruction.rd,
                      result.gpr[instruction.rs1] >> (static_cast<uint8_t>(instruction.imm) & 63U));
            result.next_pc = instruction.next_pc;
            result.retired_instructions += 1;
            break;
        case DbtIrOpcode::ShiftRightArithmeticRegImm:
            write_gpr(result.gpr,
                      instruction.rd,
                      static_cast<uint64_t>(static_cast<int64_t>(result.gpr[instruction.rs1]) >>
                                            (static_cast<uint8_t>(instruction.imm) & 63U)));
            result.next_pc = instruction.next_pc;
            result.retired_instructions += 1;
            break;
        case DbtIrOpcode::ShiftLeftRegImmWord:
            write_gpr(result.gpr,
                      instruction.rd,
                      sign_extend_word(static_cast<uint32_t>(result.gpr[instruction.rs1]) <<
                                       (static_cast<uint8_t>(instruction.imm) & 31U)));
            result.next_pc = instruction.next_pc;
            result.retired_instructions += 1;
            break;
        case DbtIrOpcode::ShiftRightLogicalRegImmWord:
            write_gpr(result.gpr,
                      instruction.rd,
                      sign_extend_word(static_cast<uint32_t>(result.gpr[instruction.rs1]) >>
                                       (static_cast<uint8_t>(instruction.imm) & 31U)));
            result.next_pc = instruction.next_pc;
            result.retired_instructions += 1;
            break;
        case DbtIrOpcode::ShiftRightArithmeticRegImmWord:
            write_gpr(result.gpr,
                      instruction.rd,
                      sign_extend_word(static_cast<uint32_t>(
                          static_cast<int32_t>(result.gpr[instruction.rs1]) >>
                          (static_cast<uint8_t>(instruction.imm) & 31U))));
            result.next_pc = instruction.next_pc;
            result.retired_instructions += 1;
            break;
        case DbtIrOpcode::SetLessThanRegImm:
            write_gpr(result.gpr,
                      instruction.rd,
                      static_cast<int64_t>(result.gpr[instruction.rs1]) < instruction.imm ? 1U : 0U);
            result.next_pc = instruction.next_pc;
            result.retired_instructions += 1;
            break;
        case DbtIrOpcode::SetLessThanUnsignedRegImm:
            write_gpr(result.gpr,
                      instruction.rd,
                      result.gpr[instruction.rs1] < static_cast<uint64_t>(instruction.imm) ? 1U : 0U);
            result.next_pc = instruction.next_pc;
            result.retired_instructions += 1;
            break;
        case DbtIrOpcode::AddRegReg:
            write_gpr(result.gpr,
                      instruction.rd,
                      result.gpr[instruction.rs1] + result.gpr[instruction.rs2]);
            result.next_pc = instruction.next_pc;
            result.retired_instructions += 1;
            break;
        case DbtIrOpcode::SubRegReg:
            write_gpr(result.gpr,
                      instruction.rd,
                      result.gpr[instruction.rs1] - result.gpr[instruction.rs2]);
            result.next_pc = instruction.next_pc;
            result.retired_instructions += 1;
            break;
        case DbtIrOpcode::AddRegRegWord:
            write_gpr(result.gpr,
                      instruction.rd,
                      sign_extend_word(static_cast<uint32_t>(result.gpr[instruction.rs1] +
                                                             result.gpr[instruction.rs2])));
            result.next_pc = instruction.next_pc;
            result.retired_instructions += 1;
            break;
        case DbtIrOpcode::SubRegRegWord:
            write_gpr(result.gpr,
                      instruction.rd,
                      sign_extend_word(static_cast<uint32_t>(result.gpr[instruction.rs1] -
                                                             result.gpr[instruction.rs2])));
            result.next_pc = instruction.next_pc;
            result.retired_instructions += 1;
            break;
        case DbtIrOpcode::XorRegReg:
            write_gpr(result.gpr,
                      instruction.rd,
                      result.gpr[instruction.rs1] ^ result.gpr[instruction.rs2]);
            result.next_pc = instruction.next_pc;
            result.retired_instructions += 1;
            break;
        case DbtIrOpcode::OrRegReg:
            write_gpr(result.gpr,
                      instruction.rd,
                      result.gpr[instruction.rs1] | result.gpr[instruction.rs2]);
            result.next_pc = instruction.next_pc;
            result.retired_instructions += 1;
            break;
        case DbtIrOpcode::AndRegReg:
            write_gpr(result.gpr,
                      instruction.rd,
                      result.gpr[instruction.rs1] & result.gpr[instruction.rs2]);
            result.next_pc = instruction.next_pc;
            result.retired_instructions += 1;
            break;
        case DbtIrOpcode::ShiftLeftRegReg:
            write_gpr(result.gpr,
                      instruction.rd,
                      result.gpr[instruction.rs1] << (result.gpr[instruction.rs2] & 63U));
            result.next_pc = instruction.next_pc;
            result.retired_instructions += 1;
            break;
        case DbtIrOpcode::ShiftRightLogicalRegReg:
            write_gpr(result.gpr,
                      instruction.rd,
                      result.gpr[instruction.rs1] >> (result.gpr[instruction.rs2] & 63U));
            result.next_pc = instruction.next_pc;
            result.retired_instructions += 1;
            break;
        case DbtIrOpcode::ShiftRightArithmeticRegReg:
            write_gpr(result.gpr,
                      instruction.rd,
                      static_cast<uint64_t>(static_cast<int64_t>(result.gpr[instruction.rs1]) >>
                                            (result.gpr[instruction.rs2] & 63U)));
            result.next_pc = instruction.next_pc;
            result.retired_instructions += 1;
            break;
        case DbtIrOpcode::ShiftLeftRegRegWord:
            write_gpr(result.gpr,
                      instruction.rd,
                      sign_extend_word(static_cast<uint32_t>(result.gpr[instruction.rs1]) <<
                                       (result.gpr[instruction.rs2] & 31U)));
            result.next_pc = instruction.next_pc;
            result.retired_instructions += 1;
            break;
        case DbtIrOpcode::ShiftRightLogicalRegRegWord:
            write_gpr(result.gpr,
                      instruction.rd,
                      sign_extend_word(static_cast<uint32_t>(result.gpr[instruction.rs1]) >>
                                       (result.gpr[instruction.rs2] & 31U)));
            result.next_pc = instruction.next_pc;
            result.retired_instructions += 1;
            break;
        case DbtIrOpcode::ShiftRightArithmeticRegRegWord:
            write_gpr(result.gpr,
                      instruction.rd,
                      sign_extend_word(static_cast<uint32_t>(
                          static_cast<int32_t>(result.gpr[instruction.rs1]) >>
                          (result.gpr[instruction.rs2] & 31U))));
            result.next_pc = instruction.next_pc;
            result.retired_instructions += 1;
            break;
        case DbtIrOpcode::SetLessThanRegReg:
            write_gpr(result.gpr,
                      instruction.rd,
                      static_cast<int64_t>(result.gpr[instruction.rs1]) <
                              static_cast<int64_t>(result.gpr[instruction.rs2])
                          ? 1U
                          : 0U);
            result.next_pc = instruction.next_pc;
            result.retired_instructions += 1;
            break;
        case DbtIrOpcode::SetLessThanUnsignedRegReg:
            write_gpr(result.gpr,
                      instruction.rd,
                      result.gpr[instruction.rs1] < result.gpr[instruction.rs2] ? 1U : 0U);
            result.next_pc = instruction.next_pc;
            result.retired_instructions += 1;
            break;
        case DbtIrOpcode::Fallthrough:
            result.next_pc = instruction.next_pc;
            break;
        }
    }

    result.gpr[0] = 0;
    return result;
}
