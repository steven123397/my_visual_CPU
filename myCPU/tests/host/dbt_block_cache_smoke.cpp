#include <cstdint>
#include <cstdio>
#include <vector>

#include "../../src/cpu.h"
#include "../../src/exec/dbt_block_cache.h"
#include "../../src/exec/dbt_block_plan.h"
#include "../../src/exec/dbt_translator.h"
#include "../../src/mem/bus.h"
#include "../../src/mem/ram.h"

namespace {

constexpr uint64_t kEntry = MEM_BASE;
constexpr uint32_t kAddiX1One = 0x00100093U;      // addi x1, x0, 1
constexpr uint32_t kAddiX2X1Two = 0x00208113U;    // addi x2, x1, 2
constexpr uint32_t kAddiX3X0Three = 0x00300193U;  // addi x3, x0, 3
constexpr uint32_t kAddiX4X3Four = 0x00418213U;   // addi x4, x3, 4
constexpr uint32_t kLwX1FromX0 = 0x00002083U;     // lw x1, 0(x0)

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

DbtTranslationUnit translate_program(Ram& ram,
                                     Bus& bus,
                                     CPU& cpu,
                                     uint64_t start_pc,
                                     const std::vector<uint32_t>& program) {
    for (size_t i = 0; i < program.size(); ++i) {
        write32(ram, start_pc + static_cast<uint64_t>(i * sizeof(uint32_t)), program[i]);
    }
    const uint64_t end_pc = start_pc + static_cast<uint64_t>((program.size() - 1) * sizeof(uint32_t));
    return translate_dbt_block(plan_dbt_block(cpu, bus, start_pc, end_pc));
}

bool test_cache_records_hit_miss_and_replaces_metadata() {
    Ram ram;
    Bus bus(ram);
    CPU cpu;
    cpu_init(cpu, kEntry);

    const DbtTranslationUnit unit =
        translate_program(ram, bus, cpu, kEntry, {kAddiX1One, kAddiX2X1Two});

    DbtBlockCache cache;
    const bool inserted = cache.insert(unit);
    const DbtTranslationUnit* hit = cache.lookup(kEntry, kEntry + 4);
    const DbtTranslationUnit* miss = cache.lookup(kEntry, kEntry + 8);
    const bool replaced = cache.insert(unit);
    const DbtBlockCacheStats stats = cache.stats();

    return expect(unit.ok, "cache smoke setup should create an accepted translation unit") &&
           expect(inserted, "metadata-only DBT block cache should accept ok translation units") &&
           expect(hit != nullptr && hit->start_pc == kEntry && hit->end_pc == kEntry + 4,
                  "metadata-only DBT block cache should return exact key hits") &&
           expect(miss == nullptr,
                  "metadata-only DBT block cache should miss different block ranges") &&
           expect(replaced,
                  "metadata-only DBT block cache should allow replacing same-key metadata") &&
           expect(cache.size() == 1,
                  "metadata-only DBT block cache should keep one entry for same-key replacement") &&
           expect(stats.entries == 1,
                  "metadata-only DBT block cache stats should expose entry count") &&
           expect(stats.lookups == 2 && stats.hits == 1 && stats.misses == 1,
                  "metadata-only DBT block cache should count hit and miss lookups") &&
           expect(stats.insertions == 2,
                  "metadata-only DBT block cache should count successful metadata inserts");
}

bool test_cache_rejects_failed_translation_units() {
    Ram ram;
    Bus bus(ram);
    CPU cpu;
    cpu_init(cpu, kEntry);

    const DbtTranslationUnit rejected =
        translate_program(ram, bus, cpu, kEntry, {kAddiX1One, kLwX1FromX0});

    DbtBlockCache cache;
    const bool inserted = cache.insert(rejected);
    const DbtBlockCacheStats stats = cache.stats();

    return expect(!rejected.ok, "cache reject setup should create rejected translation unit") &&
           expect(!inserted,
                  "metadata-only DBT block cache should not cache rejected translation units") &&
           expect(cache.size() == 0,
                  "metadata-only DBT block cache should remain empty after rejected insert") &&
           expect(stats.rejected_insertions == 1,
                  "metadata-only DBT block cache should count rejected inserts");
}

bool test_cache_invalidates_by_existing_dry_run_contract() {
    Ram ram;
    Bus bus(ram);
    CPU cpu;
    cpu_init(cpu, kEntry);

    const DbtTranslationUnit first =
        translate_program(ram, bus, cpu, kEntry, {kAddiX1One, kAddiX2X1Two});
    const DbtTranslationUnit second =
        translate_program(ram, bus, cpu, kEntry + 0x40, {kAddiX3X0Three, kAddiX4X3Four});

    DbtBlockCache cache;
    cache.insert(first);
    cache.insert(second);

    const DbtBlockCacheInvalidationResult disjoint =
        cache.invalidate(DbtInvalidationEventKind::PayloadLoad, kEntry + 0x1000, 4);
    const DbtBlockCacheInvalidationResult overlapping =
        cache.invalidate(DbtInvalidationEventKind::GuestStore, kEntry + 4, 4);
    const DbtTranslationUnit* removed = cache.lookup(kEntry, kEntry + 4);
    const DbtTranslationUnit* survivor = cache.lookup(kEntry + 0x40, kEntry + 0x44);
    const DbtBlockCacheInvalidationResult global =
        cache.invalidate(DbtInvalidationEventKind::SfenceVma, 0, 0);
    const DbtBlockCacheStats stats = cache.stats();

    return expect(first.ok && second.ok,
                  "cache invalidation setup should create accepted translation units") &&
           expect(!disjoint.invalidated && disjoint.entries_removed == 0 &&
                      disjoint.entries_examined == 2 && disjoint.reason == "range-disjoint",
                  "metadata-only DBT block cache should keep disjoint payload updates") &&
           expect(overlapping.invalidated && overlapping.entries_removed == 1 &&
                      overlapping.entries_examined == 2 && overlapping.reason == "guest-store-overlaps-block",
                  "metadata-only DBT block cache should invalidate overlapping guest stores") &&
           expect(removed == nullptr,
                  "metadata-only DBT block cache should remove invalidated block metadata") &&
           expect(survivor != nullptr,
                  "metadata-only DBT block cache should keep disjoint block metadata") &&
           expect(global.invalidated && global.entries_removed == 1 && global.entries_examined == 1 &&
                      global.reason == "sfence-vma",
                  "metadata-only DBT block cache should apply global TLB invalidation events") &&
           expect(cache.size() == 0,
                  "metadata-only DBT block cache should be empty after global invalidation") &&
           expect(stats.invalidation_checks == 3 && stats.invalidations == 2 &&
                      stats.non_invalidating_events == 1 && stats.invalidated_entries == 2,
                  "metadata-only DBT block cache should count invalidation checks and entries");
}

bool test_cache_global_invalidation_matrix_is_stable() {
    DbtBlockCache empty_cache;
    const DbtBlockCacheInvalidationResult empty_global =
        empty_cache.invalidate(DbtInvalidationEventKind::PrimaryImageLoad, 0, 0);
    const DbtBlockCacheStats empty_stats = empty_cache.stats();

    Ram ram;
    Bus bus(ram);
    CPU cpu;
    cpu_init(cpu, kEntry);

    const DbtTranslationUnit first =
        translate_program(ram, bus, cpu, kEntry, {kAddiX1One, kAddiX2X1Two});
    const DbtTranslationUnit second =
        translate_program(ram, bus, cpu, kEntry + 0x40, {kAddiX3X0Three, kAddiX4X3Four});

    const DbtInvalidationEventKind global_events[] = {
        DbtInvalidationEventKind::PrimaryImageLoad,
        DbtInvalidationEventKind::DebugReset,
        DbtInvalidationEventKind::SatpWrite,
        DbtInvalidationEventKind::SfenceVma,
        DbtInvalidationEventKind::RegionAttributesChanged,
    };
    const char* global_reasons[] = {
        "primary-image-load",
        "debug-reset",
        "satp-write",
        "sfence-vma",
        "region-attributes-changed",
    };

    bool ok = expect(!empty_global.invalidated && empty_global.entries_removed == 0 &&
                         empty_global.entries_examined == 0 &&
                         empty_global.reason == "primary-image-load",
                     "metadata-only DBT block cache should classify empty global invalidations") &&
              expect(empty_stats.invalidation_checks == 1 &&
                         empty_stats.non_invalidating_events == 1,
                     "metadata-only DBT block cache should count empty invalidation checks");

    for (size_t i = 0; i < sizeof(global_events) / sizeof(global_events[0]); ++i) {
        DbtBlockCache cache;
        cache.insert(first);
        cache.insert(second);

        const DbtBlockCacheInvalidationResult result = cache.invalidate(global_events[i], 0, 0);
        const DbtBlockCacheStats stats = cache.stats();

        ok = ok &&
             expect(result.invalidated && result.entries_removed == 2 &&
                        result.entries_examined == 2 && result.reason == global_reasons[i],
                    "metadata-only DBT block cache should invalidate all entries for global events") &&
             expect(cache.size() == 0,
                    "metadata-only DBT block cache should remove all metadata for global events") &&
             expect(stats.invalidation_checks == 1 && stats.invalidations == 1 &&
                        stats.invalidated_entries == 2,
                    "metadata-only DBT block cache should count global invalidation matrix events");
    }

    return expect(first.ok && second.ok,
                  "cache global invalidation setup should create accepted translation units") &&
           ok;
}

}  // namespace

int main() {
    if (!test_cache_records_hit_miss_and_replaces_metadata()) {
        return 1;
    }
    if (!test_cache_rejects_failed_translation_units()) {
        return 1;
    }
    if (!test_cache_invalidates_by_existing_dry_run_contract()) {
        return 1;
    }
    if (!test_cache_global_invalidation_matrix_is_stable()) {
        return 1;
    }
    std::puts("dbt_block_cache_smoke: PASS");
    return 0;
}
