#include <cstdint>
#include <cstdio>

#include "../../src/cpu.h"
#include "../../src/exec/execution_profile.h"
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
constexpr uint32_t kLwX2FromX4 = 0x00022103U;     // lw x2, 0(x4)
constexpr uint32_t kSwX1ToX4 = 0x00122023U;       // sw x1, 0(x4)
constexpr uint32_t kJalX0Skip8 = 0x0080006fU;     // jal x0, 8
constexpr uint32_t kSfenceVma = 0x12000073U;      // sfence.vma x0, x0
constexpr uint64_t kData = MEM_BASE + 0x100;
constexpr uint32_t kDataWord = 0x11223344U;

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
           expect(plan.boundary_kind == "memory-load",
                  "interpreter DBT prototype plan should classify load helper boundary") &&
           expect(plan.boundary == InterpreterDbtBoundaryKind::MemoryLoad,
                  "interpreter DBT prototype plan should expose typed load helper boundary") &&
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

bool test_store_instruction_reports_memory_store_boundary() {
    Ram ram;
    Bus bus(ram);
    CPU cpu;
    cpu_init(cpu, kEntry);
    cpu.core().write_gpr(1, 0x55);
    cpu.core().write_gpr(4, kData);
    write32(ram, kEntry, kSwX1ToX4);

    const InterpreterDbtPrototypePlan plan =
        plan_interpreter_dbt_prototype_block(cpu, bus, kEntry, kEntry);

    return expect(!plan.ok, "interpreter DBT prototype should reject store instruction during preflight") &&
           expect(plan.fallback_reason == "helper-required",
                  "interpreter DBT prototype plan should report helper-required store fallback") &&
           expect(plan.boundary_kind == "memory-store",
                  "interpreter DBT prototype plan should classify store helper boundary") &&
           expect(plan.boundary == InterpreterDbtBoundaryKind::MemoryStore,
                  "interpreter DBT prototype plan should expose typed store helper boundary") &&
           expect(plan.fallback_pc == kEntry,
                  "interpreter DBT prototype plan should report store boundary PC") &&
           expect(cpu.core().instret() == 0,
                  "store boundary planning should not advance instret");
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
           expect(plan.boundary_kind == "memory-load",
                  "interpreter DBT prototype plan should classify helper boundary") &&
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
           expect(plan.boundary_kind == "control-flow",
                  "interpreter DBT prototype plan should classify control-flow boundary") &&
           expect(plan.boundary == InterpreterDbtBoundaryKind::ControlFlow,
                  "interpreter DBT prototype plan should expose typed control-flow boundary") &&
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

bool test_tlb_flush_boundary_block_is_rejected_before_prefix_commit() {
    Ram ram;
    Bus bus(ram);
    CPU cpu;
    cpu_init(cpu, kEntry);
    write32(ram, kEntry + 0, kAddiX1One);
    write32(ram, kEntry + 4, kSfenceVma);

    const InterpreterDbtPrototypePlan plan =
        plan_interpreter_dbt_prototype_block(cpu, bus, kEntry, kEntry + 4);
    const InterpreterDbtPrototypeResult result =
        run_interpreter_dbt_prototype_block(cpu, bus, kEntry, kEntry + 4);

    return expect(!plan.ok, "interpreter DBT prototype should reject TLB-flush block") &&
           expect(plan.inlineable_instructions == 1,
                  "interpreter DBT prototype plan should report inlineable prefix before TLB flush") &&
           expect(plan.fallback_pc == kEntry + 4,
                  "interpreter DBT prototype plan should report TLB-flush boundary PC") &&
           expect(plan.fallback_reason == "fallback-required",
                  "interpreter DBT prototype plan should report fallback-required TLB-flush boundary") &&
           expect(plan.boundary_kind == "tlb-flush",
                  "interpreter DBT prototype plan should classify TLB flush separately") &&
           expect(plan.boundary == InterpreterDbtBoundaryKind::TlbFlush,
                  "interpreter DBT prototype plan should expose typed TLB-flush boundary") &&
           expect(!result.ok, "interpreter DBT prototype should not execute TLB-flush block") &&
           expect(result.retired_instructions == 0,
                  "interpreter DBT prototype should reject TLB-flush block before retiring prefix") &&
           expect(cpu.core().read_gpr(1) == 0,
                  "interpreter DBT prototype should not commit inlineable prefix before TLB flush") &&
           expect(cpu.core().pc() == kEntry,
                  "interpreter DBT prototype should keep PC at block start after TLB-flush rejection") &&
           expect(cpu.core().instret() == 0,
                  "interpreter DBT prototype should keep instret unchanged after TLB-flush rejection") &&
           expect(cpu.core().cycle() == 0,
                  "interpreter DBT prototype should keep cycle unchanged after TLB-flush rejection");
}

bool test_inline_block_plan_exposes_dry_run_ir_ops() {
    Ram ram;
    Bus bus(ram);
    CPU cpu;
    cpu_init(cpu, kEntry);
    write32(ram, kEntry + 0, kAddiX1One);
    write32(ram, kEntry + 4, kAddiX2X1Two);
    write32(ram, kEntry + 8, kAddX3X1X2);

    const InterpreterDbtPrototypePlan plan =
        plan_interpreter_dbt_prototype_block(cpu, bus, kEntry, kEntry + 8);

    return expect(plan.ok, "dry-run IR plan should accept pure inlineable block") &&
           expect(plan.dry_run_ir.size() == 3,
                  "dry-run IR plan should expose one op per inlineable instruction") &&
           expect(plan.dry_run_ir[0].pc == kEntry,
                  "dry-run IR op should preserve guest PC") &&
           expect(plan.dry_run_ir[0].raw == kAddiX1One,
                  "dry-run IR op should preserve raw instruction") &&
           expect(plan.dry_run_ir[0].size == 4,
                  "dry-run IR op should preserve instruction size") &&
           expect(plan.dry_run_ir[0].kind == InterpreterDbtDryRunIrKind::ArchitectedEffect,
                  "dry-run IR op should use shared architected-effect semantics") &&
           expect(plan.dry_run_ir[0].rd_write,
                  "dry-run IR op should expose register write metadata") &&
           expect(plan.dry_run_ir[0].rd == 1,
                  "dry-run IR op should expose target rd metadata") &&
           expect(plan.dry_run_ir[0].next_pc == kEntry + 4,
                  "dry-run IR op should preserve fallthrough next PC") &&
           expect(cpu.core().pc() == kEntry,
                  "dry-run IR planning should not advance architectural PC") &&
           expect(cpu.core().instret() == 0,
                  "dry-run IR planning should not advance instret");
}

bool test_invalidation_dry_run_classifies_global_and_overlap_events() {
    const InterpreterDbtInvalidationPlan primary =
        plan_interpreter_dbt_invalidation_event(InterpreterDbtInvalidationEventKind::PrimaryImageLoad,
                                                0,
                                                0,
                                                kEntry,
                                                kEntry + 8);
    const InterpreterDbtInvalidationPlan disjoint_payload =
        plan_interpreter_dbt_invalidation_event(InterpreterDbtInvalidationEventKind::PayloadLoad,
                                                kEntry + 0x1000,
                                                4,
                                                kEntry,
                                                kEntry + 8);
    const InterpreterDbtInvalidationPlan overlapping_store =
        plan_interpreter_dbt_invalidation_event(InterpreterDbtInvalidationEventKind::GuestStore,
                                                kEntry + 4,
                                                4,
                                                kEntry,
                                                kEntry + 8);
    const InterpreterDbtInvalidationPlan sfence =
        plan_interpreter_dbt_invalidation_event(InterpreterDbtInvalidationEventKind::SfenceVma,
                                                0,
                                                0,
                                                kEntry,
                                                kEntry + 8);

    return expect(primary.invalidates,
                  "primary image load should invalidate future translated blocks") &&
           expect(primary.reason == "primary-image-load",
                  "primary image load dry-run should report a stable reason") &&
           expect(!disjoint_payload.invalidates,
                  "disjoint payload load should not invalidate an unrelated future translated block") &&
           expect(disjoint_payload.reason == "range-disjoint",
                  "disjoint payload load dry-run should report range-disjoint") &&
           expect(overlapping_store.invalidates,
                  "guest store overlapping translated code should invalidate future translated blocks") &&
           expect(overlapping_store.reason == "guest-store-overlaps-block",
                  "guest store overlap dry-run should report a stable reason") &&
           expect(sfence.invalidates,
                  "sfence.vma should invalidate future translated blocks") &&
           expect(sfence.reason == "sfence-vma",
                  "sfence.vma dry-run should report a stable reason");
}

bool test_hot_path_candidate_plan_uses_profile_ranking() {
    Ram ram;
    Bus bus(ram);
    CPU cpu;
    cpu_init(cpu, kEntry);
    write32(ram, kEntry + 0, kAddiX1One);
    write32(ram, kEntry + 4, kAddiX2X1Two);
    write32(ram, kEntry + 8, kAddX3X1X2);

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
            .end_pc = kEntry + 8,
            .executions = 3,
            .retired_instructions = 9,
        },
    };

    const InterpreterDbtPrototypePlan plan =
        plan_interpreter_dbt_prototype_hot_path(cpu, bus, profile);

    return expect(plan.ok, "hot path prototype plan should select the repeated candidate") &&
           expect(plan.start_pc == kEntry,
                  "hot path prototype plan should use the ranked candidate start PC") &&
           expect(plan.end_pc == kEntry + 8,
                  "hot path prototype plan should use the ranked candidate end PC") &&
           expect(plan.inlineable_instructions == 3,
                  "hot path prototype plan should preflight the ranked candidate");
}

