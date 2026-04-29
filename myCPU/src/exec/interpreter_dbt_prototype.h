#pragma once

#include <cstdint>
#include <string>

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

struct InterpreterDbtPrototypePlan {
    bool ok{false};
    uint64_t start_pc{0};
    uint64_t end_pc{0};
    uint64_t candidate_executions{0};
    uint64_t candidate_retired_instructions{0};
    uint64_t inlineable_instructions{0};
    uint64_t fallback_pc{0};
    std::string fallback_reason{};
};

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
