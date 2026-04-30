#include <cstdint>
#include <cstdio>
#include <string>

#include "../../src/cpu.h"
#include "../../src/exec/dbt_block_plan.h"
#include "../../src/exec/dbt_ir.h"
#include "../../src/exec/dbt_ir_lowering.h"
#include "../../src/exec/dbt_translator.h"
#include "../../src/mem/bus.h"
#include "../../src/mem/ram.h"

namespace {

constexpr uint64_t kEntry = MEM_BASE;
constexpr uint32_t kLuiX1 = 0x123450b7U;       // lui x1, 0x12345
constexpr uint32_t kAddiX2X1Neg1 = 0xfff08113U;  // addi x2, x1, -1
constexpr uint32_t kAddX3X1X2 = 0x002081b3U;   // add x3, x1, x2
constexpr uint32_t kAddiwX4X3One = 0x0011821bU;  // addiw x4, x3, 1
constexpr uint32_t kLwX1FromX0 = 0x00002083U;  // lw x1, 0(x0)

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

DbtTranslationUnit translate_words(const uint32_t* words, std::size_t count, CPU* observed_cpu = nullptr) {
    Ram ram;
    Bus bus(ram);
    CPU local_cpu;
    CPU& cpu = observed_cpu != nullptr ? *observed_cpu : local_cpu;
    if (observed_cpu == nullptr) {
        cpu_init(cpu, kEntry);
    }
    for (std::size_t i = 0; i < count; ++i) {
        write32(ram, kEntry + i * 4U, words[i]);
    }
    return translate_dbt_block(plan_dbt_block(cpu, bus, kEntry, kEntry + (count - 1U) * 4U));
}

bool test_lowers_ir_v0_to_backend_neutral_ops() {
    const uint32_t words[] = {
        kLuiX1,
        kAddiX2X1Neg1,
        kAddX3X1X2,
        kAddiwX4X3One,
    };
    const DbtTranslationUnit unit = translate_words(words, 4);
    const DbtIrLoweringResult lowered = lower_dbt_ir_unit(unit);

    return expect(unit.ok, "source translation unit should be ok") &&
           expect(lowered.ok, "lowering should accept IR v0 unit") &&
           expect(lowered.start_pc == kEntry && lowered.end_pc == kEntry + 12,
                  "lowering should preserve unit range") &&
           expect(lowered.instructions.size() == 5,
                  "lowering should preserve four compute ops plus fallthrough") &&
           expect(lowered.instructions[0].opcode == DbtLoweredOpcode::Compute &&
                      lowered.instructions[0].alu == DbtLoweredAluOp::Move &&
                      lowered.instructions[0].rd == 1 &&
                      lowered.instructions[0].lhs_kind == DbtLoweredOperandKind::Immediate &&
                      lowered.instructions[0].width == DbtLoweredWidth::Xlen,
                  "lui should lower to immediate move compute op") &&
           expect(lowered.instructions[1].alu == DbtLoweredAluOp::Add &&
                      lowered.instructions[1].rd == 2 &&
                      lowered.instructions[1].rs1 == 1 &&
                      lowered.instructions[1].lhs_kind == DbtLoweredOperandKind::Gpr &&
                      lowered.instructions[1].rhs_kind == DbtLoweredOperandKind::Immediate &&
                      lowered.instructions[1].imm == -1,
                  "addi should lower to GPR plus immediate compute op") &&
           expect(lowered.instructions[2].alu == DbtLoweredAluOp::Add &&
                      lowered.instructions[2].rd == 3 &&
                      lowered.instructions[2].rs1 == 1 &&
                      lowered.instructions[2].rs2 == 2 &&
                      lowered.instructions[2].rhs_kind == DbtLoweredOperandKind::Gpr,
                  "add should lower to two-GPR compute op") &&
           expect(lowered.instructions[3].alu == DbtLoweredAluOp::Add &&
                      lowered.instructions[3].rd == 4 &&
                      lowered.instructions[3].width == DbtLoweredWidth::Word &&
                      lowered.instructions[3].sign_extend_word,
                  "addiw should lower to word compute op with sign extension") &&
           expect(lowered.instructions[4].opcode == DbtLoweredOpcode::Fallthrough &&
                      lowered.instructions[4].next_pc == kEntry + 16,
                  "fallthrough should lower as explicit block exit op");
}

bool test_lowering_rejects_non_ok_units_without_prefix_ops() {
    const uint32_t words[] = {kLwX1FromX0};
    const DbtTranslationUnit unit = translate_words(words, 1);
    const DbtIrLoweringResult lowered = lower_dbt_ir_unit(unit);

    return expect(!unit.ok, "memory source unit should be rejected before lowering") &&
           expect(!lowered.ok, "lowering should reject rejected translation unit") &&
           expect(lowered.reject_kind == unit.reject_kind,
                  "lowering should preserve reject kind from translation unit") &&
           expect(lowered.reject_pc == unit.reject_pc && lowered.reject_raw == unit.reject_raw,
                  "lowering should preserve reject pc and raw instruction") &&
           expect(lowered.instructions.empty(),
                  "lowering should not emit prefix ops for rejected units");
}

bool test_lowering_rejects_unknown_ir_without_prefix_ops() {
    DbtTranslationUnit unit{
        .ok = true,
        .start_pc = kEntry,
        .end_pc = kEntry + 8,
    };
    unit.instructions.push_back(DbtIrInstruction{
        .opcode = DbtIrOpcode::AddRegImm,
        .pc = kEntry,
        .raw = kAddiX2X1Neg1,
        .size = 4,
        .rd = 2,
        .rs1 = 1,
        .imm = -1,
        .next_pc = kEntry + 4,
    });
    unit.instructions.push_back(DbtIrInstruction{
        .opcode = static_cast<DbtIrOpcode>(0xffU),
        .pc = kEntry + 4,
        .raw = 0xffffffffU,
        .size = 4,
        .next_pc = kEntry + 8,
    });

    const DbtIrLoweringResult lowered = lower_dbt_ir_unit(unit);

    return expect(!lowered.ok, "lowering should reject unknown IR opcode") &&
           expect(lowered.reject_kind == DbtRejectKind::UnsupportedIr,
                  "unknown IR should reject as unsupported IR") &&
           expect(lowered.reject_pc == kEntry + 4 && lowered.reject_raw == 0xffffffffU,
                  "unknown IR reject should preserve offending op") &&
           expect(lowered.instructions.empty(),
                  "unknown IR reject should not expose prefix lowered ops");
}

bool test_lowering_is_dry_run_and_exposes_stable_names() {
    const uint32_t words[] = {kAddiX2X1Neg1};
    CPU cpu;
    cpu_init(cpu, kEntry);
    cpu.core().write_gpr(1, 41);
    const uint64_t before_x1 = cpu.core().read_gpr(1);
    const uint64_t before_x2 = cpu.core().read_gpr(2);
    const uint64_t before_pc = cpu.core().pc();

    const DbtTranslationUnit unit = translate_words(words, 1, &cpu);
    const DbtIrLoweringResult lowered = lower_dbt_ir_unit(unit);

    return expect(lowered.ok, "lowering single addi should succeed") &&
           expect(cpu.core().read_gpr(1) == before_x1 &&
                      cpu.core().read_gpr(2) == before_x2 &&
                      cpu.core().pc() == before_pc,
                  "lowering should not mutate CPU architectural state") &&
           expect(dbt_lowered_opcode_name(DbtLoweredOpcode::Compute) ==
                      std::string("compute"),
                  "lowered opcode name should expose compute") &&
           expect(dbt_lowered_alu_op_name(DbtLoweredAluOp::SetLessThanUnsigned) ==
                      std::string("set-less-than-unsigned"),
                  "lowered ALU op name should expose unsigned compare") &&
           expect(dbt_lowered_operand_kind_name(DbtLoweredOperandKind::Pc) ==
                      std::string("pc"),
                  "lowered operand kind name should expose pc") &&
           expect(dbt_lowered_width_name(DbtLoweredWidth::Word) == std::string("word"),
                  "lowered width name should expose word");
}

}  // namespace

int main() {
    if (!test_lowers_ir_v0_to_backend_neutral_ops()) {
        return 1;
    }
    if (!test_lowering_rejects_non_ok_units_without_prefix_ops()) {
        return 1;
    }
    if (!test_lowering_rejects_unknown_ir_without_prefix_ops()) {
        return 1;
    }
    if (!test_lowering_is_dry_run_and_exposes_stable_names()) {
        return 1;
    }
    std::puts("dbt_ir_lowering_smoke: PASS");
    return 0;
}
