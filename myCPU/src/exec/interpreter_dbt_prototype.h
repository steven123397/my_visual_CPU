#pragma once

#include <cstdint>
#include <string>

#include "dbt_block_plan.h"

class CPU;
class Bus;
struct ExecutionProfileSnapshot;

struct InterpreterDbtPrototypeResult {
    bool ok{false};
    bool used_fallback{false};
    uint64_t start_pc{0};
    uint64_t end_pc{0};
    uint64_t retired_instructions{0};
    uint64_t fallback_pc{0};
    std::string fallback_reason{};
};

using InterpreterDbtBoundaryKind = DbtBoundaryKind;
using InterpreterDbtDryRunIrKind = DbtDryRunIrKind;
using InterpreterDbtDryRunIrOp = DbtDryRunIrOp;
using InterpreterDbtPrototypePlan = DbtBlockPlan;
using InterpreterDbtInvalidationEventKind = DbtInvalidationEventKind;
using InterpreterDbtInvalidationPlan = DbtInvalidationPlan;

InterpreterDbtPrototypePlan plan_interpreter_dbt_prototype_block(CPU& cpu,
                                                                 Bus& bus,
                                                                 uint64_t start_pc,
                                                                 uint64_t end_pc);

InterpreterDbtPrototypePlan plan_interpreter_dbt_prototype_hot_path(
    CPU& cpu,
    Bus& bus,
    const ExecutionProfileSnapshot& profile);

InterpreterDbtPrototypeResult run_interpreter_dbt_prototype_block(CPU& cpu,
                                                                  Bus& bus,
                                                                  uint64_t start_pc,
                                                                  uint64_t end_pc);

InterpreterDbtPrototypeResult run_interpreter_dbt_prototype_with_functional_fallback(
    CPU& cpu,
    Bus& bus,
    uint64_t start_pc,
    uint64_t end_pc);

InterpreterDbtInvalidationPlan plan_interpreter_dbt_invalidation_event(
    InterpreterDbtInvalidationEventKind kind,
    uint64_t event_addr,
    uint64_t event_size,
    uint64_t block_start_pc,
    uint64_t block_end_pc);
