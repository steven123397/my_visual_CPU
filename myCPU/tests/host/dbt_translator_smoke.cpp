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
constexpr uint32_t kAuipcX5One = 0x00001297U;   // auipc x5, 0x1
constexpr uint32_t kAddiwX6X2Minus1 = 0xfff1031bU;  // addiw x6, x2, -1
constexpr uint32_t kSllwX7X6X1 = 0x001313bbU;   // sllw x7, x6, x1
constexpr uint32_t kFence = 0x0000000fU;         // fence
constexpr uint32_t kJalX0Skip8 = 0x0080006fU;   // jal x0, 8
constexpr uint32_t kSfenceVma = 0x12000073U;    // sfence.vma x0, x0
constexpr uint64_t kHelperData = MEM_BASE + 0x100;

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
           0x2fU;
}

constexpr uint32_t encode_rtype(uint8_t opcode,
                                uint8_t funct3,
                                uint8_t funct7,
                                uint8_t rd,
                                uint8_t rs1,
                                uint8_t rs2) {
    return (static_cast<uint32_t>(funct7) << 25) |
           (static_cast<uint32_t>(rs2) << 20) |
           (static_cast<uint32_t>(rs1) << 15) |
           (static_cast<uint32_t>(funct3) << 12) |
           (static_cast<uint32_t>(rd) << 7) |
           static_cast<uint32_t>(opcode);
}

