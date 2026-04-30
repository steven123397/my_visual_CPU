#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "dbt_ir.h"

enum class DbtLoweredOpcode : uint8_t {
    Compute,
    Fallthrough,
};

enum class DbtLoweredOperandKind : uint8_t {
    None,
    Gpr,
    Immediate,
    Pc,
};

enum class DbtLoweredAluOp : uint8_t {
    None,
    Move,
    Add,
    Sub,
    Xor,
    Or,
    And,
    ShiftLeft,
    ShiftRightLogical,
    ShiftRightArithmetic,
    SetLessThan,
    SetLessThanUnsigned,
};

enum class DbtLoweredWidth : uint8_t {
    Xlen,
    Word,
};

struct DbtLoweredInstruction {
    DbtLoweredOpcode opcode{DbtLoweredOpcode::Compute};
    uint64_t pc{0};
    uint32_t raw{0};
    DbtLoweredAluOp alu{DbtLoweredAluOp::None};
    DbtLoweredWidth width{DbtLoweredWidth::Xlen};
    DbtLoweredOperandKind lhs_kind{DbtLoweredOperandKind::None};
    DbtLoweredOperandKind rhs_kind{DbtLoweredOperandKind::None};
    uint8_t rd{0};
    uint8_t rs1{0};
    uint8_t rs2{0};
    int64_t imm{0};
    uint64_t next_pc{0};
    bool writes_gpr{false};
    bool sign_extend_word{false};
};

struct DbtIrLoweringResult {
    bool ok{false};
    uint64_t start_pc{0};
    uint64_t end_pc{0};
    DbtRejectKind reject_kind{DbtRejectKind::None};
    uint64_t reject_pc{0};
    uint32_t reject_raw{0};
    std::string reject_reason{};
    std::vector<DbtLoweredInstruction> instructions{};
};

DbtIrLoweringResult lower_dbt_ir_unit(const DbtTranslationUnit& unit);

const char* dbt_lowered_opcode_name(DbtLoweredOpcode opcode);
const char* dbt_lowered_operand_kind_name(DbtLoweredOperandKind kind);
const char* dbt_lowered_alu_op_name(DbtLoweredAluOp op);
const char* dbt_lowered_width_name(DbtLoweredWidth width);
