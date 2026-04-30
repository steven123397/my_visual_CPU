#include <cstdint>
#include <cstdio>
#include <string>

#include "../../src/cpu.h"
#include "../../src/exec/dbt_jit_engine.h"
#include "../../src/exec/dbt_reference_fallback.h"
#include "../../src/exec/dbt_runtime_dispatch.h"
#include "../../src/mem/bus.h"
#include "../../src/mem/ram.h"

namespace {

constexpr uint64_t kEntry = MEM_BASE;
constexpr uint32_t kAddiX1One = 0x00100093U;  // addi x1, x0, 1
constexpr uint32_t kLwX1FromX0 = 0x00002083U; // lw x1, 0(x0)
constexpr uint32_t kJalX0Skip8 = 0x0080006fU; // jal x0, 8

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

DbtRuntimeDispatchContract dispatch_contract(Ram& ram,
                                             Bus& bus,
                                             CPU& cpu,
                                             uint64_t pc,
                                             uint32_t raw) {
    write32(ram, pc, raw);
    DbtJitEngineDryRun engine;
    return plan_dbt_runtime_dispatch_contract(engine.dry_run_block(cpu, bus, pc, pc));
}

bool test_reference_fallback_bridge_plans_plain_reference_step_without_execution() {
    Ram ram;
    Bus bus(ram);
    CPU cpu;
    cpu_init(cpu, kEntry);
    const uint64_t before_pc = cpu.core().pc();
    const uint64_t before_instret = cpu.core().instret();

    const DbtRuntimeDispatchContract contract =
        dispatch_contract(ram, bus, cpu, kEntry, kJalX0Skip8);
    const DbtReferenceFallbackPlan plan = plan_dbt_reference_fallback_step(contract);
    const std::string line = format_dbt_reference_fallback_plan(plan);

    return expect(contract.kind == DbtRuntimeDispatchKind::ReferenceStep,
                  "setup should produce a plain reference-step dispatch contract") &&
           expect(plan.ok && plan.kind == DbtReferenceFallbackKind::ReferenceStep,
                  "plain fallback should produce a reference step bridge plan") &&
           expect(plan.reference_step_required && plan.reference_step_count == 1,
                  "plain fallback should request exactly one future reference step") &&
           expect(!plan.requires_helper_bridge && plan.helper_replay_kind == DbtHelperReplayKind::None,
                  "plain fallback should not claim helper bridge metadata") &&
           expect(plan.pc == kEntry && plan.reject_kind == DbtRejectKind::ControlFlow &&
                      plan.reject_reason == "fallback-required",
                  "plain fallback should preserve PC and reject metadata") &&
           expect(plan.reference_backend == "functional-reference",
                  "fallback bridge should name the reference backend contract") &&
           expect(plan.dry_run_only && !plan.executed_reference_step &&
                      !plan.mutates_cpu_state && !plan.executed_guest_code &&
                      !plan.generated_host_code && !plan.requested_executable_memory,
                  "fallback bridge should remain non-executing") &&
           expect(cpu.core().pc() == before_pc && cpu.core().instret() == before_instret,
                  "fallback bridge planning should not mutate CPU state") &&
           expect(line.find("reference-fallback: kind=reference-step") != std::string::npos,
                  "fallback bridge formatter should expose stable kind") &&
           expect(line.find("executed-reference-step=false") != std::string::npos,
                  "fallback bridge formatter should expose non-execution flag");
}

bool test_reference_fallback_bridge_preserves_helper_context_without_executing_helper() {
    Ram ram;
    Bus bus(ram);
    CPU cpu;
    cpu_init(cpu, kEntry);

    const DbtRuntimeDispatchContract contract =
        dispatch_contract(ram, bus, cpu, kEntry, kLwX1FromX0);
    const DbtReferenceFallbackPlan plan = plan_dbt_reference_fallback_step(contract);

    return expect(contract.kind == DbtRuntimeDispatchKind::HelperBridgeToReference,
                  "setup should produce helper-bridge dispatch contract") &&
           expect(plan.ok && plan.kind == DbtReferenceFallbackKind::HelperBridgeReferenceStep,
                  "helper dispatch should produce helper bridge reference fallback plan") &&
           expect(plan.requires_helper_bridge && plan.reference_step_required &&
                      plan.reference_step_count == 1,
                  "helper bridge fallback should request helper context plus one reference step") &&
           expect(plan.helper_replay_kind == DbtHelperReplayKind::ScalarMemoryLoad &&
                      plan.reject_kind == DbtRejectKind::MemoryLoad,
                  "helper bridge fallback should preserve helper and reject metadata") &&
           expect(plan.helper_may_trap && !plan.helper_may_change_platform_state,
                  "helper bridge fallback should preserve helper effect flags") &&
           expect(!plan.executed_helper && !plan.executed_reference_step &&
                      !plan.mutates_cpu_state,
                  "helper bridge fallback should not execute helper or reference step");
}

bool test_reference_fallback_bridge_rejects_lowered_blocks() {
    Ram ram;
    Bus bus(ram);
    CPU cpu;
    cpu_init(cpu, kEntry);

    const DbtRuntimeDispatchContract contract =
        dispatch_contract(ram, bus, cpu, kEntry, kAddiX1One);
    const DbtReferenceFallbackPlan plan = plan_dbt_reference_fallback_step(contract);

    return expect(contract.kind == DbtRuntimeDispatchKind::LoweredBlock,
                  "setup should produce lowered block dispatch contract") &&
           expect(!plan.ok && plan.kind == DbtReferenceFallbackKind::None,
                  "lowered blocks should not produce reference fallback plans") &&
           expect(plan.reject_reason == "lowered-block-does-not-require-reference-fallback",
                  "lowered block rejection should expose stable reason") &&
           expect(!plan.reference_step_required && plan.reference_step_count == 0,
                  "lowered block rejection should not request reference step") &&
           expect(dbt_reference_fallback_kind_name(DbtReferenceFallbackKind::ReferenceStep) ==
                      std::string("reference-step"),
                  "reference fallback kind names should be stable");
}

}  // namespace

int main() {
    if (!test_reference_fallback_bridge_plans_plain_reference_step_without_execution()) {
        return 1;
    }
    if (!test_reference_fallback_bridge_preserves_helper_context_without_executing_helper()) {
        return 1;
    }
    if (!test_reference_fallback_bridge_rejects_lowered_blocks()) {
        return 1;
    }
    std::puts("dbt_reference_fallback_smoke: PASS");
    return 0;
}
