#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "dbt_block_plan.h"

enum class DbtIrOpcode : uint8_t {
    WriteRegImm,
    AddPcImm,
    AddRegImm,
    AddRegImmWord,
    XorRegImm,
    OrRegImm,
    AndRegImm,
    ShiftLeftRegImm,
    ShiftRightLogicalRegImm,
    ShiftRightArithmeticRegImm,
    ShiftLeftRegImmWord,
    ShiftRightLogicalRegImmWord,
    ShiftRightArithmeticRegImmWord,
    SetLessThanRegImm,
    SetLessThanUnsignedRegImm,
    AddRegReg,
    SubRegReg,
    AddRegRegWord,
    SubRegRegWord,
    XorRegReg,
    OrRegReg,
    AndRegReg,
    ShiftLeftRegReg,
    ShiftRightLogicalRegReg,
    ShiftRightArithmeticRegReg,
    ShiftLeftRegRegWord,
    ShiftRightLogicalRegRegWord,
    ShiftRightArithmeticRegRegWord,
    SetLessThanRegReg,
    SetLessThanUnsignedRegReg,
    Fallthrough,
};

enum class DbtRejectKind : uint8_t {
    None,
    PlanRejected,
    HelperRequired,
    FallbackRequired,
    FetchFault,
    UnsupportedInstruction,
    UnsupportedIr,
    ControlFlow,
    MemoryLoad,
    MemoryStore,
    CsrWrite,
    Trap,
    Atomic,
    Vector,
    TlbFlush,
    TrapReturn,
    Halt,
    NotRetired,
};

struct DbtIrInstruction {
    DbtIrOpcode opcode{DbtIrOpcode::Fallthrough};
    uint64_t pc{0};
    uint32_t raw{0};
    uint8_t size{0};
    uint8_t rd{0};
    uint8_t rs1{0};
    uint8_t rs2{0};
    int64_t imm{0};
    uint64_t next_pc{0};
};

struct DbtTranslationUnit {
    bool ok{false};
    uint64_t start_pc{0};
    uint64_t end_pc{0};
    DbtRejectKind reject_kind{DbtRejectKind::None};
    uint64_t reject_pc{0};
    uint32_t reject_raw{0};
    std::string reject_reason{};
    std::string boundary_kind{};
    DbtBoundaryKind boundary{DbtBoundaryKind::None};
    DbtHelperPlan helper_plan{};
    std::vector<DbtCodePhysicalSpan> code_spans{};
    std::vector<DbtIrInstruction> instructions{};
};

const char* dbt_ir_opcode_name(DbtIrOpcode opcode);
const char* dbt_reject_kind_name(DbtRejectKind kind);