bool test_hot_path_candidate_without_repetition_reports_none() {
    Ram ram;
    Bus bus(ram);
    CPU cpu;
    cpu_init(cpu, kEntry);

    ExecutionProfileSnapshot profile;
    profile.hot_paths = {
        ExecutionHotPathEntry{
            .start_pc = kEntry,
            .end_pc = kEntry + 8,
            .executions = 1,
            .retired_instructions = 3,
        },
    };

    const InterpreterDbtPrototypePlan plan =
        plan_interpreter_dbt_prototype_hot_path(cpu, bus, profile);

    return expect(!plan.ok, "hot path prototype plan should reject one-off paths") &&
           expect(plan.start_pc == kEntry,
                  "hot path prototype plan should preserve the rejected candidate start PC") &&
           expect(plan.end_pc == kEntry + 8,
                  "hot path prototype plan should preserve the rejected candidate end PC") &&
           expect(plan.inlineable_instructions == 0,
                  "hot path prototype plan should not preflight one-off paths") &&
           expect(plan.fallback_reason == "insufficient-repetition",
                  "hot path prototype plan should report insufficient repetition");
}

bool test_hot_path_candidate_helper_boundary_does_not_commit_prefix() {
    Ram ram;
    Bus bus(ram);
    CPU cpu;
    cpu_init(cpu, kEntry);
    write32(ram, kEntry + 0, kAddiX1One);
    write32(ram, kEntry + 4, kLwX1FromX0);

    ExecutionProfileSnapshot profile;
    profile.hot_paths = {
        ExecutionHotPathEntry{
            .start_pc = kEntry,
            .end_pc = kEntry + 4,
            .executions = 4,
            .retired_instructions = 8,
        },
    };

    const InterpreterDbtPrototypePlan plan =
        plan_interpreter_dbt_prototype_hot_path(cpu, bus, profile);

    return expect(!plan.ok, "hot path prototype plan should reject helper boundary") &&
           expect(plan.inlineable_instructions == 1,
                  "hot path prototype plan should report inlineable prefix before helper boundary") &&
           expect(plan.fallback_pc == kEntry + 4,
                  "hot path prototype plan should report first helper boundary PC") &&
           expect(plan.fallback_reason == "helper-required",
                  "hot path prototype plan should report helper-required boundary") &&
           expect(plan.boundary_kind == "memory-load",
                  "hot path prototype plan should classify helper boundary") &&
           expect(cpu.core().read_gpr(1) == 0,
                  "hot path prototype plan should not commit inlineable prefix") &&
           expect(cpu.core().pc() == kEntry,
                  "hot path prototype plan should not advance PC") &&
           expect(cpu.core().instret() == 0,
                  "hot path prototype plan should not advance instret") &&
           expect(cpu.core().cycle() == 0,
                  "hot path prototype plan should not advance cycles");
}

