#include <cstdint>
#include <cstdio>
#include <string>

#include "../../src/cpu.h"
#include "../../src/exec/dbt_jit_engine.h"
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

bool test_runtime_dispatch_contract_accepts_lowered_blocks_without_execution() {
    Ram ram;
    Bus bus(ram);
    CPU cpu;
    cpu_init(cpu, kEntry);
    cpu.core().write_gpr(1, 99);
    write32(ram, kEntry, kAddiX1One);

    DbtJitEngineDryRun engine;
    const uint64_t before_x1 = cpu.core().read_gpr(1);
    const uint64_t before_pc = cpu.core().pc();
    const DbtJitDryRunResult result = engine.dry_run_block(cpu, bus, kEntry, kEntry);
    const DbtRuntimeDispatchContract contract = plan_dbt_runtime_dispatch_contract(result);
    const std::string line = format_dbt_runtime_dispatch_contract(contract);

    return expect(contract.ok, "lowered dispatch contract should be valid") &&
           expect(contract.kind == DbtRuntimeDispatchKind::LoweredBlock,
                  "lowered dispatch contract should select lowered block path") &&
           expect(contract.can_enter_lowered_block && !contract.requires_helper_bridge &&
                      !contract.reference_step_required,
                  "lowered dispatch contract should not require helper or reference step") &&
           expect(contract.dry_run_only && !contract.mutates_cpu_state &&
                      !contract.generated_host_code &&
                      !contract.requested_executable_memory &&
                      !contract.executed_guest_code,
                  "runtime dispatch contract should remain non-executing") &&
           expect(contract.lowered_instruction_count == 2,
                  "runtime dispatch contract should expose lowered op count") &&
           expect(contract.reject_kind == DbtRejectKind::None &&
                      contract.helper_replay_kind == DbtHelperReplayKind::None,
                  "lowered dispatch contract should not carry reject or helper metadata") &&
           expect(cpu.core().read_gpr(1) == before_x1 && cpu.core().pc() == before_pc,
                  "runtime dispatch contract should not mutate CPU state") &&
           expect(line.find("runtime-dispatch: kind=lowered-block") != std::string::npos,
                  "runtime dispatch formatter should expose stable kind") &&
           expect(line.find("host-code=false exec-mem=false guest-exec=false") != std::string::npos,
                  "runtime dispatch formatter should expose non-execution flags");
}

bool test_runtime_dispatch_contract_bridges_helpers_to_reference_step() {
    Ram ram;
    Bus bus(ram);
    CPU cpu;
    cpu_init(cpu, kEntry);
    write32(ram, kEntry, kLwX1FromX0);

    DbtJitEngineDryRun engine;
    const DbtJitDryRunResult result = engine.dry_run_block(cpu, bus, kEntry, kEntry);
    const DbtRuntimeDispatchContract contract = plan_dbt_runtime_dispatch_contract(result);

    return expect(contract.ok, "helper dispatch contract should be valid") &&
           expect(contract.kind == DbtRuntimeDispatchKind::HelperBridgeToReference,
                  "helper dispatch contract should route through helper bridge") &&
           expect(!contract.can_enter_lowered_block && contract.requires_helper_bridge &&
                      contract.reference_step_required,
                  "helper dispatch contract should require reference fallback while helper execution is absent") &&
           expect(contract.helper_replay_kind == DbtHelperReplayKind::ScalarMemoryLoad &&
                      contract.reject_kind == DbtRejectKind::MemoryLoad,
                  "helper dispatch contract should preserve helper and reject metadata") &&
           expect(contract.helper_may_trap && !contract.helper_may_change_platform_state,
                  "scalar load helper dispatch contract should preserve effect flags") &&
           expect(contract.dry_run_only && !contract.mutates_cpu_state,
                  "helper dispatch contract should remain a dry-run");
}

bool test_runtime_dispatch_contract_routes_plain_fallback_to_reference_step() {
    Ram ram;
    Bus bus(ram);
    CPU cpu;
    cpu_init(cpu, kEntry);
    write32(ram, kEntry, kJalX0Skip8);

    DbtJitEngineDryRun engine;
    const DbtJitDryRunResult result = engine.dry_run_block(cpu, bus, kEntry, kEntry);
    const DbtRuntimeDispatchContract contract = plan_dbt_runtime_dispatch_contract(result);

    return expect(contract.ok, "fallback dispatch contract should be valid") &&
           expect(contract.kind == DbtRuntimeDispatchKind::ReferenceStep,
                  "plain fallback dispatch contract should select reference step") &&
           expect(!contract.can_enter_lowered_block && !contract.requires_helper_bridge &&
                      contract.reference_step_required,
                  "plain fallback dispatch should not claim helper or lowered execution") &&
           expect(contract.reject_kind == DbtRejectKind::ControlFlow &&
                      contract.reject_reason == "fallback-required",
                  "plain fallback dispatch should preserve stable reject metadata") &&
           expect(contract.helper_replay_kind == DbtHelperReplayKind::None,
                  "plain fallback dispatch should not expose helper replay metadata") &&
           expect(dbt_runtime_dispatch_kind_name(contract.kind) ==
                      std::string("reference-step"),
                  "runtime dispatch kind names should be stable");
}

}  // namespace

int main() {
    if (!test_runtime_dispatch_contract_accepts_lowered_blocks_without_execution()) {
        return 1;
    }
    if (!test_runtime_dispatch_contract_bridges_helpers_to_reference_step()) {
        return 1;
    }
    if (!test_runtime_dispatch_contract_routes_plain_fallback_to_reference_step()) {
        return 1;
    }
    std::puts("dbt_runtime_dispatch_smoke: PASS");
    return 0;
}
