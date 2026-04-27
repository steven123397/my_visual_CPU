#include <array>
#include <cstdint>
#include <cstdio>
#include <string>

#include "../../src/arch/csr_file.h"
#include "../../src/cpu.h"
#include "../../src/debug/debug_protocol.h"
#include "../../src/exec/functional_backend.h"
#include "../../src/exec/pipeline_backend.h"
#include "../../src/mem/bus.h"
#include "../../src/mem/ram.h"

namespace {

constexpr uint64_t kEntry = MEM_BASE;
constexpr uint64_t kDataAddr = kEntry + 0x40;
constexpr uint64_t kTrapVector = kEntry + 0x80;
constexpr uint64_t kRootPageTable = 0x80100000ULL;
constexpr uint64_t kLevel1PageTable = 0x80101000ULL;
constexpr uint64_t kLevel0PageTable = 0x80102000ULL;
constexpr uint64_t kSatpModeSv39 = 8ULL << 60;
constexpr uint64_t kPteV = 1ULL << 0;
constexpr uint64_t kPteR = 1ULL << 1;
constexpr uint64_t kPteW = 1ULL << 2;
constexpr uint64_t kPteX = 1ULL << 3;
constexpr uint32_t kAuipcX10 = 0x00000517U;             // auipc x10, 0
constexpr uint32_t kAddiX10Plus64 = 0x04050513U;        // addi x10, x10, 64
constexpr uint32_t kLwX6FromX10 = 0x00052303U;          // lw x6, 0(x10)
constexpr uint32_t kLwX6FromX10Again = 0x00052303U;     // lw x6, 0(x10)
constexpr uint32_t kSwX6ToX10Plus4 = 0x00652223U;       // sw x6, 4(x10)
constexpr uint32_t kJalX0Skip8 = 0x0080006fU;           // jal x0, 8
constexpr uint32_t kAddiX1WrongPath = 0x06300093U;      // addi x1, x0, 99
constexpr uint32_t kAddiA7Exit = 0x05d00893U;           // addi a7, x0, 93
constexpr uint32_t kEcall = 0x00000073U;                // ecall

bool expect(bool condition, const char* message) {
    if (!condition) {
        std::fprintf(stderr, "%s\n", message);
        return false;
    }
    return true;
}

bool expect_contains(const std::string& haystack, const char* needle, const char* message) {
    if (haystack.find(needle) == std::string::npos) {
        std::fprintf(stderr, "%s\n", message);
        std::fprintf(stderr, "output was:\n%s\n", haystack.c_str());
        return false;
    }
    return true;
}

void write32(Ram& ram, uint64_t addr, uint32_t value) {
    ram.write_bytes(addr, &value, sizeof(value));
}

void write64(Ram& ram, uint64_t addr, uint64_t value) {
    ram.store(addr, value, 8);
}

uint64_t sv39_vpn(uint64_t vaddr, int level) {
    return (vaddr >> (12 + level * 9)) & 0x1ffULL;
}

void map_sv39_leaf(Ram& ram, uint64_t vaddr, uint64_t paddr, uint64_t flags) {
    write64(ram,
            kRootPageTable + sv39_vpn(vaddr, 2) * 8,
            ((kLevel1PageTable >> 12) << 10) | kPteV);
    write64(ram,
            kLevel1PageTable + sv39_vpn(vaddr, 1) * 8,
            ((kLevel0PageTable >> 12) << 10) | kPteV);
    write64(ram,
            kLevel0PageTable + sv39_vpn(vaddr, 0) * 8,
            ((paddr >> 12) << 10) | flags);
}

const ExecutionMemoryRegionEntry* find_memory_region(const ExecutionProfileSnapshot& profile,
                                                     const char* kind) {
    for (const ExecutionMemoryRegionEntry& entry : profile.memory_regions) {
        if (entry.kind == kind) {
            return &entry;
        }
    }
    return nullptr;
}

bool test_debug_snapshot_json_exposes_profile_contract() {
    DebugSnapshot snapshot{};
    snapshot.summary.backend = "pipeline";
    const std::string output = debug_snapshot_json(snapshot);
    return expect_contains(output, "\"profile\":{",
                           "debug snapshot JSON should expose a top-level profile section") &&
           expect_contains(output, "\"hot_paths\":[",
                           "debug snapshot JSON should expose hot-path observations") &&
           expect_contains(output, "\"memory_regions\":[",
                           "debug snapshot JSON should expose memory-region observations") &&
           expect_contains(output, "\"traps\":[",
                           "debug snapshot JSON should expose trap observations");
}

bool test_pipeline_snapshot_json_exposes_hot_path_memory_and_trap_signals() {
    Ram ram;
    Bus bus(ram);
    CPU cpu;
    cpu_init(cpu, kEntry);

    write32(ram, kEntry + 0, kAuipcX10);
    write32(ram, kEntry + 4, kAddiX10Plus64);
    write32(ram, kEntry + 8, kLwX6FromX10);
    write32(ram, kEntry + 12, kSwX6ToX10Plus4);
    write32(ram, kEntry + 16, kJalX0Skip8);
    write32(ram, kEntry + 20, kAddiX1WrongPath);
    write32(ram, kEntry + 24, kAddiA7Exit);
    write32(ram, kEntry + 28, kEcall);
    ram.write_bytes(kDataAddr, std::array<uint8_t, 4>{1, 0, 0, 0}.data(), 4);

    PipelineBackend backend(cpu, bus);
    for (int step = 0; step < 48 && !cpu.core().halted(); ++step) {
        backend.step();
    }

    DebugSnapshot snapshot{};
    snapshot.summary.pc = cpu.core().pc();
    snapshot.summary.instret = cpu.core().instret();
    snapshot.summary.halted = cpu.core().halted();
    snapshot.summary.backend = backend.name();
    snapshot.pipeline = backend.debug_snapshot().pipeline;
    snapshot.profile = backend.debug_snapshot().profile;
    const std::string output = debug_snapshot_json(snapshot);

    return expect(cpu.core().halted(), "execution profile smoke should halt") &&
           expect_contains(output, "\"profile\":{",
                           "pipeline snapshot JSON should expose profile output") &&
           expect_contains(output, "\"hot_paths\":[",
                           "pipeline snapshot JSON should expose hot-path output") &&
           expect_contains(output, "\"memory_regions\":[",
                           "pipeline snapshot JSON should expose memory-region output") &&
           expect_contains(output, "\"traps\":[",
                           "pipeline snapshot JSON should expose trap output");
}

bool test_pipeline_profile_counts_faulting_memory_observation() {
    Ram ram;
    Bus bus(ram);
    CPU cpu;
    cpu_init(cpu, kEntry);

    map_sv39_leaf(ram, kEntry, kEntry, kPteV | kPteR | kPteX);
    write32(ram, kEntry + 0, kLwX6FromX10);
    write32(ram, kTrapVector + 0, kAddiA7Exit);
    write32(ram, kTrapVector + 4, kEcall);

    cpu.core().write_gpr(10, 1ULL << 39);
    cpu.core().set_privilege_mode(PrivilegeMode::Supervisor);
    cpu.csr().write(CSR_STVEC, kTrapVector, cpu.core());
    cpu.csr().write(CSR_MEDELEG, 1ULL << 13, cpu.core());
    cpu.csr().write(CSR_SATP, kSatpModeSv39 | (kRootPageTable >> 12), cpu.core());
    cpu.address_space().flush_tlb();

    PipelineBackend backend(cpu, bus);
    for (int step = 0; step < 32 && !cpu.core().halted(); ++step) {
        backend.step();
    }

    const ExecutionProfileSnapshot profile = backend.debug_snapshot().profile;
    const ExecutionMemoryRegionEntry* unmapped = find_memory_region(profile, "unmapped");

    return expect(cpu.core().halted(),
                  "faulting-memory profile smoke should halt via trap handler ecall") &&
           expect(profile.total_traps == 1,
                  "faulting-memory profile smoke should record the load page fault trap") &&
           expect(profile.total_memory_observations == 1,
                  "faulting-memory profile smoke should count the faulting memory access") &&
           expect(unmapped != nullptr,
                  "faulting-memory profile smoke should classify translation failures as unmapped observations") &&
           expect(unmapped->reads == 1,
                  "faulting-memory profile smoke should count the load fault as a read observation") &&
           expect(unmapped->writes == 0,
                  "faulting-memory profile smoke should not count the load fault as a write observation") &&
           expect(unmapped->faults == 1,
                  "faulting-memory profile smoke should increment the unmapped fault counter") &&
           expect(unmapped->bytes == 4,
                  "faulting-memory profile smoke should preserve the access width");
}

bool test_pipeline_shadow_cache_counts_reused_ram_line() {
    Ram ram;
    Bus bus(ram);
    CPU cpu;
    cpu_init(cpu, kEntry);

    write32(ram, kEntry + 0, kAuipcX10);
    write32(ram, kEntry + 4, kAddiX10Plus64);
    write32(ram, kEntry + 8, kLwX6FromX10);
    write32(ram, kEntry + 12, kLwX6FromX10Again);
    write32(ram, kEntry + 16, kAddiA7Exit);
    write32(ram, kEntry + 20, kEcall);
    ram.write_bytes(kDataAddr, std::array<uint8_t, 4>{1, 0, 0, 0}.data(), 4);

    PipelineBackend backend(cpu, bus);
    for (int step = 0; step < 32 && !cpu.core().halted(); ++step) {
        backend.step();
    }

    const BackendDebugSnapshot backend_snapshot = backend.debug_snapshot();
    const ExecutionProfileSnapshot profile = backend.debug_snapshot().profile;
    DebugSnapshot snapshot{};
    snapshot.summary.pc = cpu.core().pc();
    snapshot.summary.instret = cpu.core().instret();
    snapshot.summary.halted = cpu.core().halted();
    snapshot.summary.privilege = cpu.core().privilege_mode();
    snapshot.summary.backend = backend.name();
    snapshot.pipeline = backend_snapshot.pipeline;
    snapshot.profile = profile;
    const std::string output = debug_snapshot_json(snapshot);

    return expect(cpu.core().halted(),
                  "shadow-cache profile smoke should halt via ecall") &&
           expect_contains(output,
                           "\"shadow_cache\":{\"line_size_bytes\":64,\"capacity_lines\":64,\"resident_lines\":1,\"line_accesses\":2,\"hits\":1,\"misses\":1,\"evictions\":0,\"bypasses\":0}",
                           "shadow-cache profile smoke should count one miss followed by one hit on the reused RAM line");
}

bool test_functional_profile_counts_reused_ram_line() {
    Ram ram;
    Bus bus(ram);
    CPU cpu;
    cpu_init(cpu, kEntry);

    write32(ram, kEntry + 0, kAuipcX10);
    write32(ram, kEntry + 4, kAddiX10Plus64);
    write32(ram, kEntry + 8, kLwX6FromX10);
    write32(ram, kEntry + 12, kLwX6FromX10Again);
    write32(ram, kEntry + 16, kAddiA7Exit);
    write32(ram, kEntry + 20, kEcall);
    ram.write_bytes(kDataAddr, std::array<uint8_t, 4>{1, 0, 0, 0}.data(), 4);

    FunctionalBackend backend(cpu, bus);
    for (int step = 0; step < 16 && !cpu.core().halted(); ++step) {
        backend.step();
    }

    const ExecutionProfileSnapshot profile = backend.debug_snapshot().profile;
    const ExecutionMemoryRegionEntry* ram_region = find_memory_region(profile, "ram");

    return expect(cpu.core().halted(),
                  "functional shadow-cache profile smoke should halt via ecall") &&
           expect(profile.total_retirements == 6,
                  "functional shadow-cache profile smoke should record each retired instruction") &&
           expect(profile.total_memory_observations == 2,
                  "functional shadow-cache profile smoke should count both load observations") &&
           expect(ram_region != nullptr,
                  "functional shadow-cache profile smoke should classify the load observations as RAM") &&
           expect(ram_region->reads == 2,
                  "functional shadow-cache profile smoke should count both observations as reads") &&
           expect(ram_region->writes == 0,
                  "functional shadow-cache profile smoke should not classify the load observations as writes") &&
           expect(profile.shadow_cache.line_accesses == 2,
                  "functional shadow-cache profile smoke should count both RAM line accesses") &&
           expect(profile.shadow_cache.hits == 1,
                  "functional shadow-cache profile smoke should record one reused-line hit") &&
           expect(profile.shadow_cache.misses == 1,
                  "functional shadow-cache profile smoke should record one first-touch miss") &&
           expect(ram_region->shadow_cache_line_accesses == 2,
                  "functional shadow-cache profile smoke should export region-level line accesses") &&
           expect(ram_region->shadow_cache_hits == 1,
                  "functional shadow-cache profile smoke should export the RAM-region hit count") &&
           expect(ram_region->shadow_cache_misses == 1,
                  "functional shadow-cache profile smoke should export the RAM-region miss count");
}

}  // namespace

int main() {
    if (!test_debug_snapshot_json_exposes_profile_contract()) {
        return 1;
    }
    if (!test_pipeline_snapshot_json_exposes_hot_path_memory_and_trap_signals()) {
        return 1;
    }
    if (!test_pipeline_profile_counts_faulting_memory_observation()) {
        return 1;
    }
    if (!test_pipeline_shadow_cache_counts_reused_ram_line()) {
        return 1;
    }
    if (!test_functional_profile_counts_reused_ram_line()) {
        return 1;
    }
    std::puts("execution_profile_smoke: PASS");
    return 0;
}
