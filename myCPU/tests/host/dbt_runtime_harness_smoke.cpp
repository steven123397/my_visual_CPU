#include <cstdint>
#include <cstdio>
#include <string>
#include <vector>

#include "../../src/cpu.h"
#include "../../src/exec/dbt_executable_cache.h"
#include "../../src/exec/dbt_runtime_harness.h"
#include "../../src/exec/dbt_runtime_invalidation.h"
#include "../../src/mem/bus.h"
#include "../../src/mem/ram.h"
#include "../../src/platform/machine.h"

namespace {

constexpr uint64_t kEntry = MEM_BASE;

bool expect(bool condition, const char* message) {
    if (!condition) {
        std::fprintf(stderr, "%s\n", message);
        return false;
    }
    return true;
}

constexpr uint32_t encode_i(uint32_t imm,
                            uint8_t rs1,
                            uint8_t funct3,
                            uint8_t rd,
                            uint8_t opcode) {
    return ((imm & 0xfffU) << 20) |
           (static_cast<uint32_t>(rs1) << 15) |
           (static_cast<uint32_t>(funct3) << 12) |
           (static_cast<uint32_t>(rd) << 7) |
           static_cast<uint32_t>(opcode);
}

constexpr uint32_t encode_r(uint8_t funct7,
                            uint8_t rs2,
                            uint8_t rs1,
                            uint8_t funct3,
                            uint8_t rd,
                            uint8_t opcode) {
    return (static_cast<uint32_t>(funct7) << 25) |
           (static_cast<uint32_t>(rs2) << 20) |
           (static_cast<uint32_t>(rs1) << 15) |
           (static_cast<uint32_t>(funct3) << 12) |
           (static_cast<uint32_t>(rd) << 7) |
           static_cast<uint32_t>(opcode);
}

constexpr uint32_t addi(uint8_t rd, uint8_t rs1, int32_t imm) {
    return encode_i(static_cast<uint32_t>(imm), rs1, 0, rd, 0x13);
}

constexpr uint32_t slti(uint8_t rd, uint8_t rs1, int32_t imm) {
    return encode_i(static_cast<uint32_t>(imm), rs1, 2, rd, 0x13);
}

constexpr uint32_t sltiu(uint8_t rd, uint8_t rs1, int32_t imm) {
    return encode_i(static_cast<uint32_t>(imm), rs1, 3, rd, 0x13);
}

constexpr uint32_t xori(uint8_t rd, uint8_t rs1, int32_t imm) {
    return encode_i(static_cast<uint32_t>(imm), rs1, 4, rd, 0x13);
}

constexpr uint32_t ori(uint8_t rd, uint8_t rs1, int32_t imm) {
    return encode_i(static_cast<uint32_t>(imm), rs1, 6, rd, 0x13);
}

constexpr uint32_t andi(uint8_t rd, uint8_t rs1, int32_t imm) {
    return encode_i(static_cast<uint32_t>(imm), rs1, 7, rd, 0x13);
}

constexpr uint32_t slli(uint8_t rd, uint8_t rs1, uint8_t shamt) {
    return encode_i(shamt, rs1, 1, rd, 0x13);
}

constexpr uint32_t srli(uint8_t rd, uint8_t rs1, uint8_t shamt) {
    return encode_i(shamt, rs1, 5, rd, 0x13);
}

constexpr uint32_t srai(uint8_t rd, uint8_t rs1, uint8_t shamt) {
    return encode_i((0x20U << 5) | shamt, rs1, 5, rd, 0x13);
}

constexpr uint32_t addiw(uint8_t rd, uint8_t rs1, int32_t imm) {
    return encode_i(static_cast<uint32_t>(imm), rs1, 0, rd, 0x1B);
}

constexpr uint32_t slliw(uint8_t rd, uint8_t rs1, uint8_t shamt) {
    return encode_i(shamt, rs1, 1, rd, 0x1B);
}

constexpr uint32_t srliw(uint8_t rd, uint8_t rs1, uint8_t shamt) {
    return encode_i(shamt, rs1, 5, rd, 0x1B);
}

constexpr uint32_t sraiw(uint8_t rd, uint8_t rs1, uint8_t shamt) {
    return encode_i((0x20U << 5) | shamt, rs1, 5, rd, 0x1B);
}

constexpr uint32_t encode_u(uint32_t imm, uint8_t rd, uint8_t opcode) {
    return (imm & 0xfffff000U) |
           (static_cast<uint32_t>(rd) << 7) |
           static_cast<uint32_t>(opcode);
}

constexpr uint32_t lui(uint8_t rd, uint32_t imm) {
    return encode_u(imm, rd, 0x37);
}

constexpr uint32_t auipc(uint8_t rd, uint32_t imm) {
    return encode_u(imm, rd, 0x17);
}

constexpr uint32_t add(uint8_t rd, uint8_t rs1, uint8_t rs2) {
    return encode_r(0x00, rs2, rs1, 0, rd, 0x33);
}

constexpr uint32_t sub(uint8_t rd, uint8_t rs1, uint8_t rs2) {
    return encode_r(0x20, rs2, rs1, 0, rd, 0x33);
}

constexpr uint32_t sll(uint8_t rd, uint8_t rs1, uint8_t rs2) {
    return encode_r(0x00, rs2, rs1, 1, rd, 0x33);
}

constexpr uint32_t slt(uint8_t rd, uint8_t rs1, uint8_t rs2) {
    return encode_r(0x00, rs2, rs1, 2, rd, 0x33);
}

constexpr uint32_t sltu(uint8_t rd, uint8_t rs1, uint8_t rs2) {
    return encode_r(0x00, rs2, rs1, 3, rd, 0x33);
}

constexpr uint32_t bit_xor(uint8_t rd, uint8_t rs1, uint8_t rs2) {
    return encode_r(0x00, rs2, rs1, 4, rd, 0x33);
}

constexpr uint32_t srl(uint8_t rd, uint8_t rs1, uint8_t rs2) {
    return encode_r(0x00, rs2, rs1, 5, rd, 0x33);
}

constexpr uint32_t sra(uint8_t rd, uint8_t rs1, uint8_t rs2) {
    return encode_r(0x20, rs2, rs1, 5, rd, 0x33);
}

constexpr uint32_t bit_or(uint8_t rd, uint8_t rs1, uint8_t rs2) {
    return encode_r(0x00, rs2, rs1, 6, rd, 0x33);
}

constexpr uint32_t bit_and(uint8_t rd, uint8_t rs1, uint8_t rs2) {
    return encode_r(0x00, rs2, rs1, 7, rd, 0x33);
}

constexpr uint32_t addw(uint8_t rd, uint8_t rs1, uint8_t rs2) {
    return encode_r(0x00, rs2, rs1, 0, rd, 0x3B);
}

constexpr uint32_t subw(uint8_t rd, uint8_t rs1, uint8_t rs2) {
    return encode_r(0x20, rs2, rs1, 0, rd, 0x3B);
}

constexpr uint32_t sllw(uint8_t rd, uint8_t rs1, uint8_t rs2) {
    return encode_r(0x00, rs2, rs1, 1, rd, 0x3B);
}

constexpr uint32_t srlw(uint8_t rd, uint8_t rs1, uint8_t rs2) {
    return encode_r(0x00, rs2, rs1, 5, rd, 0x3B);
}

constexpr uint32_t sraw(uint8_t rd, uint8_t rs1, uint8_t rs2) {
    return encode_r(0x20, rs2, rs1, 5, rd, 0x3B);
}

void write_program(Ram& ram, const std::vector<uint32_t>& program) {
    for (size_t i = 0; i < program.size(); ++i) {
        const uint64_t addr = kEntry + static_cast<uint64_t>(i * sizeof(uint32_t));
        const uint32_t raw = program[i];
        ram.write_bytes(addr, &raw, sizeof(raw));
    }
}

bool expect_cpu_gprs_equal(const CPU& lhs, const CPU& rhs) {
    for (uint32_t i = 0; i < 32; ++i) {
        if (lhs.core().read_gpr(i) != rhs.core().read_gpr(i)) {
            std::fprintf(stderr,
                         "gpr mismatch at x%u: jit=%llu ref=%llu\n",
                         i,
                         static_cast<unsigned long long>(lhs.core().read_gpr(i)),
                         static_cast<unsigned long long>(rhs.core().read_gpr(i)));
            return false;
        }
    }
    return true;
}

bool test_opt_in_runtime_harness_executes_single_integer_block_with_reference_guardrail() {
    const std::vector<uint32_t> program{
        addi(1, 0, 9),
        addi(2, 1, -4),
        add(3, 1, 2),
        sub(4, 3, 1),
        addi(0, 4, 123),
    };

    Ram jit_ram;
    Bus jit_bus(jit_ram);
    CPU jit_cpu;
    cpu_init(jit_cpu, kEntry);
    write_program(jit_ram, program);

    Ram ref_ram;
    Bus ref_bus(ref_ram);
    CPU ref_cpu;
    cpu_init(ref_cpu, kEntry);
    write_program(ref_ram, program);
    for (size_t i = 0; i < program.size(); ++i) {
        cpu_step(ref_cpu, ref_bus);
    }

    const uint64_t block_end =
        kEntry + static_cast<uint64_t>((program.size() - 1) * sizeof(uint32_t));
    const DbtRuntimeHarnessResult result =
        run_dbt_runtime_harness_block(jit_cpu, jit_bus, kEntry, block_end);
    const std::string line = format_dbt_runtime_harness_result(result);

    return expect(result.ok, "opt-in runtime harness should execute pure integer block") &&
           expect(result.executed_host_code && result.used_executable_memory,
                  "opt-in runtime harness should use emitted host code and executable memory") &&
           expect(result.retired_instructions == program.size(),
                  "opt-in runtime harness should retire all block instructions") &&
           expect(result.next_pc == ref_cpu.core().pc() && jit_cpu.core().pc() == ref_cpu.core().pc(),
                  "opt-in runtime harness should commit fallthrough PC") &&
           expect(jit_cpu.core().instret() == ref_cpu.core().instret(),
                  "opt-in runtime harness should commit retired instruction count") &&
           expect_cpu_gprs_equal(jit_cpu, ref_cpu) &&
           expect(result.differential_checked && result.differential_matched,
                  "opt-in runtime harness should report differential guardrail success") &&
           expect(line.find("runtime-harness: ok=true") != std::string::npos,
                  "runtime harness formatter should expose stable prefix") &&
           expect(line.find("host-code=true exec-mem=true guest-exec=true") != std::string::npos,
                  "runtime harness formatter should expose execution flags");
}

bool test_opt_in_runtime_harness_rejects_helper_blocks_without_state_mutation() {
    constexpr uint32_t kLwX1FromX0 = 0x00002083U;
    Ram ram;
    Bus bus(ram);
    CPU cpu;
    cpu_init(cpu, kEntry);
    write_program(ram, {kLwX1FromX0});
    cpu.core().write_gpr(1, 99);
    const uint64_t before_x1 = cpu.core().read_gpr(1);
    const uint64_t before_pc = cpu.core().pc();

    const DbtRuntimeHarnessResult result =
        run_dbt_runtime_harness_block(cpu, bus, kEntry, kEntry);

    return expect(!result.ok, "helper block should not execute through runtime harness") &&
           expect(result.fallback_required,
                  "helper block should require reference fallback") &&
           expect(!result.executed_host_code && !result.mutated_cpu_state,
                  "rejected harness block should not execute code or mutate state") &&
           expect(result.reject_kind == DbtRejectKind::MemoryLoad,
                  "rejected harness block should preserve helper reject kind") &&
           expect(cpu.core().read_gpr(1) == before_x1 && cpu.core().pc() == before_pc &&
                      cpu.core().instret() == 0,
                  "helper rejection should leave CPU architectural state untouched");
}

bool test_opt_in_runtime_harness_reuses_runtime_executable_cache() {
    const std::vector<uint32_t> program{
        addi(1, 0, 3),
        addi(2, 1, 4),
    };

    Ram ram;
    Bus bus(ram);
    write_program(ram, program);

    CPU first_cpu;
    cpu_init(first_cpu, kEntry);
    CPU second_cpu;
    cpu_init(second_cpu, kEntry);

    DbtExecutableCacheRuntime cache;
    const uint64_t block_end =
        kEntry + static_cast<uint64_t>((program.size() - 1) * sizeof(uint32_t));

    const DbtRuntimeHarnessResult first =
        run_dbt_runtime_harness_block_with_cache(first_cpu, bus, cache, kEntry, block_end);
    const DbtExecutableCacheDryRunStats after_first = cache.stats();
    const DbtRuntimeHarnessResult second =
        run_dbt_runtime_harness_block_with_cache(second_cpu, bus, cache, kEntry, block_end);
    const DbtExecutableCacheDryRunStats after_second = cache.stats();

    return expect(first.ok && !first.used_executable_cache &&
                      first.inserted_executable_cache,
                  "first opt-in runtime cache execution should emit and cache host code") &&
           expect(second.ok && second.used_executable_cache &&
                      !second.inserted_executable_cache,
                  "second opt-in runtime cache execution should reuse resident host code") &&
           expect(first.executed_host_code && second.executed_host_code &&
                      first.used_executable_memory && second.used_executable_memory,
                  "runtime cache harness should execute host code through executable memory") &&
           expect(first_cpu.core().read_gpr(2) == 7 &&
                      second_cpu.core().read_gpr(2) == 7 &&
                      first_cpu.core().pc() == second_cpu.core().pc(),
                  "runtime cache harness should commit the same architected result") &&
           expect(after_first.host_executables_inserted == 1 &&
                      after_first.host_executables_released == 0,
                  "first runtime cache execution should make one host executable resident") &&
           expect(after_second.host_executables_inserted == 1 &&
                      after_second.host_executables_released == 0 &&
                      after_second.hits == 1 && after_second.misses == 1,
                  "runtime cache reuse should hit without emitting another resident executable");
}

bool test_opt_in_runtime_harness_executes_expanded_pure_integer_matrix() {
    const std::vector<uint32_t> program{
        lui(1, 0x12345000),
        auipc(2, 0x00012000),
        addi(3, 0, -8),
        xori(4, 3, 0x7f),
        ori(5, 4, 0x30),
        andi(6, 5, 0x7f),
        slti(7, 3, -1),
        sltiu(8, 3, 1),
        slli(9, 7, 5),
        srli(10, 9, 2),
        srai(11, 3, 2),
        bit_xor(12, 4, 6),
        bit_or(13, 12, 9),
        bit_and(14, 13, 5),
        slt(15, 3, 7),
        sltu(16, 3, 7),
        sll(17, 7, 6),
        srl(18, 9, 7),
        sra(19, 3, 7),
        addiw(20, 1, -1),
        addiw(21, 0, -1),
        slliw(22, 21, 4),
        srliw(23, 22, 1),
        sraiw(24, 22, 2),
        addw(25, 22, 7),
        subw(26, 22, 7),
        sllw(27, 7, 7),
        srlw(28, 22, 7),
        sraw(29, 22, 7),
    };

    Ram jit_ram;
    Bus jit_bus(jit_ram);
    CPU jit_cpu;
    cpu_init(jit_cpu, kEntry);
    write_program(jit_ram, program);

    Ram ref_ram;
    Bus ref_bus(ref_ram);
    CPU ref_cpu;
    cpu_init(ref_cpu, kEntry);
    write_program(ref_ram, program);
    for (size_t i = 0; i < program.size(); ++i) {
        cpu_step(ref_cpu, ref_bus);
    }

    const uint64_t block_end =
        kEntry + static_cast<uint64_t>((program.size() - 1) * sizeof(uint32_t));
    const DbtRuntimeHarnessResult result =
        run_dbt_runtime_harness_block(jit_cpu, jit_bus, kEntry, block_end);

    return expect(result.ok, "runtime harness should execute expanded pure integer matrix") &&
           expect(result.executed_host_code && result.used_executable_memory,
                  "expanded pure integer matrix should use executable host code") &&
           expect(result.retired_instructions == program.size(),
                  "expanded pure integer matrix should retire all instructions") &&
           expect(result.next_pc == ref_cpu.core().pc() &&
                      jit_cpu.core().pc() == ref_cpu.core().pc(),
                  "expanded pure integer matrix should commit reference PC") &&
           expect_cpu_gprs_equal(jit_cpu, ref_cpu);
}

bool test_dispatch_harness_v1_emits_hits_and_rejects_stale_after_invalidation() {
    Ram ram;
    Bus bus(ram);
    DbtExecutableCacheRuntime cache;
    const uint64_t block_end = kEntry;

    write_program(ram, {addi(1, 0, 1)});
    CPU first_cpu;
    cpu_init(first_cpu, kEntry);
    const DbtRuntimeHarnessResult first =
        run_dbt_runtime_harness_block_with_cache(first_cpu, bus, cache, kEntry, block_end);

    CPU hit_cpu;
    cpu_init(hit_cpu, kEntry);
    const DbtRuntimeHarnessResult hit =
        run_dbt_runtime_harness_block_with_cache(hit_cpu, bus, cache, kEntry, block_end);

    const DbtRuntimeInvalidationHookResult invalidation =
        apply_dbt_runtime_invalidation_hook(cache, DbtRuntimeInvalidationEvent{
                                                       .kind = DbtInvalidationEventKind::GuestStore,
                                                       .addr = kEntry,
                                                       .size = 4,
                                                   });
    write_program(ram, {addi(1, 0, 5)});
    CPU after_invalidation_cpu;
    cpu_init(after_invalidation_cpu, kEntry);
    const DbtRuntimeHarnessResult after_invalidation =
        run_dbt_runtime_harness_block_with_cache(after_invalidation_cpu,
                                                 bus,
                                                 cache,
                                                 kEntry,
                                                 block_end);
    const DbtExecutableCacheDryRunStats stats = cache.stats();

    return expect(first.ok && first.cache_lookup && first.cache_miss &&
                      first.emitted_on_miss && !first.executed_on_hit,
                  "dispatch harness v1 should lookup and emit host code on cache miss") &&
           expect(hit.ok && hit.cache_lookup && hit.cache_hit &&
                      hit.executed_on_hit && !hit.emitted_on_miss,
                  "dispatch harness v1 should execute resident host code on cache hit") &&
           expect(invalidation.ok && invalidation.stale_dispatch_prevented,
                  "runtime invalidation should explicitly prevent stale dispatch") &&
           expect(after_invalidation.ok && after_invalidation.cache_lookup &&
                      after_invalidation.cache_miss &&
                      after_invalidation.emitted_on_miss &&
                      !after_invalidation.executed_on_hit,
                  "dispatch harness v1 should miss and re-emit after invalidation") &&
           expect(first_cpu.core().read_gpr(1) == 1 &&
                      hit_cpu.core().read_gpr(1) == 1 &&
                      after_invalidation_cpu.core().read_gpr(1) == 5,
                  "dispatch harness v1 should execute the post-invalidation guest code, not stale code") &&
           expect(stats.host_executables_released >= 1 &&
                      stats.stale_dispatches_prevented >= 1,
                  "runtime cache stats should record stale executable release");
}

bool test_runtime_harness_summary_stats_expose_opt_in_executable_path() {
    Ram ram;
    Bus bus(ram);
    DbtExecutableCacheRuntime cache;
    DbtRuntimeHarnessStats stats;

    write_program(ram, {addi(1, 0, 1)});
    CPU miss_cpu;
    cpu_init(miss_cpu, kEntry);
    const DbtRuntimeHarnessResult miss =
        run_dbt_runtime_harness_block_with_cache(miss_cpu, bus, cache, kEntry, kEntry);
    record_dbt_runtime_harness_result(stats, miss);

    CPU hit_cpu;
    cpu_init(hit_cpu, kEntry);
    const DbtRuntimeHarnessResult hit =
        run_dbt_runtime_harness_block_with_cache(hit_cpu, bus, cache, kEntry, kEntry);
    record_dbt_runtime_harness_result(stats, hit);

    write_program(ram, {addi(1, 0, 5)});
    CPU mismatch_cpu;
    cpu_init(mismatch_cpu, kEntry);
    const DbtRuntimeHarnessResult mismatch =
        run_dbt_runtime_harness_block_with_cache(mismatch_cpu, bus, cache, kEntry, kEntry);
    record_dbt_runtime_harness_result(stats, mismatch);

    const DbtRuntimeInvalidationHookResult invalidation =
        apply_dbt_runtime_invalidation_hook(cache, DbtRuntimeInvalidationEvent{
                                                       .kind = DbtInvalidationEventKind::GuestStore,
                                                       .addr = kEntry,
                                                       .size = 4,
                                                   });
    record_dbt_runtime_invalidation_result(stats, invalidation);

    write_program(ram, {0x00002083U});  // lw x1, 0(x0)
    CPU fallback_cpu;
    cpu_init(fallback_cpu, kEntry);
    const DbtRuntimeHarnessResult fallback =
        run_dbt_runtime_harness_block(fallback_cpu, bus, kEntry, kEntry);
    record_dbt_runtime_harness_result(stats, fallback);

    const std::string line = format_dbt_runtime_harness_stats(stats);

    return expect(miss.ok && miss.emitted_on_miss,
                  "stats setup should include cache miss emission") &&
           expect(hit.ok && hit.executed_on_hit,
                  "stats setup should include cache hit execution") &&
           expect(!mismatch.ok && mismatch.differential_checked &&
                      !mismatch.differential_matched,
                  "stats setup should include differential mismatch") &&
           expect(invalidation.ok && invalidation.stale_dispatch_prevented,
                  "stats setup should include stale dispatch invalidation") &&
           expect(!fallback.ok && fallback.fallback_required,
                  "stats setup should include fallback-required result") &&
           expect(stats.dispatches == 4 &&
                      stats.cache_lookups == 3 &&
                      stats.cache_hits == 2 &&
                      stats.cache_misses == 1,
                  "runtime stats should count dispatch lookup hit/miss") &&
           expect(stats.host_emits == 1 &&
                      stats.host_executes == 3 &&
                      stats.fallbacks == 2 &&
                      stats.invalidations == 1 &&
                      stats.stale_dispatches_prevented == 1 &&
                      stats.differential_mismatches == 1,
                  "runtime stats should count emit/exec/fallback/invalidate/mismatch") &&
           expect(line.find("runtime-harness-stats:") != std::string::npos,
                  "runtime stats formatter should expose stable prefix") &&
           expect(line.find("hits=2 misses=1 emits=1 exec=3 fallback=2 invalidate=1") !=
                      std::string::npos,
                  "runtime stats formatter should expose core counters") &&
           expect(line.find("differential-mismatch=1") != std::string::npos,
                  "runtime stats formatter should expose mismatch counter");
}

bool test_default_machine_backend_does_not_enable_jit() {
    const Machine machine;

    return expect(machine.backend_kind() == BackendKind::Functional,
                  "default machine backend should remain functional") &&
           expect(dbt_runtime_harness_is_default_enabled() == false,
                  "DBT runtime harness should remain opt-in and not default-enabled");
}

}  // namespace

int main() {
    if (!test_opt_in_runtime_harness_executes_single_integer_block_with_reference_guardrail()) {
        return 1;
    }
    if (!test_opt_in_runtime_harness_rejects_helper_blocks_without_state_mutation()) {
        return 1;
    }
    if (!test_opt_in_runtime_harness_reuses_runtime_executable_cache()) {
        return 1;
    }
    if (!test_opt_in_runtime_harness_executes_expanded_pure_integer_matrix()) {
        return 1;
    }
    if (!test_dispatch_harness_v1_emits_hits_and_rejects_stale_after_invalidation()) {
        return 1;
    }
    if (!test_runtime_harness_summary_stats_expose_opt_in_executable_path()) {
        return 1;
    }
    if (!test_default_machine_backend_does_not_enable_jit()) {
        return 1;
    }
    std::puts("dbt_runtime_harness_smoke: PASS");
    return 0;
}
