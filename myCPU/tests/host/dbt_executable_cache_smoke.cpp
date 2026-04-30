#include <cstdint>
#include <cstdio>

#include "../../src/cpu.h"
#include "../../src/exec/dbt_executable_cache.h"
#include "../../src/exec/dbt_jit_engine.h"
#include "../../src/exec/dbt_runtime_dispatch.h"
#include "../../src/mem/bus.h"
#include "../../src/mem/ram.h"

namespace {

constexpr uint64_t kEntry = MEM_BASE;
constexpr uint32_t kAddiX1One = 0x00100093U;  // addi x1, x0, 1
constexpr uint32_t kLwX1FromX0 = 0x00002083U; // lw x1, 0(x0)

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

DbtRuntimeDispatchContract lowered_contract(Ram& ram,
                                             Bus& bus,
                                             CPU& cpu,
                                             uint64_t pc) {
    write32(ram, pc, kAddiX1One);
    DbtJitEngineDryRun engine;
    return plan_dbt_runtime_dispatch_contract(engine.dry_run_block(cpu, bus, pc, pc));
}

DbtRuntimeDispatchContract helper_contract(Ram& ram,
                                            Bus& bus,
                                            CPU& cpu,
                                            uint64_t pc) {
    write32(ram, pc, kLwX1FromX0);
    DbtJitEngineDryRun engine;
    return plan_dbt_runtime_dispatch_contract(engine.dry_run_block(cpu, bus, pc, pc));
}

bool test_executable_cache_enforces_overlapping_store_invalidation() {
    Ram ram;
    Bus bus(ram);
    CPU cpu;
    cpu_init(cpu, kEntry);

    const DbtRuntimeDispatchContract contract = lowered_contract(ram, bus, cpu, kEntry);
    DbtExecutableCacheDryRun cache;
    const bool inserted = cache.insert(contract);
    const DbtExecutableCacheLookup before = cache.lookup(kEntry, kEntry);
    const DbtExecutableCacheInvalidationResult disjoint =
        cache.enforce_invalidation(DbtInvalidationEventKind::GuestStore, kEntry + 0x40, 4);
    const DbtExecutableCacheLookup after_disjoint = cache.lookup(kEntry, kEntry);
    const DbtExecutableCacheInvalidationResult overlap =
        cache.enforce_invalidation(DbtInvalidationEventKind::GuestStore, kEntry, 4);
    const DbtExecutableCacheLookup after_overlap = cache.lookup(kEntry, kEntry);
    const DbtExecutableCacheDryRunStats stats = cache.stats();

    return expect(contract.ok && contract.kind == DbtRuntimeDispatchKind::LoweredBlock,
                  "executable cache setup should create a lowered dispatch contract") &&
           expect(inserted, "executable cache dry-run should accept lowered block contracts") &&
           expect(before.hit && before.contract.start_pc == kEntry,
                  "executable cache dry-run should return resident lowered contracts") &&
           expect(!before.contract.generated_host_code &&
                      !before.contract.requested_executable_memory &&
                      !before.contract.executed_guest_code,
                  "executable cache dry-run should never report host code or executable memory") &&
           expect(!disjoint.invalidated && disjoint.entries_removed == 0 &&
                      disjoint.entries_examined == 1 && disjoint.reason == "range-disjoint",
                  "executable cache dry-run should keep disjoint guest stores") &&
           expect(after_disjoint.hit,
                  "disjoint invalidation should leave lowered contract resident") &&
           expect(overlap.invalidated && overlap.entries_removed == 1 &&
                      overlap.entries_examined == 1 &&
                      overlap.reason == "guest-store-overlaps-block" &&
                      overlap.stale_dispatch_prevented,
                  "overlapping guest stores should force executable cache invalidation") &&
           expect(!after_overlap.hit && after_overlap.reason == "miss",
                  "invalidated executable cache entries should not be dispatchable") &&
           expect(stats.insertions == 1 && stats.lookups == 3 && stats.hits == 2 &&
                      stats.misses == 1 && stats.invalidation_enforcements == 2 &&
                      stats.invalidations == 1 && stats.stale_dispatches_prevented == 1,
                  "executable cache dry-run should count enforced invalidations and stale prevention");
}

bool test_executable_cache_rejects_non_lowered_dispatch_contracts() {
    Ram ram;
    Bus bus(ram);
    CPU cpu;
    cpu_init(cpu, kEntry);

    const DbtRuntimeDispatchContract helper = helper_contract(ram, bus, cpu, kEntry);
    DbtExecutableCacheDryRun cache;
    const bool inserted = cache.insert(helper);
    const DbtExecutableCacheDryRunStats stats = cache.stats();

    return expect(helper.kind == DbtRuntimeDispatchKind::HelperBridgeToReference,
                  "helper setup should create a helper bridge contract") &&
           expect(!inserted,
                  "executable cache dry-run should reject helper/reference contracts") &&
           expect(cache.size() == 0,
                  "rejected executable cache contracts should not become resident") &&
           expect(stats.rejected_insertions == 1,
                  "executable cache dry-run should count rejected insertions");
}

bool test_executable_cache_global_invalidation_enforcement_is_stable() {
    Ram ram;
    Bus bus(ram);
    CPU cpu;
    cpu_init(cpu, kEntry);

    const DbtRuntimeDispatchContract first = lowered_contract(ram, bus, cpu, kEntry);
    const DbtRuntimeDispatchContract second = lowered_contract(ram, bus, cpu, kEntry + 0x40);

    DbtExecutableCacheDryRun cache;
    cache.insert(first);
    cache.insert(second);

    const DbtExecutableCacheInvalidationResult result =
        cache.enforce_invalidation(DbtInvalidationEventKind::PrimaryImageLoad, 0, 0);
    const DbtExecutableCacheLookup first_lookup = cache.lookup(kEntry, kEntry);
    const DbtExecutableCacheLookup second_lookup = cache.lookup(kEntry + 0x40, kEntry + 0x40);
    const DbtExecutableCacheDryRunStats stats = cache.stats();

    return expect(first.ok && second.ok,
                  "global executable cache setup should create lowered dispatch contracts") &&
           expect(result.invalidated && result.entries_removed == 2 &&
                      result.entries_examined == 2 && result.reason == "primary-image-load" &&
                      result.stale_dispatch_prevented,
                  "global events should force executable cache invalidation") &&
           expect(!first_lookup.hit && !second_lookup.hit,
                  "global invalidation should prevent all stale executable dispatches") &&
           expect(stats.invalidations == 1 && stats.invalidated_entries == 2 &&
                      stats.stale_dispatches_prevented == 2,
                  "global executable cache invalidation should count removed entries");
}

}  // namespace

int main() {
    if (!test_executable_cache_enforces_overlapping_store_invalidation()) {
        return 1;
    }
    if (!test_executable_cache_rejects_non_lowered_dispatch_contracts()) {
        return 1;
    }
    if (!test_executable_cache_global_invalidation_enforcement_is_stable()) {
        return 1;
    }
    std::puts("dbt_executable_cache_smoke: PASS");
    return 0;
}
