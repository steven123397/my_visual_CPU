#include <cstdio>
#include <cstdint>
#include <exception>
#include <stdexcept>
#include <string>
#include <vector>

#include "../../src/cpu.h"
#include "../../src/devices/clint.h"
#include "../../src/devices/device.h"
#include "../../src/isa/atomic_contract.h"
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

constexpr uint64_t kExternalDeviceBase =
    static_cast<uint64_t>(MEM_BASE) + static_cast<uint64_t>(MEM_SIZE) + 0x1000ULL;
constexpr uint64_t kFaultingDeviceBase = kExternalDeviceBase + 0x1000ULL;
constexpr uint64_t kSv39Mode = 8ULL << 60;
constexpr uint64_t kPteV = 1ULL << 0;
constexpr uint64_t kPteR = 1ULL << 1;
constexpr uint64_t kPteW = 1ULL << 2;
constexpr uint64_t kPteA = 1ULL << 6;
constexpr uint64_t kPteD = 1ULL << 7;

uint64_t pte_for(uint64_t paddr, uint64_t flags) {
    return ((paddr >> 12) << 10) | flags;
}

SimpleL1DataCache make_cache(size_t capacity_lines = 2) {
    return SimpleL1DataCache(SimpleL1DataCacheConfig{
        .enabled = true,
        .line_size_bytes = 64,
        .capacity_lines = capacity_lines,
    });
}

class NonCacheableRamDevice final : public Device {
public:
    NonCacheableRamDevice(uint64_t base, uint64_t size)
        : Device(base, size), bytes_(static_cast<size_t>(size), 0) {}

    uint64_t load(uint64_t addr, int size) override {
        ++loads_;
        const uint64_t offset = addr - base();
        uint64_t value = 0;
        for (int i = 0; i < size; ++i) {
            value |= static_cast<uint64_t>(bytes_[static_cast<size_t>(offset + i)]) << (i * 8);
        }
        return value;
    }

    void store(uint64_t addr, uint64_t value, int size) override {
        const uint64_t offset = addr - base();
        for (int i = 0; i < size; ++i) {
            bytes_[static_cast<size_t>(offset + i)] =
                static_cast<uint8_t>((value >> (i * 8)) & 0xFFU);
        }
    }

    const char* debug_name() const override {
        return "non-cacheable-ram";
    }

    PhysicalRegionInfo region_info() const override {
        return {
            .kind = PhysicalRegionKind::Ram,
            .cacheable = false,
            .dma_visible = true,
            .has_side_effect = false,
            .supports_burst = true,
            .label = debug_name(),
        };
    }

    uint64_t loads() const {
        return loads_;
    }

private:
    std::vector<uint8_t> bytes_;
    uint64_t loads_{0};
};

class FaultingCacheableRamDevice final : public Device {
public:
    FaultingCacheableRamDevice(uint64_t base, uint64_t size)
        : Device(base, size), bytes_(static_cast<size_t>(size), 0) {}

    uint64_t load(uint64_t addr, int size) override {
        if (fail_loads_) {
            throw std::runtime_error("forced cacheable RAM load fault");
        }
        const uint64_t offset = addr - base();
        uint64_t value = 0;
        for (int i = 0; i < size; ++i) {
            value |= static_cast<uint64_t>(bytes_[static_cast<size_t>(offset + i)]) << (i * 8);
        }
        return value;
    }

    void store(uint64_t addr, uint64_t value, int size) override {
        const uint64_t offset = addr - base();
        for (int i = 0; i < size; ++i) {
            bytes_[static_cast<size_t>(offset + i)] =
                static_cast<uint8_t>((value >> (i * 8)) & 0xFFU);
        }
    }

    const char* debug_name() const override {
        return "faulting-cacheable-ram";
    }

    PhysicalRegionInfo region_info() const override {
        return {
            .kind = PhysicalRegionKind::Ram,
            .cacheable = true,
            .dma_visible = true,
            .has_side_effect = false,
            .supports_burst = true,
            .label = debug_name(),
        };
    }

    void set_fail_loads(bool fail) {
        fail_loads_ = fail;
    }

private:
    std::vector<uint8_t> bytes_;
    bool fail_loads_{true};
};

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