bool test_inline_block_uses_prototype_without_functional_fallback() {
    Ram ram;
    Bus bus(ram);
    CPU cpu;
    cpu_init(cpu, kEntry);
    write32(ram, kEntry + 0, kAddiX1One);
    write32(ram, kEntry + 4, kAddiX2X1Two);
    write32(ram, kEntry + 8, kAddX3X1X2);

    const InterpreterDbtPrototypeResult result =
        run_interpreter_dbt_prototype_with_functional_fallback(cpu, bus, kEntry, kEntry + 8);

    return expect(result.ok, "inlineable fallback wrapper should complete") &&
           expect(!result.used_fallback,
                  "inlineable fallback wrapper should stay on prototype path") &&
           expect(result.retired_instructions == 3,
                  "inlineable fallback wrapper should retire full block") &&
           expect(result.fallback_reason.empty(),
                  "inlineable fallback wrapper should not report fallback reason") &&
           expect(cpu.core().read_gpr(3) == 4,
                  "inlineable fallback wrapper should execute prototype effects");
}

bool test_helper_boundary_replays_from_functional_fallback() {
    Ram reference_ram;
    Bus reference_bus(reference_ram);
    CPU reference_cpu;
    cpu_init(reference_cpu, kEntry);
    reference_cpu.core().write_gpr(4, kData);
    write32(reference_ram, kEntry + 0, kAddiX1One);
    write32(reference_ram, kEntry + 4, kLwX2FromX4);
    write32(reference_ram, kData, kDataWord);

    Ram fallback_ram;
    Bus fallback_bus(fallback_ram);
    CPU fallback_cpu;
    cpu_init(fallback_cpu, kEntry);
    fallback_cpu.core().write_gpr(4, kData);
    write32(fallback_ram, kEntry + 0, kAddiX1One);
    write32(fallback_ram, kEntry + 4, kLwX2FromX4);
    write32(fallback_ram, kData, kDataWord);

    FunctionalBackend reference(reference_cpu, reference_bus);
    reference.step();
    reference.step();

    const InterpreterDbtPrototypeResult result =
        run_interpreter_dbt_prototype_with_functional_fallback(fallback_cpu,
                                                              fallback_bus,
                                                              kEntry,
                                                              kEntry + 4);

    return expect(result.ok, "helper boundary fallback replay should complete") &&
           expect(result.used_fallback,
                  "helper boundary fallback replay should use functional fallback") &&
           expect(result.retired_instructions == 2,
                  "helper boundary fallback replay should execute prefix and boundary") &&
           expect(result.fallback_pc == kEntry + 4,
                  "helper boundary fallback replay should report first boundary PC") &&
           expect(result.fallback_reason == "helper-required",
                  "helper boundary fallback replay should preserve helper reason") &&
           expect(fallback_cpu.core().read_gpr(1) == reference_cpu.core().read_gpr(1),
                  "helper boundary fallback replay should match functional prefix result") &&
           expect(fallback_cpu.core().read_gpr(2) == reference_cpu.core().read_gpr(2),
                  "helper boundary fallback replay should match functional load result") &&
           expect(fallback_cpu.core().pc() == reference_cpu.core().pc(),
                  "helper boundary fallback replay should match functional PC") &&
           expect(fallback_cpu.core().instret() == reference_cpu.core().instret(),
                  "helper boundary fallback replay should match functional instret") &&
           expect(fallback_cpu.core().cycle() == reference_cpu.core().cycle(),
                  "helper boundary fallback replay should match functional cycle");
}

