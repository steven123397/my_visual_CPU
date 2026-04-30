#include "dbt_translator.h"

extern "C" {
#include "../decode.h"
}

namespace {

DbtTranslationUnit reject_from_plan(const DbtBlockPlan& plan) {
    return DbtTranslationUnit{
        .ok = false,
        .start_pc = plan.start_pc,
        .end_pc = plan.end_pc,
        .reject_reason = plan.fallback_reason,
        .boundary_kind = plan.boundary_kind,
        .boundary = plan.boundary,
    };
}

DbtTranslationUnit reject_unsupported_ir(const DbtBlockPlan& plan) {
    return DbtTranslationUnit{
        .ok = false,
        .start_pc = plan.start_pc,
        .end_pc = plan.end_pc,
        .reject_reason = "unsupported-ir",
        .boundary_kind = "unsupported-ir",
        .boundary = DbtBoundaryKind::Unsupported,
    };
}

bool translate_integer_op(const DbtDryRunIrOp& op, DbtIrInstruction& out) {
    Insn insn{};
    decode(op.raw, &insn);
    insn.raw = op.raw;

    out.pc = op.pc;
    out.raw = op.raw;
    out.size = op.size;
    out.rd = insn.rd;
    out.rs1 = insn.rs1;
    out.rs2 = insn.rs2;
    out.imm = insn.imm;
    out.next_pc = op.next_pc;

    if (insn.opcode == 0x13U && insn.funct3 == 0) {
        out.opcode = insn.rs1 == 0 ? DbtIrOpcode::WriteRegImm : DbtIrOpcode::AddRegImm;
        return true;
    }
    if (insn.opcode == 0x33U && insn.funct3 == 0 && insn.funct7 == 0x00U) {
        out.opcode = DbtIrOpcode::AddRegReg;
        return true;
    }
    if (insn.opcode == 0x33U && insn.funct3 == 0 && insn.funct7 == 0x20U) {
        out.opcode = DbtIrOpcode::SubRegReg;
        return true;
    }

    return false;
}

DbtIrInstruction make_fallthrough(const DbtBlockPlan& plan,
                                  const std::vector<DbtIrInstruction>& instructions) {
    if (!instructions.empty()) {
        const DbtIrInstruction& last = instructions.back();
        return DbtIrInstruction{
            .opcode = DbtIrOpcode::Fallthrough,
            .pc = last.pc,
            .next_pc = last.next_pc,
        };
    }
    return DbtIrInstruction{
        .opcode = DbtIrOpcode::Fallthrough,
        .pc = plan.start_pc,
        .next_pc = plan.start_pc,
    };
}

}  // namespace

DbtTranslationUnit translate_dbt_block(const DbtBlockPlan& plan) {
    if (!plan.ok) {
        return reject_from_plan(plan);
    }

    DbtTranslationUnit unit{
        .ok = true,
        .start_pc = plan.start_pc,
        .end_pc = plan.end_pc,
    };
    unit.instructions.reserve(plan.dry_run_ir.size() + 1);

    for (const DbtDryRunIrOp& op : plan.dry_run_ir) {
        DbtIrInstruction instruction{};
        if (!translate_integer_op(op, instruction)) {
            return reject_unsupported_ir(plan);
        }
        unit.instructions.push_back(instruction);
    }

    unit.instructions.push_back(make_fallthrough(plan, unit.instructions));
    return unit;
}
