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

bool test_ir_eval_matches_reference_for_wider_integer_ir_v0() {
    const std::vector<uint32_t> program{
        addi(1, 0, -8),
        xori(2, 1, 0x7f),
        ori(3, 2, 0x30),
        andi(4, 3, 0x7f),
        slti(5, 1, -1),
        sltiu(6, 1, 1),
        addi(7, 0, 1),
        slli(8, 7, 5),
        srli(9, 8, 2),
        srai(10, 1, 2),
        bit_xor(11, 2, 4),
        bit_or(12, 11, 8),
        bit_and(13, 12, 3),
        slt(14, 1, 7),
        sltu(15, 1, 7),
        sll(16, 7, 5),
        srl(17, 8, 5),
        sra(18, 1, 5),
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

    return expect(plan.ok, "DBT block plan should accept wider integer IR v0 program") &&
           expect(unit.ok, "translator should accept wider integer IR v0 program") &&
           expect(evaluated.ok, "IR evaluator should accept wider integer IR v0 unit") &&
           expect(evaluated.retired_instructions == program.size(),
                  "IR evaluator should count wider integer IR v0 instructions") &&
           expect(evaluated.next_pc == ref_cpu.core().pc(),
                  "wider IR evaluator next PC should match reference PC") &&
           expect_gprs_equal(evaluated.gpr, ref_cpu);
}

bool test_ir_eval_matches_reference_for_u_type_and_word_ir_v0() {
    const std::vector<uint32_t> program{
        lui(1, 0x12345000),
        auipc(2, 0x00012000),
        addiw(3, 1, -1),
        addiw(4, 0, -1),
        slliw(5, 4, 4),
        srliw(6, 5, 1),
        sraiw(7, 5, 2),
        addi(8, 0, 3),
        addw(9, 5, 8),
        subw(10, 5, 8),
        sllw(11, 8, 8),
        srlw(12, 5, 8),
        sraw(13, 5, 8),
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

    return expect(plan.ok, "DBT block plan should accept U-type and word IR v0 program") &&
           expect(unit.ok, "translator should accept U-type and word IR v0 program") &&
           expect(evaluated.ok, "IR evaluator should accept U-type and word IR v0 unit") &&
           expect(evaluated.retired_instructions == program.size(),
                  "IR evaluator should count U-type and word IR v0 instructions") &&
           expect(evaluated.next_pc == ref_cpu.core().pc(),
                  "U-type and word IR evaluator next PC should match reference PC") &&
           expect_gprs_equal(evaluated.gpr, ref_cpu);
}

bool test_ir_eval_rejects_non_ok_translation_unit() {
    DbtTranslationUnit unit;
    unit.ok = false;
    unit.reject_kind = DbtRejectKind::MemoryLoad;
    unit.reject_reason = "helper-required";

    CPU cpu;
    cpu_init(cpu, kEntry);
    const DbtIrEvaluationResult evaluated = evaluate_dbt_ir_unit(unit, make_ir_input(cpu));

    return expect(!evaluated.ok, "IR evaluator should reject failed translation units") &&
           expect(evaluated.reject_kind == DbtRejectKind::MemoryLoad,
                  "IR evaluator should preserve translation reject kind") &&
           expect(evaluated.reject_reason == "helper-required",
                  "IR evaluator should preserve translation reject reason");
}

}  // namespace

int main() {
    if (!test_ir_eval_matches_reference_for_integer_block()) {
        return 1;
    }
    if (!test_ir_eval_matches_reference_for_wider_integer_ir_v0()) {
        return 1;
    }
    if (!test_ir_eval_matches_reference_for_u_type_and_word_ir_v0()) {
        return 1;
    }
    if (!test_ir_eval_rejects_non_ok_translation_unit()) {
        return 1;
    }
    std::puts("dbt_ir_eval_smoke: PASS");
    return 0;
}
