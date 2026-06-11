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
constexpr uint64_t kMachineExitVector = kEntry + 0xc0;
constexpr uint64_t kRootPageTable = 0x80100000ULL;
constexpr uint64_t kLevel1PageTable = 0x80101000ULL;
constexpr uint64_t kLevel0PageTable = 0x80102000ULL;
constexpr uint64_t kSatpModeSv39 = 8ULL << 60;
constexpr uint64_t kPteV = 1ULL << 0;
constexpr uint64_t kPteR = 1ULL << 1;
constexpr uint64_t kPteW = 1ULL << 2;
constexpr uint64_t kPteX = 1ULL << 3;
constexpr uint32_t kOpcodeAmo = 0x2fU;
constexpr uint32_t kAuipcX10 = 0x00000517U;             // auipc x10, 0
constexpr uint32_t kAddiX10Plus64 = 0x04050513U;        // addi x10, x10, 64
constexpr uint32_t kLwX6FromX10 = 0x00052303U;          // lw x6, 0(x10)
constexpr uint32_t kLwX6FromX10Again = 0x00052303U;     // lw x6, 0(x10)
constexpr uint32_t kSwX6ToX10Plus4 = 0x00652223U;       // sw x6, 4(x10)
constexpr uint32_t kJalX0Skip8 = 0x0080006fU;           // jal x0, 8
constexpr uint32_t kAddiX1WrongPath = 0x06300093U;      // addi x1, x0, 99
constexpr uint32_t kAddiA7Exit = 0x05d00893U;           // addi a7, x0, 93
constexpr uint32_t kEcall = 0x00000073U;                // ecall

constexpr uint32_t encode_amo(uint32_t funct5,
                              bool aq,
                              bool rl,
                              uint32_t rs2,
                              uint32_t rs1,
                              uint32_t funct3,
                              uint32_t rd) {
    return (funct5 << 27) |
           (static_cast<uint32_t>(aq) << 26) |
           (static_cast<uint32_t>(rl) << 25) |
           (rs2 << 20) |
           (rs1 << 15) |
           (funct3 << 12) |
           (rd << 7) |
           kOpcodeAmo;
}

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

const ExecutionPcCostEntry* find_pc_cost(const ExecutionProfileSnapshot& profile, uint64_t pc) {
    for (const ExecutionPcCostEntry& entry : profile.pc_costs) {
        if (entry.pc == pc) {
            return &entry;
        }
    }
    return nullptr;
}