uint32_t encode_vdot(uint8_t vd, uint8_t vs1, uint8_t vs2) {
    return encode_rtype(0x57, 0, 0x22, vd, vs1, vs2);
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
           expect(unit.helper_plan.required,
                  "translator should preserve helper plan metadata") &&
           expect(unit.helper_plan.kind == DbtHelperKind::MemoryLoad,
                  "translator should preserve typed memory-load helper kind") &&
           expect(unit.helper_plan.pc == kEntry + 4 && unit.helper_plan.raw == kLwX1FromX0,
                  "translator should preserve helper PC and raw instruction") &&
           expect(unit.helper_plan.rd == 1 && unit.helper_plan.addr == 0 &&
                      unit.helper_plan.size == 4 && unit.helper_plan.sign_extend,
                  "translator should preserve memory-load helper details") &&
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
    write32(ram, kEntry + 4, kFence);

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
           expect(unit.reject_raw == kFence,
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

bool test_translates_u_type_and_word_ops_to_ir_v0() {
    Ram ram;
    Bus bus(ram);
    CPU cpu;
    cpu_init(cpu, kEntry);
    write32(ram, kEntry + 0, kLuiX2One);
    write32(ram, kEntry + 4, kAuipcX5One);
    write32(ram, kEntry + 8, kAddiwX6X2Minus1);
    write32(ram, kEntry + 12, kSllwX7X6X1);

    const DbtBlockPlan plan = plan_dbt_block(cpu, bus, kEntry, kEntry + 12);
    const DbtTranslationUnit unit = translate_dbt_block(plan);

    return expect(plan.ok, "translator U-type setup should build inlineable DBT block plan") &&
           expect(unit.ok, "translator should accept U-type and word ops") &&
           expect(unit.instructions.size() == 5,
                  "U-type and word translation should include four ops plus fallthrough") &&
           expect(unit.instructions[0].opcode == DbtIrOpcode::WriteRegImm,
                  "lui should translate to immediate register write") &&
           expect(unit.instructions[0].rd == 2 && unit.instructions[0].imm == 0x1000,
                  "lui should preserve rd and decoded upper immediate") &&
           expect(unit.instructions[1].opcode == DbtIrOpcode::AddPcImm,
                  "auipc should translate to PC-relative immediate write") &&
           expect(unit.instructions[1].rd == 5 && unit.instructions[1].imm == 0x1000,
                  "auipc should preserve rd and decoded upper immediate") &&
           expect(unit.instructions[2].opcode == DbtIrOpcode::AddRegImmWord,
                  "addiw should translate to word immediate add") &&
           expect(unit.instructions[3].opcode == DbtIrOpcode::ShiftLeftRegRegWord,
                  "sllw should translate to word register shift") &&
           expect(unit.instructions[4].opcode == DbtIrOpcode::Fallthrough,
                  "U-type and word translation should end with fallthrough");
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

bool test_preserves_atomic_and_vector_helper_metadata() {
    const uint32_t kAmoAddW =
        encode_amo(0x00, true, false, 6, 10, 0x2, 5);  // amoadd.w.aq x5, x6, (x10)
    Ram atomic_ram;
    Bus atomic_bus(atomic_ram);
    CPU atomic_cpu;
    cpu_init(atomic_cpu, kEntry);
    atomic_cpu.core().write_gpr(10, kHelperData);
    atomic_cpu.core().write_gpr(6, 0x12345678U);
    write32(atomic_ram, kEntry + 0, kAmoAddW);
    const DbtTranslationUnit atomic_unit =
        translate_dbt_block(plan_dbt_block(atomic_cpu, atomic_bus, kEntry, kEntry));

    const uint32_t kVdot = encode_vdot(5, 1, 2);
    Ram vector_ram;
    Bus vector_bus(vector_ram);
    CPU vector_cpu;
    cpu_init(vector_cpu, kEntry);
    write32(vector_ram, kEntry + 0, kVdot);
    const DbtTranslationUnit vector_unit =
        translate_dbt_block(plan_dbt_block(vector_cpu, vector_bus, kEntry, kEntry));

    return expect(!atomic_unit.ok, "translator should reject atomic helper plan") &&
           expect(atomic_unit.reject_kind == DbtRejectKind::Atomic,
                  "translator should classify atomic helper reject") &&
           expect(atomic_unit.helper_plan.required,
                  "translator should preserve atomic helper metadata") &&
           expect(atomic_unit.helper_plan.kind == DbtHelperKind::Atomic,
                  "translator should preserve atomic helper kind") &&
           expect(atomic_unit.helper_plan.atomic_op == DbtAtomicHelperOp::Add,
                  "translator should preserve atomic operation") &&
           expect(atomic_unit.helper_plan.addr == kHelperData &&
                      atomic_unit.helper_plan.size == 4 &&
                      atomic_unit.helper_plan.value == 0x12345678U,
                  "translator should preserve atomic address, width, and value") &&
           expect(atomic_unit.helper_plan.atomic_aq && !atomic_unit.helper_plan.atomic_rl,
                  "translator should preserve atomic ordering bits") &&
           expect(atomic_unit.instructions.empty(),
                  "translator should not emit prefix IR for atomic helper plan") &&
           expect(!vector_unit.ok, "translator should reject vector helper plan") &&
           expect(vector_unit.reject_kind == DbtRejectKind::Vector,
                  "translator should classify vector helper reject") &&
           expect(vector_unit.helper_plan.required,
                  "translator should preserve vector helper metadata") &&
           expect(vector_unit.helper_plan.vector_op == DbtVectorHelperOp::Dot,
                  "translator should preserve vector operation") &&
           expect(vector_unit.helper_plan.rd == 5 &&
                      vector_unit.helper_plan.vector_vs1 == 1 &&
                      vector_unit.helper_plan.vector_vs2 == 2,
                  "translator should preserve vector register operands") &&
           expect(vector_unit.instructions.empty(),
                  "translator should not emit prefix IR for vector helper plan");
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
           expect(!control_unit.helper_plan.required,
                  "translator should not attach helper plan to control-flow fallback") &&
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
    if (!test_translates_u_type_and_word_ops_to_ir_v0()) {
        return 1;
    }
    if (!test_rejects_supported_non_ir_v0_integer_ops_without_prefix_ir()) {
        return 1;
    }
    if (!test_preserves_atomic_and_vector_helper_metadata()) {
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
