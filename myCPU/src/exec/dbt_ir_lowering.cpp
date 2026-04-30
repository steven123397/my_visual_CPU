#include "dbt_ir_lowering.h"

namespace {

DbtIrLoweringResult reject_lowering(const DbtTranslationUnit& unit) {
    return DbtIrLoweringResult{
        .ok = false,
        .start_pc = unit.start_pc,
        .end_pc = unit.end_pc,
        .reject_kind = unit.reject_kind,
        .reject_pc = unit.reject_pc,
        .reject_raw = unit.reject_raw,
        .reject_reason = unit.reject_reason,
    };
}

DbtIrLoweringResult reject_unsupported_ir(const DbtTranslationUnit& unit,
                                          const DbtIrInstruction& instruction) {
    return DbtIrLoweringResult{
        .ok = false,
        .start_pc = unit.start_pc,
        .end_pc = unit.end_pc,
        .reject_kind = DbtRejectKind::UnsupportedIr,
        .reject_pc = instruction.pc,
        .reject_raw = instruction.raw,
        .reject_reason = "unsupported-ir",
    };
}

DbtLoweredInstruction base_compute(const DbtIrInstruction& instruction) {
    return DbtLoweredInstruction{
        .opcode = DbtLoweredOpcode::Compute,
        .pc = instruction.pc,
        .raw = instruction.raw,
        .rd = instruction.rd,
        .rs1 = instruction.rs1,
        .rs2 = instruction.rs2,
        .imm = instruction.imm,
        .next_pc = instruction.next_pc,
        .writes_gpr = instruction.rd != 0,
    };
}

DbtLoweredInstruction lower_move(const DbtIrInstruction& instruction,
                                 DbtLoweredOperandKind source_kind) {
    DbtLoweredInstruction lowered = base_compute(instruction);
    lowered.alu = DbtLoweredAluOp::Move;
    lowered.lhs_kind = source_kind;
    return lowered;
}

DbtLoweredInstruction lower_binary(const DbtIrInstruction& instruction,
                                   DbtLoweredAluOp op,
                                   DbtLoweredOperandKind lhs_kind,
                                   DbtLoweredOperandKind rhs_kind,
                                   DbtLoweredWidth width = DbtLoweredWidth::Xlen) {
    DbtLoweredInstruction lowered = base_compute(instruction);
    lowered.alu = op;
    lowered.width = width;
    lowered.lhs_kind = lhs_kind;
    lowered.rhs_kind = rhs_kind;
    lowered.sign_extend_word = width == DbtLoweredWidth::Word;
    return lowered;
}

bool lower_ir_instruction(const DbtIrInstruction& instruction, DbtLoweredInstruction& lowered) {
    switch (instruction.opcode) {
    case DbtIrOpcode::WriteRegImm:
        lowered = lower_move(instruction, DbtLoweredOperandKind::Immediate);
        return true;
    case DbtIrOpcode::AddPcImm:
        lowered = lower_binary(instruction,
                               DbtLoweredAluOp::Add,
                               DbtLoweredOperandKind::Pc,
                               DbtLoweredOperandKind::Immediate);
        return true;
    case DbtIrOpcode::AddRegImm:
        lowered = lower_binary(instruction,
                               DbtLoweredAluOp::Add,
                               DbtLoweredOperandKind::Gpr,
                               DbtLoweredOperandKind::Immediate);
        return true;
    case DbtIrOpcode::AddRegImmWord:
        lowered = lower_binary(instruction,
                               DbtLoweredAluOp::Add,
                               DbtLoweredOperandKind::Gpr,
                               DbtLoweredOperandKind::Immediate,
                               DbtLoweredWidth::Word);
        return true;
    case DbtIrOpcode::XorRegImm:
        lowered = lower_binary(instruction,
                               DbtLoweredAluOp::Xor,
                               DbtLoweredOperandKind::Gpr,
                               DbtLoweredOperandKind::Immediate);
        return true;
    case DbtIrOpcode::OrRegImm:
        lowered = lower_binary(instruction,
                               DbtLoweredAluOp::Or,
                               DbtLoweredOperandKind::Gpr,
                               DbtLoweredOperandKind::Immediate);
        return true;
    case DbtIrOpcode::AndRegImm:
        lowered = lower_binary(instruction,
                               DbtLoweredAluOp::And,
                               DbtLoweredOperandKind::Gpr,
                               DbtLoweredOperandKind::Immediate);
        return true;
    case DbtIrOpcode::ShiftLeftRegImm:
        lowered = lower_binary(instruction,
                               DbtLoweredAluOp::ShiftLeft,
                               DbtLoweredOperandKind::Gpr,
                               DbtLoweredOperandKind::Immediate);
        return true;
    case DbtIrOpcode::ShiftRightLogicalRegImm:
        lowered = lower_binary(instruction,
                               DbtLoweredAluOp::ShiftRightLogical,
                               DbtLoweredOperandKind::Gpr,
                               DbtLoweredOperandKind::Immediate);
        return true;
    case DbtIrOpcode::ShiftRightArithmeticRegImm:
        lowered = lower_binary(instruction,
                               DbtLoweredAluOp::ShiftRightArithmetic,
                               DbtLoweredOperandKind::Gpr,
                               DbtLoweredOperandKind::Immediate);
        return true;
    case DbtIrOpcode::ShiftLeftRegImmWord:
        lowered = lower_binary(instruction,
                               DbtLoweredAluOp::ShiftLeft,
                               DbtLoweredOperandKind::Gpr,
                               DbtLoweredOperandKind::Immediate,
                               DbtLoweredWidth::Word);
        return true;
    case DbtIrOpcode::ShiftRightLogicalRegImmWord:
        lowered = lower_binary(instruction,
                               DbtLoweredAluOp::ShiftRightLogical,
                               DbtLoweredOperandKind::Gpr,
                               DbtLoweredOperandKind::Immediate,
                               DbtLoweredWidth::Word);
        return true;
    case DbtIrOpcode::ShiftRightArithmeticRegImmWord:
        lowered = lower_binary(instruction,
                               DbtLoweredAluOp::ShiftRightArithmetic,
                               DbtLoweredOperandKind::Gpr,
                               DbtLoweredOperandKind::Immediate,
                               DbtLoweredWidth::Word);
        return true;
    case DbtIrOpcode::SetLessThanRegImm:
        lowered = lower_binary(instruction,
                               DbtLoweredAluOp::SetLessThan,
                               DbtLoweredOperandKind::Gpr,
                               DbtLoweredOperandKind::Immediate);
        return true;
    case DbtIrOpcode::SetLessThanUnsignedRegImm:
        lowered = lower_binary(instruction,
                               DbtLoweredAluOp::SetLessThanUnsigned,
                               DbtLoweredOperandKind::Gpr,
                               DbtLoweredOperandKind::Immediate);
        return true;
    case DbtIrOpcode::AddRegReg:
        lowered = lower_binary(instruction,
                               DbtLoweredAluOp::Add,
                               DbtLoweredOperandKind::Gpr,
                               DbtLoweredOperandKind::Gpr);
        return true;
    case DbtIrOpcode::SubRegReg:
        lowered = lower_binary(instruction,
                               DbtLoweredAluOp::Sub,
                               DbtLoweredOperandKind::Gpr,
                               DbtLoweredOperandKind::Gpr);
        return true;
    case DbtIrOpcode::AddRegRegWord:
        lowered = lower_binary(instruction,
                               DbtLoweredAluOp::Add,
                               DbtLoweredOperandKind::Gpr,
                               DbtLoweredOperandKind::Gpr,
                               DbtLoweredWidth::Word);
        return true;
    case DbtIrOpcode::SubRegRegWord:
        lowered = lower_binary(instruction,
                               DbtLoweredAluOp::Sub,
                               DbtLoweredOperandKind::Gpr,
                               DbtLoweredOperandKind::Gpr,
                               DbtLoweredWidth::Word);
        return true;
    case DbtIrOpcode::XorRegReg:
        lowered = lower_binary(instruction,
                               DbtLoweredAluOp::Xor,
                               DbtLoweredOperandKind::Gpr,
                               DbtLoweredOperandKind::Gpr);
        return true;
    case DbtIrOpcode::OrRegReg:
        lowered = lower_binary(instruction,
                               DbtLoweredAluOp::Or,
                               DbtLoweredOperandKind::Gpr,
                               DbtLoweredOperandKind::Gpr);
        return true;
    case DbtIrOpcode::AndRegReg:
        lowered = lower_binary(instruction,
                               DbtLoweredAluOp::And,
                               DbtLoweredOperandKind::Gpr,
                               DbtLoweredOperandKind::Gpr);
        return true;
    case DbtIrOpcode::ShiftLeftRegReg:
        lowered = lower_binary(instruction,
                               DbtLoweredAluOp::ShiftLeft,
                               DbtLoweredOperandKind::Gpr,
                               DbtLoweredOperandKind::Gpr);
        return true;
    case DbtIrOpcode::ShiftRightLogicalRegReg:
        lowered = lower_binary(instruction,
                               DbtLoweredAluOp::ShiftRightLogical,
                               DbtLoweredOperandKind::Gpr,
                               DbtLoweredOperandKind::Gpr);
        return true;
    case DbtIrOpcode::ShiftRightArithmeticRegReg:
        lowered = lower_binary(instruction,
                               DbtLoweredAluOp::ShiftRightArithmetic,
                               DbtLoweredOperandKind::Gpr,
                               DbtLoweredOperandKind::Gpr);
        return true;
    case DbtIrOpcode::ShiftLeftRegRegWord:
        lowered = lower_binary(instruction,
                               DbtLoweredAluOp::ShiftLeft,
                               DbtLoweredOperandKind::Gpr,
                               DbtLoweredOperandKind::Gpr,
                               DbtLoweredWidth::Word);
        return true;
    case DbtIrOpcode::ShiftRightLogicalRegRegWord:
        lowered = lower_binary(instruction,
                               DbtLoweredAluOp::ShiftRightLogical,
                               DbtLoweredOperandKind::Gpr,
                               DbtLoweredOperandKind::Gpr,
                               DbtLoweredWidth::Word);
        return true;
    case DbtIrOpcode::ShiftRightArithmeticRegRegWord:
        lowered = lower_binary(instruction,
                               DbtLoweredAluOp::ShiftRightArithmetic,
                               DbtLoweredOperandKind::Gpr,
                               DbtLoweredOperandKind::Gpr,
                               DbtLoweredWidth::Word);
        return true;
    case DbtIrOpcode::SetLessThanRegReg:
        lowered = lower_binary(instruction,
                               DbtLoweredAluOp::SetLessThan,
                               DbtLoweredOperandKind::Gpr,
                               DbtLoweredOperandKind::Gpr);
        return true;
    case DbtIrOpcode::SetLessThanUnsignedRegReg:
        lowered = lower_binary(instruction,
                               DbtLoweredAluOp::SetLessThanUnsigned,
                               DbtLoweredOperandKind::Gpr,
                               DbtLoweredOperandKind::Gpr);
        return true;
    case DbtIrOpcode::Fallthrough:
        lowered = DbtLoweredInstruction{
            .opcode = DbtLoweredOpcode::Fallthrough,
            .pc = instruction.pc,
            .raw = instruction.raw,
            .next_pc = instruction.next_pc,
        };
        return true;
    }
    return false;
}

}  // namespace

