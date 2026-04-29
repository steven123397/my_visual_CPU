#include <cstdint>
#include <cstdio>

#include "../../src/cpu.h"
#include "../../src/exec/functional_backend.h"
#include "../../src/exec/interpreter_dbt_prototype.h"
#include "../../src/mem/bus.h"
#include "../../src/mem/ram.h"

namespace {

constexpr uint64_t kEntry = MEM_BASE;
constexpr uint32_t kAddiX1One = 0x00100093U;      // addi x1, x0, 1
constexpr uint32_t kAddiX2X1Two = 0x00208113U;    // addi x2, x1, 2
constexpr uint32_t kAddX3X1X2 = 0x002081b3U;      // add x3, x1, x2
constexpr uint32_t kLwX1FromX0 = 0x00002083U;     // lw x1, 0(x0)
constexpr uint32_t kJalX0Skip8 = 0x0080006fU;     // jal x0, 8

bool expect(bool condition, const char* message) {
    if (!condition) {
        std::fprintf(stderr, "%s\n", message);
        return false;
    }
    return true;
}

void write32(Ram& ram, uint64_t addr, uint32_t value) {
    ram.write_bytes(addr, &value, sizeof(value));
}

bool test_inline_straight_line_block_matches_functional_backend() {
    Ram reference_ram;
    Bus reference_bus(reference_ram);
    CPU reference_cpu;
    cpu_init(reference_cpu, kEntry);
    write32(reference_ram, kEntry + 0, kAddiX1One);
    write32(reference_ram, kEntry + 4, kAddiX2X1Two);
    write32(reference_ram, kEntry + 8, kAddX3X1X2);

    Ram prototype_ram;
    Bus prototype_bus(prototype_ram);
    CPU prototype_cpu;
    cpu_init(prototype_cpu, kEntry);
    write32(prototype_ram, kEntry + 0, kAddiX1One);
    write32(prototype_ram, kEntry + 4, kAddiX2X1Two);
    write32(prototype_ram, kEntry + 8, kAddX3X1X2);

    FunctionalBackend reference(reference_cpu, reference_bus);
    for (int i = 0; i < 3; ++i) {
        reference.step();
    }

    const InterpreterDbtPrototypePlan plan =
        plan_interpreter_dbt_prototype_block(prototype_cpu, prototype_bus, kEntry, kEntry + 8);
    const InterpreterDbtPrototypeResult result =
        run_interpreter_dbt_prototype_block(prototype_cpu, prototype_bus, kEntry, kEntry + 8);

    return expect(plan.ok, "interpreter DBT prototype should plan pure straight-line block") &&
           expect(plan.inlineable_instructions == 3,
                  "interpreter DBT prototype should plan the expected block length") &&
           expect(plan.fallback_reason.empty(),
                  "interpreter DBT prototype plan should not report fallback on inlineable block") &&
           expect(result.ok, "interpreter DBT prototype should execute pure straight-line block") &&
           expect(result.retired_instructions == 3,
                  "interpreter DBT prototype should retire the expected block length") &&
           expect(result.fallback_reason.empty(),
                  "interpreter DBT prototype should not report fallback on inlineable block") &&
           expect(prototype_cpu.core().read_gpr(1) == reference_cpu.core().read_gpr(1),
                  "interpreter DBT prototype should match functional x1") &&
           expect(prototype_cpu.core().read_gpr(2) == reference_cpu.core().read_gpr(2),
                  "interpreter DBT prototype should match functional x2") &&
           expect(prototype_cpu.core().read_gpr(3) == reference_cpu.core().read_gpr(3),
                  "interpreter DBT prototype should match functional x3") &&
           expect(prototype_cpu.core().pc() == reference_cpu.core().pc(),
                  "interpreter DBT prototype should match functional pc") &&
           expect(prototype_cpu.core().instret() == reference_cpu.core().instret(),
                  "interpreter DBT prototype should match functional instret") &&
           expect(prototype_cpu.core().cycle() == reference_cpu.core().cycle(),
                  "interpreter DBT prototype should match functional cycle count");
}

bool test_memory_instruction_requires_helper_fallback() {
    Ram ram;
    Bus bus(ram);
    CPU cpu;
    cpu_init(cpu, kEntry);
    write32(ram, kEntry, kLwX1FromX0);

    const InterpreterDbtPrototypePlan plan =
        plan_interpreter_dbt_prototype_block(cpu, bus, kEntry, kEntry);
    const InterpreterDbtPrototypeResult result =
        run_interpreter_dbt_prototype_block(cpu, bus, kEntry, kEntry);

    return expect(!plan.ok, "interpreter DBT prototype should reject memory instruction during preflight") &&
           expect(plan.inlineable_instructions == 0,
                  "interpreter DBT prototype plan should not count helper-required instruction") &&
           expect(plan.fallback_pc == kEntry,
                  "interpreter DBT prototype plan should report memory fallback PC") &&
           expect(plan.fallback_reason == "helper-required",
                  "interpreter DBT prototype plan should report helper-required fallback") &&
           expect(!result.ok, "interpreter DBT prototype should fallback on memory instruction") &&
           expect(result.retired_instructions == 0,
                  "interpreter DBT prototype should not retire helper-required instruction") &&
           expect(result.fallback_pc == kEntry,
                  "interpreter DBT prototype should report fallback PC") &&
           expect(result.fallback_reason == "helper-required",
                  "interpreter DBT prototype should report helper-required fallback") &&
           expect(cpu.core().pc() == kEntry,
                  "interpreter DBT prototype should leave PC unchanged after fallback") &&
           expect(cpu.core().instret() == 0,
                  "interpreter DBT prototype should not advance instret after fallback") &&
           expect(cpu.core().cycle() == 0,
                  "interpreter DBT prototype should not advance cycles after fallback");
}

bool test_helper_boundary_block_is_rejected_before_prefix_commit() {
    Ram ram;
    Bus bus(ram);
    CPU cpu;
    cpu_init(cpu, kEntry);
    write32(ram, kEntry + 0, kAddiX1One);
    write32(ram, kEntry + 4, kLwX1FromX0);

    const InterpreterDbtPrototypePlan plan =
        plan_interpreter_dbt_prototype_block(cpu, bus, kEntry, kEntry + 4);
    const InterpreterDbtPrototypeResult result =
        run_interpreter_dbt_prototype_block(cpu, bus, kEntry, kEntry + 4);

    return expect(!plan.ok, "interpreter DBT prototype should reject mixed helper block") &&
           expect(plan.inlineable_instructions == 1,
                  "interpreter DBT prototype plan should report the inlineable prefix length") &&
           expect(plan.fallback_pc == kEntry + 4,
                  "interpreter DBT prototype plan should report first helper boundary PC") &&
           expect(plan.fallback_reason == "helper-required",
                  "interpreter DBT prototype plan should report helper-required boundary") &&
           expect(!result.ok, "interpreter DBT prototype should not execute mixed helper block") &&
           expect(result.retired_instructions == 0,
                  "interpreter DBT prototype should reject helper block before retiring prefix") &&
           expect(result.fallback_pc == kEntry + 4,
                  "interpreter DBT prototype should report first helper boundary PC") &&
           expect(result.fallback_reason == "helper-required",
                  "interpreter DBT prototype should report helper-required boundary") &&
           expect(cpu.core().read_gpr(1) == 0,
                  "interpreter DBT prototype should not commit inlineable prefix before helper fallback") &&
           expect(cpu.core().pc() == kEntry,
                  "interpreter DBT prototype should keep PC at block start after preflight rejection") &&
           expect(cpu.core().instret() == 0,
                  "interpreter DBT prototype should keep instret unchanged after preflight rejection") &&
           expect(cpu.core().cycle() == 0,
                  "interpreter DBT prototype should keep cycle unchanged after preflight rejection");
}

bool test_control_flow_boundary_block_is_rejected_before_prefix_commit() {
    Ram ram;
    Bus bus(ram);
    CPU cpu;
    cpu_init(cpu, kEntry);
    write32(ram, kEntry + 0, kAddiX1One);
    write32(ram, kEntry + 4, kJalX0Skip8);

    const InterpreterDbtPrototypePlan plan =
        plan_interpreter_dbt_prototype_block(cpu, bus, kEntry, kEntry + 4);
    const InterpreterDbtPrototypeResult result =
        run_interpreter_dbt_prototype_block(cpu, bus, kEntry, kEntry + 4);

    return expect(!plan.ok, "interpreter DBT prototype should reject control-flow block") &&
           expect(plan.inlineable_instructions == 1,
                  "interpreter DBT prototype plan should report inlineable prefix before control-flow") &&
           expect(plan.fallback_pc == kEntry + 4,
                  "interpreter DBT prototype plan should report control-flow boundary PC") &&
           expect(plan.fallback_reason == "fallback-required",
                  "interpreter DBT prototype plan should report fallback-required control-flow boundary") &&
           expect(!result.ok, "interpreter DBT prototype should not execute control-flow block") &&
           expect(result.retired_instructions == 0,
                  "interpreter DBT prototype should reject control-flow block before retiring prefix") &&
           expect(result.fallback_pc == kEntry + 4,
                  "interpreter DBT prototype should report control-flow boundary PC") &&
           expect(result.fallback_reason == "fallback-required",
                  "interpreter DBT prototype should report fallback-required control-flow boundary") &&
           expect(cpu.core().read_gpr(1) == 0,
                  "interpreter DBT prototype should not commit inlineable prefix before control-flow fallback") &&
           expect(cpu.core().pc() == kEntry,
                  "interpreter DBT prototype should keep PC at block start after control-flow rejection") &&
           expect(cpu.core().instret() == 0,
                  "interpreter DBT prototype should keep instret unchanged after control-flow rejection") &&
           expect(cpu.core().cycle() == 0,
                  "interpreter DBT prototype should keep cycle unchanged after control-flow rejection");
}

}  // namespace

int main() {
    if (!test_inline_straight_line_block_matches_functional_backend()) {
        return 1;
    }
    if (!test_memory_instruction_requires_helper_fallback()) {
        return 1;
    }
    if (!test_helper_boundary_block_is_rejected_before_prefix_commit()) {
        return 1;
    }
    if (!test_control_flow_boundary_block_is_rejected_before_prefix_commit()) {
        return 1;
    }
    std::puts("interpreter_dbt_prototype_smoke: PASS");
    return 0;
}
