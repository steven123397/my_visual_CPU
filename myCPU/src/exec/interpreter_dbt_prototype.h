#pragma once

#include <cstdint>
#include <string>

class CPU;
class Bus;

struct InterpreterDbtPrototypeResult {
    bool ok{false};
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
    uint64_t inlineable_instructions{0};
    uint64_t fallback_pc{0};
    std::string fallback_reason{};
};

InterpreterDbtPrototypePlan plan_interpreter_dbt_prototype_block(CPU& cpu,
                                                                 Bus& bus,
                                                                 uint64_t start_pc,
                                                                 uint64_t end_pc);

InterpreterDbtPrototypeResult run_interpreter_dbt_prototype_block(CPU& cpu,
                                                                  Bus& bus,
                                                                  uint64_t start_pc,
                                                                  uint64_t end_pc);
