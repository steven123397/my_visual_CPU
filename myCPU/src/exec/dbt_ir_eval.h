#pragma once

#include <array>
#include <cstdint>
#include <string>

#include "dbt_ir.h"

struct DbtIrEvaluationInput {
    std::array<uint64_t, 32> gpr{};
    uint64_t pc{0};
};

struct DbtIrEvaluationResult {
    bool ok{false};
    DbtRejectKind reject_kind{DbtRejectKind::None};
    std::string reject_reason{};
    std::array<uint64_t, 32> gpr{};
    uint64_t next_pc{0};
    uint64_t retired_instructions{0};
};

DbtIrEvaluationResult evaluate_dbt_ir_unit(const DbtTranslationUnit& unit,
                                           const DbtIrEvaluationInput& input);