DbtIrLoweringResult lower_dbt_ir_unit(const DbtTranslationUnit& unit) {
    if (!unit.ok) {
        return reject_lowering(unit);
    }

    DbtIrLoweringResult result{
        .ok = true,
        .start_pc = unit.start_pc,
        .end_pc = unit.end_pc,
    };
    result.instructions.reserve(unit.instructions.size());

    for (const DbtIrInstruction& instruction : unit.instructions) {
        DbtLoweredInstruction lowered{};
        if (!lower_ir_instruction(instruction, lowered)) {
            return reject_unsupported_ir(unit, instruction);
        }
        result.instructions.push_back(lowered);
    }

    return result;
}

const char* dbt_lowered_opcode_name(DbtLoweredOpcode opcode) {
    switch (opcode) {
    case DbtLoweredOpcode::Compute:
        return "compute";
    case DbtLoweredOpcode::Fallthrough:
        return "fallthrough";
    }
    return "unknown";
}

const char* dbt_lowered_operand_kind_name(DbtLoweredOperandKind kind) {
    switch (kind) {
    case DbtLoweredOperandKind::None:
        return "none";
    case DbtLoweredOperandKind::Gpr:
        return "gpr";
    case DbtLoweredOperandKind::Immediate:
        return "immediate";
    case DbtLoweredOperandKind::Pc:
        return "pc";
    }
    return "unknown";
}

const char* dbt_lowered_alu_op_name(DbtLoweredAluOp op) {
    switch (op) {
    case DbtLoweredAluOp::None:
        return "none";
    case DbtLoweredAluOp::Move:
        return "move";
    case DbtLoweredAluOp::Add:
        return "add";
    case DbtLoweredAluOp::Sub:
        return "sub";
    case DbtLoweredAluOp::Xor:
        return "xor";
    case DbtLoweredAluOp::Or:
        return "or";
    case DbtLoweredAluOp::And:
        return "and";
    case DbtLoweredAluOp::ShiftLeft:
        return "shift-left";
    case DbtLoweredAluOp::ShiftRightLogical:
        return "shift-right-logical";
    case DbtLoweredAluOp::ShiftRightArithmetic:
        return "shift-right-arithmetic";
    case DbtLoweredAluOp::SetLessThan:
        return "set-less-than";
    case DbtLoweredAluOp::SetLessThanUnsigned:
        return "set-less-than-unsigned";
    }
    return "unknown";
}

const char* dbt_lowered_width_name(DbtLoweredWidth width) {
    switch (width) {
    case DbtLoweredWidth::Xlen:
        return "xlen";
    case DbtLoweredWidth::Word:
        return "word";
    }
    return "unknown";
}
