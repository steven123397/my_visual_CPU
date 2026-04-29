#include <cstdio>
#include <exception>
#include <string>

#include "../../src/cpu.h"
#include "../../src/devices/clint.h"
#include "../../src/mem/bus.h"
#include "../../src/mem/ram.h"
#include "../../src/mem/simple_l1_cache.h"
#include "../../src/platform/machine.h"
#include "../../src/platform/address_map.h"

namespace {

bool expect(bool condition, const char* message) {
    if (!condition) {
        std::fprintf(stderr, "%s\n", message);
        return false;
    }
    return true;
}

SimpleL1DataCache make_cache(size_t capacity_lines = 2) {
    return SimpleL1DataCache(SimpleL1DataCacheConfig{
        .enabled = true,
        .line_size_bytes = 64,
        .capacity_lines = capacity_lines,
    });
}

bool test_reused_ram_line_records_miss_then_hit() {
    Ram ram;
    Bus bus(ram);
    SimpleL1DataCache cache = make_cache();
    const uint64_t addr = MEM_BASE + 0x100;

    if (!bus.try_store(addr, 0x11223344, 4)) {
        return expect(false, "failed to seed RAM");
    }

    uint64_t value = 0;
    return expect(cache.load(bus, addr, 4, value), "first cached load should succeed") &&
           expect(value == 0x11223344, "first cached load should return RAM value") &&
           expect(cache.stats().misses == 1, "first cached load should miss") &&
           expect(cache.stats().hits == 0, "first cached load should not hit") &&
           expect(cache.load(bus, addr, 4, value), "second cached load should succeed") &&
           expect(value == 0x11223344, "second cached load should return cached value") &&
           expect(cache.stats().misses == 1, "second cached load should not add a miss") &&
           expect(cache.stats().hits == 1, "second cached load should hit");
}

bool test_write_through_updates_ram_and_cached_line() {
    Ram ram;
    Bus bus(ram);
    SimpleL1DataCache cache = make_cache();
    const uint64_t addr = MEM_BASE + 0x140;
    uint64_t value = 0;

    if (!bus.try_store(addr, 0x01020304, 4) ||
        !cache.load(bus, addr, 4, value) ||
        !cache.store(bus, addr, 0xA0B0C0D0, 4) ||
        !bus.try_load(addr, 4, value)) {
        return expect(false, "write-through setup should succeed");
    }

    return expect(value == 0xA0B0C0D0, "write-through store should update backing RAM") &&
           expect(cache.load(bus, addr, 4, value), "post-store cached load should succeed") &&
           expect(value == 0xA0B0C0D0, "post-store cached load should see updated value") &&
           expect(cache.stats().write_through_stores == 1,
                  "cache should count write-through stores") &&
           expect(cache.stats().hits >= 1, "updated cached line should remain readable");
}

bool test_side_effect_mmio_bypasses_cache() {
    Ram ram;
    Bus bus(ram);
    Clint clint;
    bus.attach(clint);
    SimpleL1DataCache cache = make_cache();

    return expect(cache.store(bus, CLINT_BASE + CLINT_REG_MTIME, 0x1234, 8),
                  "MMIO store should still reach device") &&
           expect(clint.mtime() == 0x1234, "MMIO store should update CLINT state") &&
           expect(cache.stats().bypasses == 1, "MMIO store should bypass L1D") &&
           expect(cache.stats().write_through_stores == 0,
                  "MMIO store should not count as write-through RAM store");
}

bool test_capacity_eviction_refills_ram_line() {
    Ram ram;
    Bus bus(ram);
    SimpleL1DataCache cache = make_cache(1);
    const uint64_t first = MEM_BASE + 0x200;
    const uint64_t second = MEM_BASE + 0x240;
    uint64_t value = 0;

    if (!bus.try_store(first, 0x11111111, 4) ||
        !bus.try_store(second, 0x22222222, 4) ||
        !cache.load(bus, first, 4, value) ||
        !cache.load(bus, second, 4, value) ||
        !cache.load(bus, first, 4, value)) {
        return expect(false, "eviction setup should succeed");
    }

    return expect(value == 0x11111111, "refilled line should return original RAM value") &&
           expect(cache.stats().misses == 3, "single-line cache should miss after eviction") &&
           expect(cache.stats().evictions == 2, "single-line cache should evict on each replacement");
}

bool test_address_space_uses_enabled_l1d_for_data_loads() {
    CPU cpu;
    Ram ram;
    Bus bus(ram);
    const uint64_t addr = MEM_BASE + 0x300;

    cpu.l1_data_cache().set_enabled(true);
    if (!bus.try_store(addr, 0x55667788, 4)) {
        return expect(false, "failed to seed RAM for address-space cache test");
    }

    const AddressSpace::AccessResult first = cpu.address_space().load_result(bus, addr, 4);
    const AddressSpace::AccessResult second = cpu.address_space().load_result(bus, addr, 4);
    return expect(first.ok && first.value == 0x55667788,
                  "first address-space cached load should return RAM value") &&
           expect(second.ok && second.value == 0x55667788,
                  "second address-space cached load should return cached value") &&
           expect(cpu.l1_data_cache().stats().misses == 1,
                  "address-space cached loads should record first miss") &&
           expect(cpu.l1_data_cache().stats().hits == 1,
                  "address-space cached loads should record second hit");
}

bool test_machine_l1d_switch_defaults_off_and_can_enable() {
    Machine machine;

    return expect(!machine.l1_data_cache_enabled(), "machine should default L1D off") &&
           expect(!machine.cpu().l1_data_cache().enabled(), "CPU L1D should default off") &&
           (machine.set_l1_data_cache_enabled(true), true) &&
           expect(machine.l1_data_cache_enabled(), "machine should expose enabled L1D state") &&
           expect(machine.cpu().l1_data_cache().enabled(), "machine switch should enable CPU L1D") &&
           (machine.set_l1_data_cache_enabled(false), true) &&
           expect(!machine.l1_data_cache_enabled(), "machine switch should disable L1D");
}

}  // namespace

int main() {
    try {
        if (!test_reused_ram_line_records_miss_then_hit() ||
            !test_write_through_updates_ram_and_cached_line() ||
            !test_side_effect_mmio_bypasses_cache() ||
            !test_capacity_eviction_refills_ram_line() ||
            !test_address_space_uses_enabled_l1d_for_data_loads() ||
            !test_machine_l1d_switch_defaults_off_and_can_enable()) {
            return 1;
        }
        std::puts("simple_l1_cache: PASS");
        return 0;
    } catch (const std::exception& ex) {
        std::fprintf(stderr, "%s\n", ex.what());
        return 1;
    }
}
