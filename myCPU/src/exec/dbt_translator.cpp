#include "dbt_translator.h"

extern "C" {
#include "../decode.h"
}

namespace {

DbtRejectKind reject_kind_from_plan(const DbtBlockPlan& plan) {
    switch (plan.boundary) {
    case DbtBoundaryKind::None:
        return DbtRejectKind::PlanRejected;
    case DbtBoundaryKind::FetchFault:
        return DbtRejectKind::FetchFault;
    case DbtBoundaryKind::Unsupported:
        return DbtRejectKind::UnsupportedInstruction;
    case DbtBoundaryKind::MemoryLoad:
        return DbtRejectKind::MemoryLoad;
    case DbtBoundaryKind::MemoryStore:
        return DbtRejectKind::MemoryStore;
    case DbtBoundaryKind::Atomic:
        return DbtRejectKind::Atomic;
    case DbtBoundaryKind::Vector:
        return DbtRejectKind::Vector;
    case DbtBoundaryKind::CsrWrite:
        return DbtRejectKind::CsrWrite;
    case DbtBoundaryKind::Trap:
        return DbtRejectKind::Trap;
    case DbtBoundaryKind::Halt:
        return DbtRejectKind::Halt;
    case DbtBoundaryKind::TlbFlush:
        return DbtRejectKind::TlbFlush;
    case DbtBoundaryKind::TrapReturn:
        return DbtRejectKind::TrapReturn;
    case DbtBoundaryKind::ControlFlow:
        return DbtRejectKind::ControlFlow;
    case DbtBoundaryKind::NotRetired:
        return DbtRejectKind::NotRetired;
    case DbtBoundaryKind::Fallback:
        return plan.fallback_reason == "helper-required"
            ? DbtRejectKind::HelperRequired
            : DbtRejectKind::FallbackRequired;
    }
    return DbtRejectKind::PlanRejected;
}

DbtTranslationUnit reject_from_plan(const DbtBlockPlan& plan) {
    return DbtTranslationUnit{
        .ok = false,
        .start_pc = plan.start_pc,
        .end_pc = plan.end_pc,
        .reject_kind = reject_kind_from_plan(plan),
        .reject_pc = plan.fallback_pc,
        .reject_raw = plan.fallback_raw,
        .reject_reason = plan.fallback_reason,
        .boundary_kind = plan.boundary_kind,
        .boundary = plan.boundary,
    };
}

DbtTranslationUnit reject_unsupported_ir(const DbtBlockPlan& plan, const DbtDryRunIrOp& op) {
    return DbtTranslationUnit{
        .ok = false,
        .start_pc = plan.start_pc,
        .end_pc = plan.end_pc,
        .reject_kind = DbtRejectKind::UnsupportedIr,
        .reject_pc = op.pc,
        .reject_raw = op.raw,
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

    if (insn.opcode == 0x13U) {
        const uint8_t shamt = static_cast<uint8_t>(insn.rs2 | ((insn.funct7 & 1U) << 5));
        switch (insn.funct3) {
        case 0:
            out.opcode = insn.rs1 == 0 ? DbtIrOpcode::WriteRegImm : DbtIrOpcode::AddRegImm;
            return true;
        case 1:
            if ((insn.funct7 & ~0x01U) != 0x00U) {
                return false;
            }
            out.opcode = DbtIrOpcode::ShiftLeftRegImm;
            out.imm = shamt;
            return true;
        case 2:
            out.opcode = DbtIrOpcode::SetLessThanRegImm;
            return true;
        case 3:
            out.opcode = DbtIrOpcode::SetLessThanUnsignedRegImm;
            return true;
        case 4:
            out.opcode = DbtIrOpcode::XorRegImm;
            return true;
        case 5:
            if ((insn.funct7 & ~0x01U) == 0x00U) {
                out.opcode = DbtIrOpcode::ShiftRightLogicalRegImm;
            } else if ((insn.funct7 & ~0x01U) == 0x20U) {
                out.opcode = DbtIrOpcode::ShiftRightArithmeticRegImm;
            } else {
                return false;
            }
            out.imm = shamt;
            return true;
        case 6:
            out.opcode = DbtIrOpcode::OrRegImm;
            return true;
        case 7:
            out.opcode = DbtIrOpcode::AndRegImm;
            return true;
        default:
            return false;
        }
    }
    if (insn.opcode == 0x33U) {
        switch (insn.funct3) {
        case 0:
            if (insn.funct7 == 0x00U) {
                out.opcode = DbtIrOpcode::AddRegReg;
                return true;
            }
            if (insn.funct7 == 0x20U) {
                out.opcode = DbtIrOpcode::SubRegReg;
                return true;
            }
            return false;
        case 1:
            if (insn.funct7 != 0x00U) {
                return false;
            }
            out.opcode = DbtIrOpcode::ShiftLeftRegReg;
            return true;
        case 2:
            if (insn.funct7 != 0x00U) {
                return false;
            }
            out.opcode = DbtIrOpcode::SetLessThanRegReg;
            return true;
        case 3:
            if (insn.funct7 != 0x00U) {
                return false;
            }
            out.opcode = DbtIrOpcode::SetLessThanUnsignedRegReg;
            return true;
        case 4:
            if (insn.funct7 != 0x00U) {
                return false;
            }
            out.opcode = DbtIrOpcode::XorRegReg;
            return true;
        case 5:
            if (insn.funct7 == 0x00U) {
                out.opcode = DbtIrOpcode::ShiftRightLogicalRegReg;
            } else if (insn.funct7 == 0x20U) {
                out.opcode = DbtIrOpcode::ShiftRightArithmeticRegReg;
            } else {
                return false;
            }
            return true;
        case 6:
            if (insn.funct7 != 0x00U) {
                return false;
            }
            out.opcode = DbtIrOpcode::OrRegReg;
            return true;
        case 7:
            if (insn.funct7 != 0x00U) {
                return false;
            }
            out.opcode = DbtIrOpcode::AndRegReg;
            return true;
        default:
            return false;
        }
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
            return reject_unsupported_ir(plan, op);
        }
        unit.instructions.push_back(instruction);
    }

    unit.instructions.push_back(make_fallthrough(plan, unit.instructions));
    return unit;
}
