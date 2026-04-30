#include "dbt_ir_eval.h"

namespace {

void write_gpr(std::array<uint64_t, 32>& gpr, uint8_t rd, uint64_t value) {
    if (rd != 0 && rd < gpr.size()) {
        gpr[rd] = value;
    }
    gpr[0] = 0;
}

DbtIrEvaluationResult reject_eval(const DbtTranslationUnit& unit, const DbtIrEvaluationInput& input) {
    return DbtIrEvaluationResult{
        .ok = false,
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
        case DbtIrOpcode::AddRegImm:
            write_gpr(result.gpr,
                      instruction.rd,
                      result.gpr[instruction.rs1] + static_cast<uint64_t>(instruction.imm));
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
        case DbtIrOpcode::Fallthrough:
            result.next_pc = instruction.next_pc;
            break;
        }
    }

    result.gpr[0] = 0;
    return result;
}