bool test_cross_line_store_bypass_invalidates_overlapping_cached_line() {
    Ram ram;
    Bus bus(ram);
    SimpleL1DataCache cache = make_cache();
    const uint64_t line_base = MEM_BASE + 0x400;
    const uint64_t cached_word = line_base + 60;
    const uint64_t cross_line_store = line_base + 62;
    uint64_t value = 0;

    if (!bus.try_store(cached_word, 0xAAAABBBB, 4) ||
        !cache.load(bus, cached_word, 4, value) ||
        !cache.store(bus, cross_line_store, 0x11223344, 4) ||
        !cache.load(bus, cached_word, 4, value)) {
        return expect(false, "cross-line store hardening setup should succeed");
    }

    return expect(value == 0x3344BBBB,
                  "post-cross-line-store load should see backing RAM, not stale cached bytes") &&
           expect(cache.stats().bypasses == 1,
                  "cross-line store should bypass the L1D data path") &&
           expect(cache.stats().misses == 2,
                  "overlapping line should be invalidated and refilled after cross-line store");
}

bool test_store_miss_is_write_through_no_allocate() {
    Ram ram;
    Bus bus(ram);
    SimpleL1DataCache cache = make_cache();
    const uint64_t addr = MEM_BASE + 0x500;
    uint64_t value = 0;

    if (!cache.store(bus, addr, 0x01020304, 4) ||
        !bus.try_load(addr, 4, value)) {
        return expect(false, "store-miss write-through setup should succeed");
    }
    if (!expect(value == 0x01020304, "store miss should update backing RAM")) {
        return false;
    }

    if (!bus.try_store(addr, 0xAABBCCDD, 4) ||
        !cache.load(bus, addr, 4, value)) {
        return expect(false, "store-miss no-allocate follow-up should succeed");
    }

    return expect(value == 0xAABBCCDD,
                  "store miss should not allocate a stale cache line") &&
           expect(cache.stats().stores == 1, "store miss should count one store") &&
           expect(cache.stats().write_through_stores == 1,
                  "store miss should still be write-through") &&
           expect(cache.stats().misses == 2,
                  "store miss and later first load should both be observable misses") &&
           expect(cache.stats().hits == 0, "store miss should not create a cached hit");
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

bool test_non_cacheable_ram_bypasses_cache() {
    Ram ram;
    Bus bus(ram);
    NonCacheableRamDevice device(kExternalDeviceBase, 0x100);
    bus.attach(device);
    SimpleL1DataCache cache = make_cache();
    const uint64_t addr = kExternalDeviceBase + 0x10;
    uint64_t value = 0;

    device.store(addr, 0x11111111, 4);
    if (!cache.load(bus, addr, 4, value) || value != 0x11111111) {
        return expect(false, "first non-cacheable RAM load should bypass and return device value");
    }

    device.store(addr, 0x22222222, 4);
    if (!cache.load(bus, addr, 4, value)) {
        return expect(false, "second non-cacheable RAM load should bypass and return device value");
    }

    return expect(value == 0x22222222,
                  "non-cacheable RAM should not return a cached value") &&
           expect(device.loads() == 2, "non-cacheable RAM should be read on each load") &&
           expect(cache.stats().bypasses == 2, "non-cacheable RAM should count bypasses") &&
           expect(cache.stats().hits == 0 && cache.stats().misses == 0,
                  "non-cacheable RAM should not count cache hits or misses");
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

bool test_unmapped_access_bypasses_without_cache_pollution() {
    Ram ram;
    Bus bus(ram);
    SimpleL1DataCache cache = make_cache();
    uint64_t value = 0xFFFF;

    return expect(!cache.load(bus, kExternalDeviceBase + 0x8000, 4, value),
                  "unmapped load should fail through the backing bus") &&
           expect(value == 0, "unmapped load should clear the output value") &&
           expect(cache.stats().bypasses == 1, "unmapped load should count a bypass") &&
           expect(cache.stats().hits == 0 && cache.stats().misses == 0,
                  "unmapped load should not populate cache hit/miss state");
}

bool test_failed_refill_does_not_leave_valid_cache_line() {
    Ram ram;
    Bus bus(ram);
    FaultingCacheableRamDevice device(kFaultingDeviceBase, 0x100);
    bus.attach(device);
    SimpleL1DataCache cache = make_cache();
    const uint64_t addr = kFaultingDeviceBase + 0x20;
    uint64_t value = 0;

    device.store(addr, 0x12345678, 4);
    if (!expect(!cache.load(bus, addr, 4, value),
                "failed refill from cacheable RAM should report failure")) {
        return false;
    }

    device.set_fail_loads(false);
    if (!cache.load(bus, addr, 4, value)) {
        return expect(false, "post-fault refill retry should succeed");
    }

    return expect(value == 0x12345678, "refill retry should read the device value") &&
           expect(cache.stats().misses == 2,
                  "failed refill should not leave a valid line for the retry to hit") &&
           expect(cache.stats().hits == 0, "failed refill should not create a cache hit");
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

bool test_instruction_fetch_bypasses_enabled_l1d() {
    CPU cpu;
    Ram ram;
    Bus bus(ram);
    const uint64_t pc = MEM_BASE + 0x600;

    cpu.core().set_pc(pc);
    cpu.l1_data_cache().set_enabled(true);
    if (!bus.try_store(pc, 0x00100093, 4)) {
        return expect(false, "failed to seed instruction word");
    }

    const AddressSpace::AccessResult fetched = cpu.address_space().fetch32_result(bus);
    const SimpleL1DataCacheStats& stats = cpu.l1_data_cache().stats();
    return expect(fetched.ok && fetched.value == 0x00100093,
                  "instruction fetch should still read the instruction word") &&
           expect(stats.loads == 0 && stats.stores == 0 && stats.hits == 0 &&
                      stats.misses == 0 && stats.bypasses == 0,
                  "instruction fetch should not touch L1D counters");
}

bool test_page_walk_bypasses_enabled_l1d() {
    CPU cpu;
    Ram ram;
    Bus bus(ram);
    const uint64_t root = MEM_BASE + 0x1000;
    const uint64_t level1 = MEM_BASE + 0x2000;
    const uint64_t level0 = MEM_BASE + 0x3000;
    const uint64_t data_page = MEM_BASE + 0x7000;
    const uint64_t vaddr = 0x4000;

    cpu.core().set_privilege_mode(PrivilegeMode::Supervisor);
    cpu.csr().write(CSR_SATP, kSv39Mode | (root >> 12), cpu.core());
    cpu.l1_data_cache().set_enabled(true);

    ram.store(root, pte_for(level1, kPteV), 8);
    ram.store(level1, pte_for(level0, kPteV), 8);
    ram.store(level0 + 4 * 8, pte_for(data_page, kPteV | kPteR | kPteW | kPteA | kPteD), 8);
    ram.store(data_page, 0xCAFEBABE, 4);

    const AddressSpace::AccessResult loaded = cpu.address_space().load_result(bus, vaddr, 4);
    const SimpleL1DataCacheStats& stats = cpu.l1_data_cache().stats();
    return expect(loaded.ok && loaded.value == 0xCAFEBABE,
                  "Sv39 data load should resolve through the page tables") &&
           expect(stats.loads == 1 && stats.misses == 1,
                  "only the final data load should touch L1D") &&
           expect(stats.stores == 0 && stats.write_through_stores == 0,
                  "page-walk PTE reads should bypass L1D store/load accounting");
}

bool test_atomic_bypasses_enabled_l1d() {
    CPU cpu;
    Ram ram;
    Bus bus(ram);
    const uint64_t addr = MEM_BASE + 0x800;

    cpu.l1_data_cache().set_enabled(true);
    ram.store(addr, 0x10, 4);
    const AtomicApplyResult result =
        apply_atomic_request(cpu,
                             bus,
                             AtomicRequest{
                                 .kind = AtomicRequest::Kind::Swap,
                                 .addr = addr,
                                 .store_value = 0x20,
                                 .rd = 5,
                                 .size = 4,
                             });
    const SimpleL1DataCacheStats& stats = cpu.l1_data_cache().stats();

    return expect(result.ok, "atomic swap should complete") &&
           expect(ram.load(addr, 4) == 0x20, "atomic swap should update backing RAM") &&
           expect(stats.loads == 0 && stats.stores == 0 && stats.hits == 0 &&
                      stats.misses == 0 && stats.bypasses == 0,
                  "atomic memory operation should bypass L1D counters");
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
            !test_cross_line_store_bypass_invalidates_overlapping_cached_line() ||
            !test_store_miss_is_write_through_no_allocate() ||
            !test_write_through_updates_ram_and_cached_line() ||
            !test_non_cacheable_ram_bypasses_cache() ||
            !test_side_effect_mmio_bypasses_cache() ||
            !test_unmapped_access_bypasses_without_cache_pollution() ||
            !test_failed_refill_does_not_leave_valid_cache_line() ||
            !test_capacity_eviction_refills_ram_line() ||
            !test_address_space_uses_enabled_l1d_for_data_loads() ||
            !test_instruction_fetch_bypasses_enabled_l1d() ||
            !test_page_walk_bypasses_enabled_l1d() ||
            !test_atomic_bypasses_enabled_l1d() ||
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