bool test_control_flow_boundary_replays_from_functional_fallback() {
    Ram reference_ram;
    Bus reference_bus(reference_ram);
    CPU reference_cpu;
    cpu_init(reference_cpu, kEntry);
    write32(reference_ram, kEntry + 0, kAddiX1One);
    write32(reference_ram, kEntry + 4, kJalX0Skip8);

    Ram fallback_ram;
    Bus fallback_bus(fallback_ram);
    CPU fallback_cpu;
    cpu_init(fallback_cpu, kEntry);
    write32(fallback_ram, kEntry + 0, kAddiX1One);
    write32(fallback_ram, kEntry + 4, kJalX0Skip8);

    FunctionalBackend reference(reference_cpu, reference_bus);
    reference.step();
    reference.step();

    const InterpreterDbtPrototypeResult result =
        run_interpreter_dbt_prototype_with_functional_fallback(fallback_cpu,
                                                              fallback_bus,
                                                              kEntry,
                                                              kEntry + 4);

    return expect(result.ok, "control-flow fallback replay should complete") &&
           expect(result.used_fallback,
                  "control-flow fallback replay should use functional fallback") &&
           expect(result.retired_instructions == 2,
                  "control-flow fallback replay should execute prefix and boundary") &&
           expect(result.fallback_pc == kEntry + 4,
                  "control-flow fallback replay should report first boundary PC") &&
           expect(result.fallback_reason == "fallback-required",
                  "control-flow fallback replay should preserve fallback reason") &&
           expect(fallback_cpu.core().read_gpr(1) == reference_cpu.core().read_gpr(1),
                  "control-flow fallback replay should match functional prefix result") &&
           expect(fallback_cpu.core().pc() == reference_cpu.core().pc(),
                  "control-flow fallback replay should match functional redirected PC") &&
           expect(fallback_cpu.core().instret() == reference_cpu.core().instret(),
                  "control-flow fallback replay should match functional instret") &&
           expect(fallback_cpu.core().cycle() == reference_cpu.core().cycle(),
                  "control-flow fallback replay should match functional cycle");
}

}  // namespace

