#include <cstdint>
#include <cstdio>
#include <string>
#include <vector>

#include "../../src/cpu.h"
#include "../../src/exec/dbt_runtime_harness.h"
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

constexpr uint32_t add(uint8_t rd, uint8_t rs1, uint8_t rs2) {
    return encode_r(0x00, rs2, rs1, 0, rd, 0x33);
}

constexpr uint32_t sub(uint8_t rd, uint8_t rs1, uint8_t rs2) {
    return encode_r(0x20, rs2, rs1, 0, rd, 0x33);
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
    if (!test_default_machine_backend_does_not_enable_jit()) {
        return 1;
    }
    std::puts("dbt_runtime_harness_smoke: PASS");
    return 0;
}
