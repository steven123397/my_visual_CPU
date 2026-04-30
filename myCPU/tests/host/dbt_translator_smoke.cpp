#include <cstdint>
#include <cstdio>

#include "../../src/cpu.h"
#include "../../src/exec/dbt_block_plan.h"
#include "../../src/exec/dbt_translator.h"
#include "../../src/mem/bus.h"
#include "../../src/mem/ram.h"

namespace {

constexpr uint64_t kEntry = MEM_BASE;
constexpr uint32_t kAddiX1One = 0x00100093U;    // addi x1, x0, 1
constexpr uint32_t kAddiX2X1Two = 0x00208113U;  // addi x2, x1, 2
constexpr uint32_t kAddX3X1X2 = 0x002081b3U;    // add x3, x1, x2
constexpr uint32_t kSubX4X3X1 = 0x40118233U;    // sub x4, x3, x1
constexpr uint32_t kLwX1FromX0 = 0x00002083U;   // lw x1, 0(x0)
constexpr uint32_t kXoriX2X1Seven = 0x0070c113U;  // xori x2, x1, 7

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

bool test_translates_inlineable_integer_block_to_ir_v0() {
    Ram ram;
    Bus bus(ram);
    CPU cpu;
    cpu_init(cpu, kEntry);
    write32(ram, kEntry + 0, kAddiX1One);
    write32(ram, kEntry + 4, kAddiX2X1Two);
    write32(ram, kEntry + 8, kAddX3X1X2);
    write32(ram, kEntry + 12, kSubX4X3X1);

    const DbtBlockPlan plan = plan_dbt_block(cpu, bus, kEntry, kEntry + 12);
    const DbtTranslationUnit unit = translate_dbt_block(plan);

    return expect(plan.ok, "translator smoke setup should build inlineable DBT block plan") &&
           expect(unit.ok, "translator should accept inlineable integer block") &&
           expect(unit.start_pc == kEntry,
                  "translation unit should preserve block start PC") &&
           expect(unit.end_pc == kEntry + 12,
                  "translation unit should preserve block end PC") &&
           expect(unit.instructions.size() == 5,
                  "translation unit should contain four IR ops plus fallthrough") &&
           expect(unit.instructions[0].opcode == DbtIrOpcode::WriteRegImm,
                  "addi from x0 should translate to WriteRegImm") &&
           expect(unit.instructions[0].rd == 1 && unit.instructions[0].imm == 1,
                  "WriteRegImm should preserve rd and immediate") &&
           expect(unit.instructions[1].opcode == DbtIrOpcode::AddRegImm,
                  "addi from nonzero rs1 should translate to AddRegImm") &&
           expect(unit.instructions[1].rd == 2 && unit.instructions[1].rs1 == 1 &&
                      unit.instructions[1].imm == 2,
                  "AddRegImm should preserve rd, rs1, and immediate") &&
           expect(unit.instructions[2].opcode == DbtIrOpcode::AddRegReg,
                  "add should translate to AddRegReg") &&
           expect(unit.instructions[2].rd == 3 && unit.instructions[2].rs1 == 1 &&
                      unit.instructions[2].rs2 == 2,
                  "AddRegReg should preserve rd, rs1, and rs2") &&
           expect(unit.instructions[3].opcode == DbtIrOpcode::SubRegReg,
                  "sub should translate to SubRegReg") &&
           expect(unit.instructions[3].rd == 4 && unit.instructions[3].rs1 == 3 &&
                      unit.instructions[3].rs2 == 1,
                  "SubRegReg should preserve rd, rs1, and rs2") &&
           expect(unit.instructions[4].opcode == DbtIrOpcode::Fallthrough,
                  "translation unit should end with explicit fallthrough") &&
           expect(unit.instructions[4].next_pc == kEntry + 16,
                  "fallthrough should point to the next sequential PC") &&
           expect(cpu.core().pc() == kEntry,
                  "translation should not advance architectural PC") &&
           expect(cpu.core().instret() == 0,
                  "translation should not advance instret");
}

bool test_rejects_non_inlineable_plan_without_prefix_ir() {
    Ram ram;
    Bus bus(ram);
    CPU cpu;
    cpu_init(cpu, kEntry);
    write32(ram, kEntry + 0, kAddiX1One);
    write32(ram, kEntry + 4, kLwX1FromX0);

    const DbtBlockPlan plan = plan_dbt_block(cpu, bus, kEntry, kEntry + 4);
    const DbtTranslationUnit unit = translate_dbt_block(plan);

    return expect(!plan.ok, "translator reject setup should build rejected DBT block plan") &&
           expect(!unit.ok, "translator should reject non-inlineable block plan") &&
           expect(unit.reject_reason == "helper-required",
                  "translator should preserve block-plan reject reason") &&
           expect(unit.boundary == DbtBoundaryKind::MemoryLoad,
                  "translator should preserve typed reject boundary") &&
           expect(unit.boundary_kind == "memory-load",
                  "translator should preserve string reject boundary") &&
           expect(unit.instructions.empty(),
                  "translator should not emit prefix IR for rejected block plan") &&
           expect(cpu.core().pc() == kEntry,
                  "rejected translation should not advance architectural PC") &&
           expect(cpu.core().instret() == 0,
                  "rejected translation should not advance instret");
}

bool test_rejects_unsupported_ir_v0_without_prefix_ir() {
    Ram ram;
    Bus bus(ram);
    CPU cpu;
    cpu_init(cpu, kEntry);
    write32(ram, kEntry + 0, kAddiX1One);
    write32(ram, kEntry + 4, kXoriX2X1Seven);

    const DbtBlockPlan plan = plan_dbt_block(cpu, bus, kEntry, kEntry + 4);
    const DbtTranslationUnit unit = translate_dbt_block(plan);

    return expect(plan.ok, "unsupported IR setup should still build an inlineable DBT block plan") &&
           expect(!unit.ok, "translator should reject inlineable ops outside IR v0") &&
           expect(unit.reject_reason == "unsupported-ir",
                  "translator should report unsupported IR v0 opcode") &&
           expect(unit.boundary == DbtBoundaryKind::Unsupported,
                  "translator should expose unsupported IR typed boundary") &&
           expect(unit.boundary_kind == "unsupported-ir",
                  "translator should expose unsupported IR string boundary") &&
           expect(unit.instructions.empty(),
                  "translator should not emit prefix IR for unsupported IR v0 blocks") &&
           expect(cpu.core().pc() == kEntry,
                  "unsupported translation should not advance architectural PC") &&
           expect(cpu.core().instret() == 0,
                  "unsupported translation should not advance instret");
}

}  // namespace

int main() {
    if (!test_translates_inlineable_integer_block_to_ir_v0()) {
        return 1;
    }
    if (!test_rejects_non_inlineable_plan_without_prefix_ir()) {
        return 1;
    }
    if (!test_rejects_unsupported_ir_v0_without_prefix_ir()) {
        return 1;
    }
    std::puts("dbt_translator_smoke: PASS");
    return 0;
}