const ExecutionBranchTargetEntry* find_branch_target(const ExecutionProfileSnapshot& profile,
                                                     uint64_t pc,
                                                     uint64_t target_pc) {
    for (const ExecutionBranchTargetEntry& entry : profile.branch_targets) {
        if (entry.pc == pc && entry.target_pc == target_pc) {
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
           expect_contains(output, "\"pc_costs\":[",
                           "debug snapshot JSON should expose per-PC cost observations") &&
           expect_contains(output, "\"branch_targets\":[",
                           "debug snapshot JSON should expose branch-target heat observations") &&
           expect_contains(output, "\"traps\":[",
                           "debug snapshot JSON should expose trap observations") &&
           expect_contains(output, "\"observation_event\":{",
                           "debug snapshot JSON should expose execution profile observation event") &&
           expect_contains(output, "\"source\":\"execution-profile\"",
                           "execution profile event should identify its source") &&
           expect_contains(output, "\"phase\":\"snapshot-summary\"",
                           "execution profile event should identify snapshot summary phase") &&
           expect_contains(output, "\"subject\":{\"backend\":\"pipeline\"",
                           "execution profile event should identify snapshot backend") &&
           expect_contains(output, "\"effect\":\"observed\"",
                           "execution profile event should be observational") &&
           expect_contains(output, "\"evidence_ref\":{\"debug_json\":\"snapshot.profile\"}",
                           "execution profile event should point back to snapshot profile JSON") &&
           expect_contains(output,
                           "\"l1_data_cache\":{\"enabled\":false,\"line_size_bytes\":64,\"capacity_lines\":64,\"loads\":0,\"stores\":0,\"hits\":0,\"misses\":0,\"evictions\":0,\"bypasses\":0,\"write_through_stores\":0}",
                           "debug snapshot JSON should expose default-off L1D cache counters");
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
           expect_contains(output, "\"payload\":{\"total_retirements\":",
                           "pipeline snapshot JSON should expose execution profile event payload") &&
           expect_contains(output, "\"top_hot_path\":{",
                           "pipeline snapshot JSON should expose top hot path in profile event") &&
           expect_contains(output, "\"top_pc_cost\":{",
                           "pipeline snapshot JSON should expose top PC cost in profile event") &&
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
    write32(ram, kMachineExitVector + 0, kAddiA7Exit);
    write32(ram, kMachineExitVector + 4, kEcall);

    cpu.core().write_gpr(10, 1ULL << 39);
    cpu.core().set_privilege_mode(PrivilegeMode::Supervisor);
    cpu.csr().write(CSR_MTVEC, kMachineExitVector, cpu.core());
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
           expect(profile.total_traps == 2,
                  "faulting-memory profile smoke should record the load page fault and S-mode exit ecall traps") &&
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

bool test_functional_profile_exposes_pc_cost_and_branch_target_heat() {
    Ram ram;
    Bus bus(ram);
    CPU cpu;
    cpu_init(cpu, kEntry);

    write32(ram, kEntry + 0, kAuipcX10);
    write32(ram, kEntry + 4, kAddiX10Plus64);
    write32(ram, kEntry + 8, kLwX6FromX10);
    write32(ram, kEntry + 12, kJalX0Skip8);
    write32(ram, kEntry + 16, kAddiX1WrongPath);
    write32(ram, kEntry + 20, kLwX6FromX10Again);
    write32(ram, kEntry + 24, kAddiA7Exit);
    write32(ram, kEntry + 28, kEcall);
    ram.write_bytes(kDataAddr, std::array<uint8_t, 4>{1, 0, 0, 0}.data(), 4);

    FunctionalBackend backend(cpu, bus);
    for (int step = 0; step < 16 && !cpu.core().halted(); ++step) {
        backend.step();
    }

    const ExecutionProfileSnapshot profile = backend.debug_snapshot().profile;
    const ExecutionPcCostEntry* first_load = find_pc_cost(profile, kEntry + 8);
    const ExecutionPcCostEntry* second_load = find_pc_cost(profile, kEntry + 20);
    const ExecutionBranchTargetEntry* jump_target =
        find_branch_target(profile, kEntry + 12, kEntry + 20);

    return expect(cpu.core().halted(), "pc-cost profile smoke should halt via ecall") &&
           expect(first_load != nullptr,
                  "pc-cost profile smoke should expose the first load PC") &&
           expect(first_load->memory_observations == 1,
                  "pc-cost profile smoke should count first-load memory observations") &&
           expect(first_load->memory_reads == 1,
                  "pc-cost profile smoke should count first-load reads") &&
           expect(first_load->memory_writes == 0,
                  "pc-cost profile smoke should not count first-load writes") &&
           expect(first_load->memory_bytes == 4,
                  "pc-cost profile smoke should preserve first-load byte cost") &&
           expect(first_load->cycles == 1,
                  "pc-cost profile smoke should count functional cycle cost per retired PC") &&
           expect(second_load != nullptr,
                  "pc-cost profile smoke should expose the second load PC") &&
           expect(second_load->memory_observations == 1,
                  "pc-cost profile smoke should count second-load memory observations") &&
           expect(jump_target != nullptr,
                  "branch-target profile smoke should expose the actual jump target") &&
           expect(jump_target->executions == 1,
                  "branch-target profile smoke should count actual target heat") &&
           expect(jump_target->redirects == 1,
                  "branch-target profile smoke should count taken redirects");
}

bool test_functional_profile_counts_atomic_lr_sc_observations() {
    Ram ram;
    Bus bus(ram);
    CPU cpu;
    cpu_init(cpu, kEntry);

    write32(ram, kEntry + 0, encode_amo(0x02, false, false, 0, 10, 0x2, 5));
    write32(ram, kEntry + 4, encode_amo(0x03, false, false, 11, 10, 0x2, 6));
    write32(ram, kEntry + 8, kAddiA7Exit);
    write32(ram, kEntry + 12, kEcall);
    ram.store(kDataAddr, 0x11223344U, 4);

    cpu.core().write_gpr(10, kDataAddr);
    cpu.core().write_gpr(11, 0x55667788U);

    FunctionalBackend backend(cpu, bus);
    for (int step = 0; step < 16 && !cpu.core().halted(); ++step) {
        backend.step();
    }

    const ExecutionProfileSnapshot profile = backend.debug_snapshot().profile;
    const ExecutionMemoryRegionEntry* ram_region = find_memory_region(profile, "ram");

    return expect(cpu.core().halted(),
                  "functional atomic profile smoke should halt via ecall") &&
           expect(profile.total_memory_observations == 2,
                  "functional atomic profile smoke should count lr/sc as two memory observations") &&
           expect(ram_region != nullptr,
                  "functional atomic profile smoke should classify lr/sc observations as RAM") &&
           expect(ram_region->reads == 1,
                  "functional atomic profile smoke should count lr as a read observation") &&
           expect(ram_region->writes == 1,
                  "functional atomic profile smoke should count successful sc as a write observation") &&
           expect(ram_region->bytes == 8,
                  "functional atomic profile smoke should preserve the lr/sc access widths") &&
           expect(profile.shadow_cache.line_accesses == 2,
                  "functional atomic profile smoke should count both atomic accesses in shadow cache") &&
           expect(profile.shadow_cache.hits == 1,
                  "functional atomic profile smoke should record the reused-line hit") &&
           expect(profile.shadow_cache.misses == 1,
                  "functional atomic profile smoke should record the first-touch miss");
}

bool test_pipeline_profile_counts_atomic_lr_sc_observations() {
    Ram ram;
    Bus bus(ram);
    CPU cpu;
    cpu_init(cpu, kEntry);

    write32(ram, kEntry + 0, encode_amo(0x02, false, false, 0, 10, 0x2, 5));
    write32(ram, kEntry + 4, encode_amo(0x03, false, false, 11, 10, 0x2, 6));
    write32(ram, kEntry + 8, kAddiA7Exit);
    write32(ram, kEntry + 12, kEcall);
    ram.store(kDataAddr, 0x11223344U, 4);

    cpu.core().write_gpr(10, kDataAddr);
    cpu.core().write_gpr(11, 0x55667788U);

    PipelineBackend backend(cpu, bus);
    for (int step = 0; step < 32 && !cpu.core().halted(); ++step) {
        backend.step();
    }

    const ExecutionProfileSnapshot profile = backend.debug_snapshot().profile;
    const ExecutionMemoryRegionEntry* ram_region = find_memory_region(profile, "ram");

    return expect(cpu.core().halted(),
                  "pipeline atomic profile smoke should halt via ecall") &&
           expect(profile.total_memory_observations == 2,
                  "pipeline atomic profile smoke should count lr/sc as two memory observations") &&
           expect(ram_region != nullptr,
                  "pipeline atomic profile smoke should classify lr/sc observations as RAM") &&
           expect(ram_region->reads == 1,
                  "pipeline atomic profile smoke should count lr as a read observation") &&
           expect(ram_region->writes == 1,
                  "pipeline atomic profile smoke should count successful sc as a write observation") &&
           expect(ram_region->bytes == 8,
                  "pipeline atomic profile smoke should preserve the lr/sc access widths") &&
           expect(profile.shadow_cache.line_accesses == 2,
                  "pipeline atomic profile smoke should count both atomic accesses in shadow cache") &&
           expect(profile.shadow_cache.hits == 1,
                  "pipeline atomic profile smoke should record the reused-line hit") &&
           expect(profile.shadow_cache.misses == 1,
                  "pipeline atomic profile smoke should record the first-touch miss");
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
    if (!test_functional_profile_exposes_pc_cost_and_branch_target_heat()) {
        return 1;
    }
    if (!test_functional_profile_counts_atomic_lr_sc_observations()) {
        return 1;
    }
    if (!test_pipeline_profile_counts_atomic_lr_sc_observations()) {
        return 1;
    }
    std::puts("execution_profile_smoke: PASS");
    return 0;
}
