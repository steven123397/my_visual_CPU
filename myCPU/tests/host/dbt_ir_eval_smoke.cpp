#include <array>
#include <cstdint>
#include <cstdio>
#include <vector>

#include "../../src/cpu.h"
#include "../../src/exec/dbt_block_plan.h"
#include "../../src/exec/dbt_ir_eval.h"
#include "../../src/exec/dbt_translator.h"
#include "../../src/mem/bus.h"
#include "../../src/mem/ram.h"

namespace {

constexpr uint64_t kEntry = MEM_BASE;

bool expect(bool condition, const char* message) {
    if (!condition) {
        std::fprintf(stderr, "%s\n", message);
        return false;
    }
    return true;
}

constexpr uint32_t encode_i(uint32_t imm, uint8_t rs1, uint8_t funct3, uint8_t rd, uint8_t opcode) {
    return ((imm & 0xfffU) << 20) |
           (static_cast<uint32_t>(rs1) << 15) |
           (static_cast<uint32_t>(funct3) << 12) |
           (static_cast<uint32_t>(rd) << 7) |
           static_cast<uint32_t>(opcode);
}

constexpr uint32_t encode_r(uint8_t funct7, uint8_t rs2, uint8_t rs1, uint8_t funct3, uint8_t rd, uint8_t opcode) {
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

std::array<uint64_t, 32> snapshot_gprs(const CPU& cpu) {
    std::array<uint64_t, 32> gprs{};
    for (uint32_t i = 0; i < gprs.size(); ++i) {
        gprs[i] = cpu.core().read_gpr(i);
    }
    return gprs;
}

DbtIrEvaluationInput make_ir_input(const CPU& cpu) {
    return DbtIrEvaluationInput{
        .gpr = snapshot_gprs(cpu),
        .pc = cpu.core().pc(),
    };
}

bool expect_gprs_equal(const std::array<uint64_t, 32>& lhs, const CPU& rhs) {
    for (uint32_t i = 0; i < lhs.size(); ++i) {
        if (lhs[i] != rhs.core().read_gpr(i)) {
            std::fprintf(stderr,
                         "gpr mismatch at x%u: ir=%llu ref=%llu\n",
                         i,
                         static_cast<unsigned long long>(lhs[i]),
                         static_cast<unsigned long long>(rhs.core().read_gpr(i)));
            return false;
        }
    }
    return true;
}

bool test_ir_eval_matches_reference_for_integer_block() {
    const std::vector<uint32_t> program{
        addi(1, 0, 5),       // x1 = 5
        addi(2, 1, -3),      // x2 = 2
        add(3, 1, 2),        // x3 = 7
        sub(4, 3, 1),        // x4 = 2
        addi(0, 4, 9),       // x0 write must be ignored
        addi(5, 0, -1),      // sign-extended immediate
        add(6, 5, 4),        // wraparound add
    };

    Ram plan_ram;
    Bus plan_bus(plan_ram);
    CPU plan_cpu;
    cpu_init(plan_cpu, kEntry);
    write_program(plan_ram, program);

    Ram ref_ram;
    Bus ref_bus(ref_ram);
    CPU ref_cpu;
    cpu_init(ref_cpu, kEntry);
    write_program(ref_ram, program);

    const uint64_t block_end = kEntry + static_cast<uint64_t>((program.size() - 1) * sizeof(uint32_t));
    const DbtBlockPlan plan = plan_dbt_block(plan_cpu, plan_bus, kEntry, block_end);
    const DbtTranslationUnit unit = translate_dbt_block(plan);
    const DbtIrEvaluationResult evaluated = evaluate_dbt_ir_unit(unit, make_ir_input(plan_cpu));

    for (size_t i = 0; i < program.size(); ++i) {
        cpu_step(ref_cpu, ref_bus);
    }

    return expect(plan.ok, "DBT block plan should accept pure integer program") &&
           expect(unit.ok, "translator should accept IR v0 integer program") &&
           expect(evaluated.ok, "IR evaluator should accept translated IR v0 unit") &&
           expect(evaluated.retired_instructions == program.size(),
                  "IR evaluator should report retired instruction count") &&
           expect(evaluated.next_pc == ref_cpu.core().pc(),
                  "IR evaluator next PC should match reference PC") &&
           expect(evaluated.next_pc == kEntry + static_cast<uint64_t>(program.size() * sizeof(uint32_t)),
                  "IR evaluator should fall through to the next sequential PC") &&
           expect(evaluated.gpr[0] == 0,
                  "IR evaluator should preserve hardwired x0") &&
           expect_gprs_equal(evaluated.gpr, ref_cpu) &&
           expect(plan_cpu.core().pc() == kEntry,
                  "IR evaluation should not mutate source CPU PC") &&
           expect(plan_cpu.core().instret() == 0,
                  "IR evaluation should not mutate source CPU instret");
}

bool test_ir_eval_rejects_non_ok_translation_unit() {
    DbtTranslationUnit unit;
    unit.ok = false;
    unit.reject_reason = "helper-required";

    CPU cpu;
    cpu_init(cpu, kEntry);
    const DbtIrEvaluationResult evaluated = evaluate_dbt_ir_unit(unit, make_ir_input(cpu));

    return expect(!evaluated.ok, "IR evaluator should reject failed translation units") &&
           expect(evaluated.reject_reason == "helper-required",
                  "IR evaluator should preserve translation reject reason");
}

}  // namespace

int main() {
    if (!test_ir_eval_matches_reference_for_integer_block()) {
        return 1;
    }
    if (!test_ir_eval_rejects_non_ok_translation_unit()) {
        return 1;
    }
    std::puts("dbt_ir_eval_smoke: PASS");
    return 0;
}
