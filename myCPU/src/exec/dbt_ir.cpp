#include "dbt_ir.h"

const char* dbt_ir_opcode_name(DbtIrOpcode opcode) {
    switch (opcode) {
    case DbtIrOpcode::WriteRegImm:
        return "write-reg-imm";
    case DbtIrOpcode::AddRegImm:
        return "add-reg-imm";
    case DbtIrOpcode::XorRegImm:
        return "xor-reg-imm";
    case DbtIrOpcode::OrRegImm:
        return "or-reg-imm";
    case DbtIrOpcode::AndRegImm:
        return "and-reg-imm";
    case DbtIrOpcode::ShiftLeftRegImm:
        return "shift-left-reg-imm";
    case DbtIrOpcode::ShiftRightLogicalRegImm:
        return "shift-right-logical-reg-imm";
    case DbtIrOpcode::ShiftRightArithmeticRegImm:
        return "shift-right-arithmetic-reg-imm";
    case DbtIrOpcode::SetLessThanRegImm:
        return "set-less-than-reg-imm";
    case DbtIrOpcode::SetLessThanUnsignedRegImm:
        return "set-less-than-unsigned-reg-imm";
    case DbtIrOpcode::AddRegReg:
        return "add-reg-reg";
    case DbtIrOpcode::SubRegReg:
        return "sub-reg-reg";
    case DbtIrOpcode::XorRegReg:
        return "xor-reg-reg";
    case DbtIrOpcode::OrRegReg:
        return "or-reg-reg";
    case DbtIrOpcode::AndRegReg:
        return "and-reg-reg";
    case DbtIrOpcode::ShiftLeftRegReg:
        return "shift-left-reg-reg";
    case DbtIrOpcode::ShiftRightLogicalRegReg:
        return "shift-right-logical-reg-reg";
    case DbtIrOpcode::ShiftRightArithmeticRegReg:
        return "shift-right-arithmetic-reg-reg";
    case DbtIrOpcode::SetLessThanRegReg:
        return "set-less-than-reg-reg";
    case DbtIrOpcode::SetLessThanUnsignedRegReg:
        return "set-less-than-unsigned-reg-reg";
    case DbtIrOpcode::Fallthrough:
        return "fallthrough";
    }
    return "unknown";
}

const char* dbt_reject_kind_name(DbtRejectKind kind) {
    switch (kind) {
    case DbtRejectKind::None:
        return "none";
    case DbtRejectKind::PlanRejected:
        return "plan-rejected";
    case DbtRejectKind::HelperRequired:
        return "helper-required";
    case DbtRejectKind::FallbackRequired:
        return "fallback-required";
    case DbtRejectKind::FetchFault:
        return "fetch-fault";
    case DbtRejectKind::UnsupportedInstruction:
        return "unsupported-instruction";
    case DbtRejectKind::UnsupportedIr:
        return "unsupported-ir";
    case DbtRejectKind::ControlFlow:
        return "control-flow";
    case DbtRejectKind::MemoryLoad:
        return "memory-load";
    case DbtRejectKind::MemoryStore:
        return "memory-store";
    case DbtRejectKind::CsrWrite:
        return "csr-write";
    case DbtRejectKind::Trap:
        return "trap";
    case DbtRejectKind::Atomic:
        return "atomic";
    case DbtRejectKind::Vector:
        return "vector";
    case DbtRejectKind::TlbFlush:
        return "tlb-flush";
    case DbtRejectKind::TrapReturn:
        return "trap-return";
    case DbtRejectKind::Halt:
        return "halt";
    case DbtRejectKind::NotRetired:
        return "not-retired";
    }
    return "unknown";
}
