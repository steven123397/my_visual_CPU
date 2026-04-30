#include <cstdint>
#include <cstdio>
#include <string>

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
constexpr uint32_t kDivX3X1X2 = 0x0220c1b3U;    // div x3, x1, x2
constexpr uint32_t kLwX1FromX0 = 0x00002083U;   // lw x1, 0(x0)
constexpr uint32_t kLuiX2One = 0x00001137U;     // lui x2, 0x1
constexpr uint32_t kJalX0Skip8 = 0x0080006fU;   // jal x0, 8
constexpr uint32_t kSfenceVma = 0x12000073U;    // sfence.vma x0, x0

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
           expect(unit.reject_kind == DbtRejectKind::MemoryLoad,
                  "translator should expose typed memory-load reject kind") &&
           expect(unit.reject_pc == kEntry + 4,
                  "translator should expose first helper reject PC") &&
           expect(unit.reject_raw == kLwX1FromX0,
                  "translator should expose first helper raw instruction") &&
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
    write32(ram, kEntry + 4, kLuiX2One);

    const DbtBlockPlan plan = plan_dbt_block(cpu, bus, kEntry, kEntry + 4);
    const DbtTranslationUnit unit = translate_dbt_block(plan);

    return expect(plan.ok, "unsupported IR setup should still build an inlineable DBT block plan") &&
           expect(!unit.ok, "translator should reject inlineable ops outside IR v0") &&
           expect(unit.reject_reason == "unsupported-ir",
                  "translator should report unsupported IR v0 opcode") &&
           expect(unit.reject_kind == DbtRejectKind::UnsupportedIr,
                  "translator should expose unsupported IR typed reject kind") &&
           expect(unit.reject_pc == kEntry + 4,
                  "translator should expose unsupported IR reject PC") &&
           expect(unit.reject_raw == kLuiX2One,
                  "translator should expose unsupported IR raw instruction") &&
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

bool test_rejects_supported_non_ir_v0_integer_ops_without_prefix_ir() {
    Ram ram;
    Bus bus(ram);
    CPU cpu;
    cpu_init(cpu, kEntry);
    write32(ram, kEntry + 0, kAddiX1One);
    write32(ram, kEntry + 4, kAddiX2X1Two);
    write32(ram, kEntry + 8, kDivX3X1X2);

    const DbtBlockPlan plan = plan_dbt_block(cpu, bus, kEntry, kEntry + 8);
    const DbtTranslationUnit unit = translate_dbt_block(plan);

    return expect(plan.ok, "DBT block plan should accept supported pure integer op before translator filter") &&
           expect(!unit.ok, "translator should reject supported integer ops outside IR v0") &&
           expect(unit.reject_kind == DbtRejectKind::UnsupportedIr,
                  "translator should classify supported non-IR-v0 integer op as unsupported IR") &&
           expect(unit.reject_pc == kEntry + 8,
                  "translator should expose supported non-IR-v0 reject PC") &&
           expect(unit.reject_raw == kDivX3X1X2,
                  "translator should expose supported non-IR-v0 raw instruction") &&
           expect(unit.instructions.empty(),
                  "translator should not emit prefix IR for supported non-IR-v0 rejects");
}

bool test_reject_taxonomy_classifies_fallback_boundaries() {
    Ram control_ram;
    Bus control_bus(control_ram);
    CPU control_cpu;
    cpu_init(control_cpu, kEntry);
    write32(control_ram, kEntry + 0, kAddiX1One);
    write32(control_ram, kEntry + 4, kJalX0Skip8);
    const DbtTranslationUnit control_unit =
        translate_dbt_block(plan_dbt_block(control_cpu, control_bus, kEntry, kEntry + 4));

    Ram tlb_ram;
    Bus tlb_bus(tlb_ram);
    CPU tlb_cpu;
    cpu_init(tlb_cpu, kEntry);
    write32(tlb_ram, kEntry + 0, kAddiX1One);
    write32(tlb_ram, kEntry + 4, kSfenceVma);
    const DbtTranslationUnit tlb_unit =
        translate_dbt_block(plan_dbt_block(tlb_cpu, tlb_bus, kEntry, kEntry + 4));

    return expect(!control_unit.ok,
                  "translator should reject control-flow block plans") &&
           expect(control_unit.reject_kind == DbtRejectKind::ControlFlow,
                  "translator should expose typed control-flow reject kind") &&
           expect(control_unit.reject_pc == kEntry + 4,
                  "translator should expose control-flow reject PC") &&
           expect(control_unit.reject_raw == kJalX0Skip8,
                  "translator should expose control-flow raw instruction") &&
           expect(control_unit.instructions.empty(),
                  "translator should not emit prefix IR for control-flow rejects") &&
           expect(!tlb_unit.ok,
                  "translator should reject TLB-flush block plans") &&
           expect(tlb_unit.reject_kind == DbtRejectKind::TlbFlush,
                  "translator should expose typed TLB-flush reject kind") &&
           expect(tlb_unit.reject_pc == kEntry + 4,
                  "translator should expose TLB-flush reject PC") &&
           expect(tlb_unit.reject_raw == kSfenceVma,
                  "translator should expose TLB-flush raw instruction") &&
           expect(tlb_unit.instructions.empty(),
                  "translator should not emit prefix IR for TLB-flush rejects");
}

bool test_reject_kind_names_are_stable() {
    return expect(dbt_reject_kind_name(DbtRejectKind::None) == std::string("none"),
                  "reject kind name should expose none") &&
           expect(dbt_reject_kind_name(DbtRejectKind::MemoryLoad) == std::string("memory-load"),
                  "reject kind name should expose memory-load") &&
           expect(dbt_reject_kind_name(DbtRejectKind::UnsupportedIr) == std::string("unsupported-ir"),
                  "reject kind name should expose unsupported-ir") &&
           expect(dbt_reject_kind_name(DbtRejectKind::TlbFlush) == std::string("tlb-flush"),
                  "reject kind name should expose tlb-flush");
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
    if (!test_rejects_supported_non_ir_v0_integer_ops_without_prefix_ir()) {
        return 1;
    }
    if (!test_reject_taxonomy_classifies_fallback_boundaries()) {
        return 1;
    }
    if (!test_reject_kind_names_are_stable()) {
        return 1;
    }
    std::puts("dbt_translator_smoke: PASS");
    return 0;
}
