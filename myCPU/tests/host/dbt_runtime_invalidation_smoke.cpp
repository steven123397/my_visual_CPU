#include <cstdint>
#include <cstdio>
#include <string>

#include "../../src/cpu.h"
#include "../../src/exec/dbt_executable_cache.h"
#include "../../src/exec/dbt_host_emitter.h"
#include "../../src/exec/dbt_jit_engine.h"
#include "../../src/exec/dbt_runtime_dispatch.h"
#include "../../src/exec/dbt_runtime_invalidation.h"
#include "../../src/mem/bus.h"
#include "../../src/mem/ram.h"

namespace {

constexpr uint64_t kEntry = MEM_BASE;
constexpr uint32_t kAddiX1One = 0x00100093U;  // addi x1, x0, 1
constexpr uint64_t kSv39Mode = 8ULL << 60;
constexpr uint64_t kAliasRootPageTable = MEM_BASE + 0x10000;
constexpr uint64_t kAliasLevel1PageTable = MEM_BASE + 0x11000;
constexpr uint64_t kAliasLevel0PageTable = MEM_BASE + 0x12000;
constexpr uint64_t kAliasCodePa = MEM_BASE + 0x40000;
constexpr uint64_t kAliasCodeVaA = 0x40000000;
constexpr uint64_t kAliasCodeVaB = 0x40001000;
constexpr uint64_t kPteV = 1ULL << 0;
constexpr uint64_t kPteR = 1ULL << 1;
constexpr uint64_t kPteX = 1ULL << 3;
constexpr uint64_t kPteA = 1ULL << 6;
constexpr uint64_t kPteD = 1ULL << 7;

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

void write64(Ram& ram, uint64_t addr, uint64_t value) {
    ram.write_bytes(addr, &value, sizeof(value));
}

uint64_t pte_for(uint64_t paddr, uint64_t flags) {
    return ((paddr >> 12) << 10) | flags;
}

uint64_t vpn(uint64_t vaddr, int level) {
    return (vaddr >> (12 + level * 9)) & 0x1ffULL;
}

void map_alias_code_page(Ram& ram, CPU& cpu) {
    write32(ram, kAliasCodePa, kAddiX1One);
    write64(ram,
            kAliasRootPageTable + vpn(kAliasCodeVaA, 2) * 8,
            pte_for(kAliasLevel1PageTable, kPteV));
    write64(ram,
            kAliasLevel1PageTable + vpn(kAliasCodeVaA, 1) * 8,
            pte_for(kAliasLevel0PageTable, kPteV));
    write64(ram,
            kAliasLevel0PageTable + vpn(kAliasCodeVaA, 0) * 8,
            pte_for(kAliasCodePa, kPteV | kPteR | kPteX | kPteA | kPteD));
    write64(ram,
            kAliasLevel0PageTable + vpn(kAliasCodeVaB, 0) * 8,
            pte_for(kAliasCodePa, kPteV | kPteR | kPteX | kPteA | kPteD));

    cpu.core().set_privilege_mode(PrivilegeMode::Supervisor);
    cpu.csr().write(CSR_SATP, kSv39Mode | (kAliasRootPageTable >> 12), cpu.core());
    cpu.address_space().flush_tlb();
}

DbtRuntimeDispatchContract lowered_contract(Ram& ram,
                                             Bus& bus,
                                             CPU& cpu,
                                             uint64_t pc) {
    write32(ram, pc, kAddiX1One);
    DbtJitEngineDryRun engine;
    return plan_dbt_runtime_dispatch_contract(engine.dry_run_block(cpu, bus, pc, pc));
}

DbtRuntimeDispatchContract mapped_lowered_contract(Bus& bus, CPU& cpu, uint64_t pc) {
    DbtJitEngineDryRun engine;
    return plan_dbt_runtime_dispatch_contract(engine.dry_run_block(cpu, bus, pc, pc));
}

bool populate_cache(DbtExecutableCacheDryRun& cache, uint64_t pc) {
    Ram ram;
    Bus bus(ram);
    CPU cpu;
    cpu_init(cpu, kEntry);
    return cache.insert(lowered_contract(ram, bus, cpu, pc));
}

bool populate_runtime_cache(DbtExecutableCacheRuntime& cache, uint64_t pc) {
    Ram ram;
    Bus bus(ram);
    CPU cpu;
    cpu_init(cpu, kEntry);
    write32(ram, pc, kAddiX1One);
    DbtJitEngineDryRun engine;
    const DbtJitDryRunResult dry_run = engine.dry_run_block(cpu, bus, pc, pc);
    DbtHostExecutable executable = emit_dbt_host_block(dry_run.lowering);
    return cache.insert(plan_dbt_runtime_dispatch_contract(dry_run), executable);
}

bool test_runtime_hook_enforces_guest_store_overlap() {
    DbtExecutableCacheDryRun cache;
    const bool inserted = populate_cache(cache, kEntry);

    const DbtRuntimeInvalidationEvent event{
        .kind = DbtInvalidationEventKind::GuestStore,
        .addr = kEntry,
        .size = 4,
    };
    const DbtRuntimeInvalidationHookResult result =
        apply_dbt_runtime_invalidation_hook(cache, event);
    const DbtExecutableCacheLookup lookup = cache.lookup(kEntry, kEntry);

    return expect(inserted, "runtime invalidation setup should cache lowered block") &&
           expect(result.ok && result.enforced,
                  "runtime invalidation hook should enforce guest store events") &&
           expect(result.event_kind == DbtInvalidationEventKind::GuestStore,
                  "runtime invalidation hook should preserve event kind") &&
           expect(result.invalidated && result.entries_removed == 1 &&
                      result.entries_examined == 1 &&
                      result.reason == "guest-store-overlaps-block" &&
                      result.stale_dispatch_prevented,
                  "overlapping guest store should prevent stale executable dispatch") &&
           expect(result.dry_run_only && !result.mutates_cpu_state &&
                      !result.generated_host_code && !result.executed_guest_code,
                  "runtime invalidation hook should remain non-executing") &&
           expect(!lookup.hit, "runtime invalidation should remove resident block");
}

bool test_runtime_hook_keeps_disjoint_payload_load() {
    DbtExecutableCacheDryRun cache;
    populate_cache(cache, kEntry);

    const DbtRuntimeInvalidationEvent event{
        .kind = DbtInvalidationEventKind::PayloadLoad,
        .addr = kEntry + 0x100,
        .size = 16,
    };
    const DbtRuntimeInvalidationHookResult result =
        apply_dbt_runtime_invalidation_hook(cache, event);
    const DbtExecutableCacheLookup lookup = cache.lookup(kEntry, kEntry);

    return expect(result.ok && result.enforced,
                  "runtime invalidation hook should enforce payload load events") &&
           expect(!result.invalidated && result.entries_removed == 0 &&
                      result.entries_examined == 1 && result.reason == "range-disjoint",
                  "disjoint payload load should not invalidate resident block") &&
           expect(lookup.hit, "disjoint payload load should keep resident block dispatchable");
}

bool test_runtime_hook_enforces_global_events() {
    const DbtInvalidationEventKind events[] = {
        DbtInvalidationEventKind::PrimaryImageLoad,
        DbtInvalidationEventKind::DebugReset,
        DbtInvalidationEventKind::SatpWrite,
        DbtInvalidationEventKind::SfenceVma,
        DbtInvalidationEventKind::RegionAttributesChanged,
    };
    const char* reasons[] = {
        "primary-image-load",
        "debug-reset",
        "satp-write",
        "sfence-vma",
        "region-attributes-changed",
    };

    for (size_t i = 0; i < sizeof(events) / sizeof(events[0]); ++i) {
        DbtExecutableCacheDryRun cache;
        populate_cache(cache, kEntry);
        const DbtRuntimeInvalidationHookResult result =
            apply_dbt_runtime_invalidation_hook(cache, DbtRuntimeInvalidationEvent{
                                                           .kind = events[i],
                                                       });
        if (!expect(result.ok && result.invalidated,
                    "global runtime invalidation event should invalidate resident block") ||
            !expect(result.entries_removed == 1 && result.reason == reasons[i],
                    "global runtime invalidation event should preserve stable reason") ||
            !expect(cache.size() == 0,
                    "global runtime invalidation event should clear resident block")) {
            return false;
        }
    }

    return true;
}

bool test_runtime_hook_formats_empty_cache_event() {
    DbtExecutableCacheDryRun cache;
    const DbtRuntimeInvalidationHookResult result =
        apply_dbt_runtime_invalidation_hook(cache, DbtRuntimeInvalidationEvent{
                                                       .kind = DbtInvalidationEventKind::PrimaryImageLoad,
                                                   });
    const std::string line = format_dbt_runtime_invalidation_hook_result(result);

    return expect(result.ok && result.enforced && !result.invalidated &&
                      result.reason == "primary-image-load",
                  "empty cache global event should still expose event reason") &&
           expect(line.find("runtime-invalidation: kind=primary-image-load") !=
                      std::string::npos,
                  "runtime invalidation formatter should expose stable kind") &&
           expect(line.find("dry-run-only=true") != std::string::npos,
                  "runtime invalidation formatter should expose dry-run flag");
}

bool test_runtime_hook_releases_runtime_host_executable_on_overlap() {
    DbtExecutableCacheRuntime cache;
    const bool inserted = populate_runtime_cache(cache, kEntry);

    const DbtRuntimeInvalidationHookResult result =
        apply_dbt_runtime_invalidation_hook(cache, DbtRuntimeInvalidationEvent{
                                                       .kind = DbtInvalidationEventKind::GuestStore,
                                                       .addr = kEntry,
                                                       .size = 4,
                                                   });
    const DbtExecutableCacheLookup lookup = cache.lookup(kEntry, kEntry);
    const DbtExecutableCacheDryRunStats stats = cache.stats();

    return expect(inserted, "runtime invalidation setup should cache host executable") &&
           expect(result.ok && result.invalidated && result.entries_removed == 1 &&
                      result.reason == "guest-store-overlaps-block" &&
                      result.stale_dispatch_prevented,
                  "runtime invalidation hook should invalidate resident host executable") &&
           expect(!lookup.hit && cache.size() == 0,
                  "runtime invalidation hook should remove host executable entry") &&
           expect(stats.host_executables_released == 1,
                  "runtime invalidation hook should release invalidated host executable");
}

bool test_runtime_hook_invalidates_physical_synonym_code_store() {
    Ram ram;
    Bus bus(ram);
    CPU cpu;
    cpu_init(cpu, kAliasCodeVaA);
    map_alias_code_page(ram, cpu);

    DbtExecutableCacheDryRun cache;
    const DbtRuntimeDispatchContract first =
        mapped_lowered_contract(bus, cpu, kAliasCodeVaA);
    const DbtRuntimeDispatchContract second =
        mapped_lowered_contract(bus, cpu, kAliasCodeVaB);
    const bool inserted_first = cache.insert(first);
    const bool inserted_second = cache.insert(second);

    const DbtRuntimeInvalidationHookResult result =
        apply_dbt_runtime_invalidation_hook(cache, DbtRuntimeInvalidationEvent{
                                                       .kind = DbtInvalidationEventKind::GuestStore,
                                                       .addr = kAliasCodePa,
                                                       .size = 4,
                                                   });
    const DbtExecutableCacheLookup first_lookup =
        cache.lookup(kAliasCodeVaA, kAliasCodeVaA);
    const DbtExecutableCacheLookup second_lookup =
        cache.lookup(kAliasCodeVaB, kAliasCodeVaB);

    return expect(first.ok && second.ok,
                  "physical synonym setup should create lowered contracts") &&
           expect(inserted_first && inserted_second,
                  "physical synonym setup should cache both virtual aliases") &&
           expect(result.ok && result.invalidated && result.entries_removed == 2 &&
                      result.entries_examined == 2 &&
                      result.reason == "guest-store-overlaps-physical-code" &&
                      result.stale_dispatch_prevented,
                  "guest store to physical code page should invalidate both virtual aliases") &&
           expect(!first_lookup.hit && !second_lookup.hit,
                  "physical synonym invalidation should remove all stale aliases");
}

}  // namespace

int main() {
    if (!test_runtime_hook_enforces_guest_store_overlap()) {
        return 1;
    }
    if (!test_runtime_hook_keeps_disjoint_payload_load()) {
        return 1;
    }
    if (!test_runtime_hook_enforces_global_events()) {
        return 1;
    }
    if (!test_runtime_hook_formats_empty_cache_event()) {
        return 1;
    }
    if (!test_runtime_hook_releases_runtime_host_executable_on_overlap()) {
        return 1;
    }
    if (!test_runtime_hook_invalidates_physical_synonym_code_store()) {
        return 1;
    }
    std::puts("dbt_runtime_invalidation_smoke: PASS");
    return 0;
}
