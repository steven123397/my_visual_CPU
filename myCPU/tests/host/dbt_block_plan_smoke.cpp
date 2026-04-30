#include <cstdint>
#include <cstdio>
#include <string>

#include "../../src/cpu.h"
#include "../../src/exec/dbt_block_plan.h"
#include "../../src/exec/execution_profile.h"
#include "../../src/mem/bus.h"
#include "../../src/mem/ram.h"

namespace {

constexpr uint64_t kEntry = MEM_BASE;
constexpr uint32_t kAddiX1One = 0x00100093U;    // addi x1, x0, 1
constexpr uint32_t kAddiX2X1Two = 0x00208113U;  // addi x2, x1, 2
constexpr uint32_t kLwX1FromX0 = 0x00002083U;   // lw x1, 0(x0)
constexpr uint32_t kSwX2To8FromX0 = 0x00202423U;  // sw x2, 8(x0)
constexpr uint32_t kCsrrwX1MstatusX2 = 0x300110f3U;  // csrrw x1, mstatus, x2
constexpr uint32_t kJalX0Skip8 = 0x0080006fU;   // jal x0, 8

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

bool test_shared_block_analyzer_plans_inlineable_ir() {
    Ram ram;
    Bus bus(ram);
    CPU cpu;
    cpu_init(cpu, kEntry);
    write32(ram, kEntry + 0, kAddiX1One);
    write32(ram, kEntry + 4, kAddiX2X1Two);

    const DbtBlockPlan plan = plan_dbt_block(cpu, bus, kEntry, kEntry + 4);

    return expect(plan.ok, "shared DBT block analyzer should accept pure inlineable block") &&
           expect(plan.start_pc == kEntry,
                  "shared DBT block analyzer should preserve block start PC") &&
           expect(plan.end_pc == kEntry + 4,
                  "shared DBT block analyzer should preserve block end PC") &&
           expect(plan.inlineable_instructions == 2,
                  "shared DBT block analyzer should count inlineable instructions") &&
           expect(plan.dry_run_ir.size() == 2,
                  "shared DBT block analyzer should expose dry-run IR ops") &&
           expect(plan.dry_run_ir[0].kind == DbtDryRunIrKind::ArchitectedEffect,
                  "shared DBT dry-run IR should use architected-effect ops") &&
           expect(plan.dry_run_ir[0].pc == kEntry,
                  "shared DBT dry-run IR should preserve guest PC") &&
           expect(plan.dry_run_ir[0].raw == kAddiX1One,
                  "shared DBT dry-run IR should preserve raw instruction") &&
           expect(plan.dry_run_ir[0].next_pc == kEntry + 4,
                  "shared DBT dry-run IR should expose fallthrough PC") &&
           expect(plan.dry_run_ir[0].rd_write && plan.dry_run_ir[0].rd == 1,
                  "shared DBT dry-run IR should expose rd write metadata") &&
           expect(cpu.core().pc() == kEntry,
                  "shared DBT block analyzer should not advance architectural PC") &&
           expect(cpu.core().instret() == 0,
                  "shared DBT block analyzer should not advance instret");
}

bool test_shared_block_analyzer_reports_helper_and_control_boundaries() {
    Ram helper_ram;
    Bus helper_bus(helper_ram);
    CPU helper_cpu;
    cpu_init(helper_cpu, kEntry);
    write32(helper_ram, kEntry + 0, kAddiX1One);
    write32(helper_ram, kEntry + 4, kLwX1FromX0);

    const DbtBlockPlan helper_plan = plan_dbt_block(helper_cpu, helper_bus, kEntry, kEntry + 4);

    Ram control_ram;
    Bus control_bus(control_ram);
    CPU control_cpu;
    cpu_init(control_cpu, kEntry);
    write32(control_ram, kEntry + 0, kAddiX1One);
    write32(control_ram, kEntry + 4, kJalX0Skip8);

    const DbtBlockPlan control_plan = plan_dbt_block(control_cpu, control_bus, kEntry, kEntry + 4);

    return expect(!helper_plan.ok,
                  "shared DBT block analyzer should reject helper-required block") &&
           expect(helper_plan.inlineable_instructions == 1,
                  "shared DBT block analyzer should report helper prefix length") &&
           expect(helper_plan.fallback_pc == kEntry + 4,
                  "shared DBT block analyzer should report helper boundary PC") &&
           expect(helper_plan.fallback_reason == "helper-required",
                  "shared DBT block analyzer should report helper fallback reason") &&
           expect(helper_plan.boundary == DbtBoundaryKind::MemoryLoad,
                  "shared DBT block analyzer should expose typed memory-load boundary") &&
           expect(!control_plan.ok,
                  "shared DBT block analyzer should reject control-flow block") &&
           expect(control_plan.boundary == DbtBoundaryKind::ControlFlow,
                  "shared DBT block analyzer should expose typed control-flow boundary") &&
           expect(control_plan.boundary_kind == "control-flow",
                  "shared DBT block analyzer should preserve stable boundary string");
}

bool test_shared_block_analyzer_builds_helper_plans() {
    Ram load_ram;
    Bus load_bus(load_ram);
    CPU load_cpu;
    cpu_init(load_cpu, kEntry);
    write32(load_ram, kEntry + 0, kLwX1FromX0);
    const DbtBlockPlan load_plan = plan_dbt_block(load_cpu, load_bus, kEntry, kEntry);

    Ram store_ram;
    Bus store_bus(store_ram);
    CPU store_cpu;
    cpu_init(store_cpu, kEntry);
    write32(store_ram, kEntry + 0, kSwX2To8FromX0);
    const DbtBlockPlan store_plan = plan_dbt_block(store_cpu, store_bus, kEntry, kEntry);

    Ram csr_ram;
    Bus csr_bus(csr_ram);
    CPU csr_cpu;
    cpu_init(csr_cpu, kEntry);
    write32(csr_ram, kEntry + 0, kCsrrwX1MstatusX2);
    const DbtBlockPlan csr_plan = plan_dbt_block(csr_cpu, csr_bus, kEntry, kEntry);

    Ram control_ram;
    Bus control_bus(control_ram);
    CPU control_cpu;
    cpu_init(control_cpu, kEntry);
    write32(control_ram, kEntry + 0, kJalX0Skip8);
    const DbtBlockPlan control_plan = plan_dbt_block(control_cpu, control_bus, kEntry, kEntry);

    return expect(!load_plan.ok, "load helper setup should reject block") &&
           expect(load_plan.helper_plan.required,
                  "load helper-required plan should expose helper metadata") &&
           expect(load_plan.helper_plan.kind == DbtHelperKind::MemoryLoad,
                  "load helper plan should classify memory load") &&
           expect(load_plan.helper_plan.pc == kEntry && load_plan.helper_plan.raw == kLwX1FromX0,
                  "load helper plan should preserve PC and raw instruction") &&
           expect(load_plan.helper_plan.rd == 1 && load_plan.helper_plan.addr == 0 &&
                      load_plan.helper_plan.size == 4 && load_plan.helper_plan.sign_extend,
                  "load helper plan should preserve rd, address, size, and sign-extension") &&
           expect(!store_plan.ok, "store helper setup should reject block") &&
           expect(store_plan.helper_plan.required,
                  "store helper-required plan should expose helper metadata") &&
           expect(store_plan.helper_plan.kind == DbtHelperKind::MemoryStore,
                  "store helper plan should classify memory store") &&
           expect(store_plan.helper_plan.addr == 8 && store_plan.helper_plan.size == 4 &&
                      store_plan.helper_plan.commit_at_boundary &&
                      store_plan.helper_plan.non_speculative,
                  "store helper plan should preserve address, size, and ordering flags") &&
           expect(!csr_plan.ok, "CSR helper setup should reject block") &&
           expect(csr_plan.helper_plan.required,
                  "CSR helper-required plan should expose helper metadata") &&
           expect(csr_plan.helper_plan.kind == DbtHelperKind::CsrWrite,
                  "CSR helper plan should classify CSR write") &&
           expect(csr_plan.helper_plan.rd == 1 && csr_plan.helper_plan.csr_addr == 0x300,
                  "CSR helper plan should preserve rd and CSR address") &&
           expect(!control_plan.ok, "control-flow setup should reject block") &&
           expect(!control_plan.helper_plan.required,
                  "fallback-required plan should not expose helper metadata") &&
           expect(dbt_helper_kind_name(DbtHelperKind::MemoryLoad) == std::string("memory-load"),
                  "helper kind name should expose memory-load") &&
           expect(dbt_helper_kind_name(DbtHelperKind::CsrWrite) == std::string("csr-write"),
                  "helper kind name should expose csr-write");
}

bool test_shared_hot_path_and_invalidation_contracts() {
    Ram ram;
    Bus bus(ram);
    CPU cpu;
    cpu_init(cpu, kEntry);
    write32(ram, kEntry + 0, kAddiX1One);
    write32(ram, kEntry + 4, kAddiX2X1Two);

    ExecutionProfileSnapshot profile;
    profile.hot_paths = {
        ExecutionHotPathEntry{
            .start_pc = kEntry + 0x40,
            .end_pc = kEntry + 0x44,
            .executions = 1,
            .retired_instructions = 200,
        },
        ExecutionHotPathEntry{
            .start_pc = kEntry,
            .end_pc = kEntry + 4,
            .executions = 3,
            .retired_instructions = 6,
        },
    };

    const DbtBlockPlan hot_path = plan_dbt_hot_path(cpu, bus, profile);
    const DbtInvalidationPlan overlapping_store =
        plan_dbt_block_invalidation_event(DbtInvalidationEventKind::GuestStore,
                                          kEntry + 4,
                                          4,
                                          kEntry,
                                          kEntry + 4);
    const DbtInvalidationPlan disjoint_payload =
        plan_dbt_block_invalidation_event(DbtInvalidationEventKind::PayloadLoad,
                                          kEntry + 0x1000,
                                          4,
                                          kEntry,
                                          kEntry + 4);

    return expect(hot_path.ok,
                  "shared DBT hot-path analyzer should plan repeated inlineable candidate") &&
           expect(hot_path.start_pc == kEntry && hot_path.end_pc == kEntry + 4,
                  "shared DBT hot-path analyzer should select ranked candidate range") &&
           expect(hot_path.candidate_executions == 3,
                  "shared DBT hot-path analyzer should preserve candidate execution count") &&
           expect(overlapping_store.invalidates,
                  "shared DBT invalidation dry-run should invalidate overlapping guest stores") &&
           expect(overlapping_store.reason == "guest-store-overlaps-block",
                  "shared DBT invalidation dry-run should report stable overlap reason") &&
           expect(!disjoint_payload.invalidates,
                  "shared DBT invalidation dry-run should keep disjoint payload loads") &&
           expect(disjoint_payload.reason == "range-disjoint",
                  "shared DBT invalidation dry-run should report stable disjoint reason");
}

}  // namespace

int main() {
    if (!test_shared_block_analyzer_plans_inlineable_ir()) {
        return 1;
    }
    if (!test_shared_block_analyzer_reports_helper_and_control_boundaries()) {
        return 1;
    }
    if (!test_shared_block_analyzer_builds_helper_plans()) {
        return 1;
    }
    if (!test_shared_hot_path_and_invalidation_contracts()) {
        return 1;
    }
    std::puts("dbt_block_plan_smoke: PASS");
    return 0;
}