int main() {
    if (!test_inline_straight_line_block_matches_functional_backend()) {
        return 1;
    }
    if (!test_memory_instruction_requires_helper_fallback()) {
        return 1;
    }
    if (!test_store_instruction_reports_memory_store_boundary()) {
        return 1;
    }
    if (!test_helper_boundary_block_is_rejected_before_prefix_commit()) {
        return 1;
    }
    if (!test_control_flow_boundary_block_is_rejected_before_prefix_commit()) {
        return 1;
    }
    if (!test_tlb_flush_boundary_block_is_rejected_before_prefix_commit()) {
        return 1;
    }
    if (!test_inline_block_plan_exposes_dry_run_ir_ops()) {
        return 1;
    }
    if (!test_invalidation_dry_run_classifies_global_and_overlap_events()) {
        return 1;
    }
    if (!test_hot_path_candidate_plan_uses_profile_ranking()) {
        return 1;
    }
    if (!test_hot_path_candidate_without_repetition_reports_none()) {
        return 1;
    }
    if (!test_hot_path_candidate_helper_boundary_does_not_commit_prefix()) {
        return 1;
    }
    if (!test_inline_block_uses_prototype_without_functional_fallback()) {
        return 1;
    }
    if (!test_helper_boundary_replays_from_functional_fallback()) {
        return 1;
    }
    if (!test_control_flow_boundary_replays_from_functional_fallback()) {
        return 1;
    }
    std::puts("interpreter_dbt_prototype_smoke: PASS");
    return 0;
}
