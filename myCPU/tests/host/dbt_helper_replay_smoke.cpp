#include <cstdint>
#include <cstdio>
#include <string>

#include "../../src/cpu.h"
#include "../../src/exec/dbt_block_plan.h"
#include "../../src/exec/dbt_helper_replay.h"
#include "../../src/exec/dbt_translator.h"
#include "../../src/mem/bus.h"
#include "../../src/mem/ram.h"

namespace {

constexpr uint64_t kEntry = MEM_BASE;
constexpr uint64_t kHelperData = MEM_BASE + 0x100;
constexpr uint32_t kAddiX1One = 0x00100093U;  // addi x1, x0, 1
constexpr uint32_t kLwX1FromX0 = 0x00002083U;  // lw x1, 0(x0)
constexpr uint32_t kSwX2To8FromX0 = 0x00202423U;  // sw x2, 8(x0)
constexpr uint32_t kCsrrwX1MstatusX2 = 0x300110f3U;  // csrrw x1, mstatus, x2
constexpr uint32_t kJalX0Skip8 = 0x0080006fU;  // jal x0, 8

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

constexpr uint32_t encode_itype(uint8_t opcode,
                                uint8_t funct3,
                                uint8_t rd,
                                uint8_t rs1,
                                int32_t imm12) {
    const uint32_t imm = static_cast<uint32_t>(imm12) & 0xfffU;
    return (imm << 20) |
           (static_cast<uint32_t>(rs1) << 15) |
           (static_cast<uint32_t>(funct3) << 12) |
           (static_cast<uint32_t>(rd) << 7) |
           static_cast<uint32_t>(opcode);
}

constexpr uint32_t encode_stype(uint8_t opcode,
                                uint8_t funct3,
                                uint8_t rs1,
                                uint8_t rs2,
                                int32_t imm12) {
    const uint32_t imm = static_cast<uint32_t>(imm12) & 0xfffU;
    return (((imm >> 5) & 0x7fU) << 25) |
           (static_cast<uint32_t>(rs2) << 20) |
           (static_cast<uint32_t>(rs1) << 15) |
           (static_cast<uint32_t>(funct3) << 12) |
           ((imm & 0x1fU) << 7) |
           static_cast<uint32_t>(opcode);
}

uint8_t sew_code_from_bytes(uint8_t sew_bytes) {
    switch (sew_bytes) {
    case 1:
        return 0;
    case 2:
        return 1;
    case 4:
        return 3;
    case 8:
        return 7;
    default:
        return 2;
    }
}

uint32_t encode_vsetcfg(uint8_t sew_bytes, uint8_t vl) {
    const uint8_t funct7 = static_cast<uint8_t>(0x40U | sew_code_from_bytes(sew_bytes));
    return encode_rtype(0x57, 7, funct7, 0, 0, static_cast<uint8_t>(vl - 1));
}

uint32_t encode_vle(uint8_t vd, uint8_t base, int32_t imm12) {
    return encode_itype(0x07, 0, vd, base, imm12);
}

uint32_t encode_vse(uint8_t vs2, uint8_t base, int32_t imm12) {
    return encode_stype(0x27, 0, base, vs2, imm12);
}

uint32_t encode_vdot(uint8_t vd, uint8_t vs1, uint8_t vs2) {
    return encode_rtype(0x57, 0, 0x22, vd, vs1, vs2);
}

DbtTranslationUnit translate_single_helper(uint32_t raw,
                                           uint64_t rs1_value = 0,
                                           uint64_t rs2_value = 0) {
    Ram ram;
    Bus bus(ram);
    CPU cpu;
    cpu_init(cpu, kEntry);
    cpu.core().write_gpr(10, rs1_value);
    cpu.core().write_gpr(6, rs2_value);
    cpu.core().write_gpr(2, rs2_value);
    write32(ram, kEntry, raw);
    return translate_dbt_block(plan_dbt_block(cpu, bus, kEntry, kEntry));
}

bool test_helper_replay_rejects_non_helper_units() {
    Ram ok_ram;
    Bus ok_bus(ok_ram);
    CPU ok_cpu;
    cpu_init(ok_cpu, kEntry);
    write32(ok_ram, kEntry, kAddiX1One);
    const DbtTranslationUnit ok_unit =
        translate_dbt_block(plan_dbt_block(ok_cpu, ok_bus, kEntry, kEntry));

    Ram fallback_ram;
    Bus fallback_bus(fallback_ram);
    CPU fallback_cpu;
    cpu_init(fallback_cpu, kEntry);
    write32(fallback_ram, kEntry, kJalX0Skip8);
    const DbtTranslationUnit fallback_unit =
        translate_dbt_block(plan_dbt_block(fallback_cpu, fallback_bus, kEntry, kEntry));

    const DbtHelperReplayPlan ok_replay = plan_dbt_helper_replay(ok_unit);
    const DbtHelperReplayPlan fallback_replay = plan_dbt_helper_replay(fallback_unit);

    return expect(!ok_replay.ok, "helper replay should reject already translated units") &&
           expect(ok_replay.reject_reason == "translation-unit-ok",
                  "helper replay should explain ok-unit rejection") &&
           expect(!fallback_replay.ok, "helper replay should reject non-helper fallbacks") &&
           expect(fallback_replay.reject_reason == "no-helper-plan",
                  "helper replay should explain missing helper metadata");
}

bool test_helper_replay_classifies_scalar_helpers() {
    const DbtHelperReplayPlan load =
        plan_dbt_helper_replay(translate_single_helper(kLwX1FromX0));
    const DbtHelperReplayPlan store =
        plan_dbt_helper_replay(translate_single_helper(kSwX2To8FromX0, 0, 0x11223344));
    const DbtHelperReplayPlan csr =
        plan_dbt_helper_replay(translate_single_helper(kCsrrwX1MstatusX2, 0, 0x1800));

    return expect(load.ok, "load helper replay should be plannable") &&
           expect(load.kind == DbtHelperReplayKind::ScalarMemoryLoad,
                  "load helper replay should classify scalar load") &&
           expect(load.reads_memory && !load.writes_memory && load.writes_gpr,
                  "load helper replay should expose memory read and GPR write effects") &&
           expect(load.addr == 0 && load.size == 4 && load.sign_extend,
                  "load helper replay should preserve address, width, and sign-extension") &&
           expect(load.may_trap && !load.serializing,
                  "load helper replay should expose trap potential without serialization") &&
           expect(store.ok, "store helper replay should be plannable") &&
           expect(store.kind == DbtHelperReplayKind::ScalarMemoryStore,
                  "store helper replay should classify scalar store") &&
           expect(!store.reads_memory && store.writes_memory && !store.writes_gpr,
                  "store helper replay should expose memory write effects") &&
           expect(store.value == 0x11223344 && store.commit_at_boundary &&
                      store.non_speculative && store.serializing,
                  "store helper replay should preserve value and ordering flags") &&
           expect(csr.ok, "CSR helper replay should be plannable") &&
           expect(csr.kind == DbtHelperReplayKind::CsrWrite,
                  "CSR helper replay should classify CSR writes") &&
           expect(csr.writes_csr && csr.writes_gpr && !csr.reads_memory &&
                      !csr.writes_memory,
                  "CSR helper replay should expose CSR and GPR effects") &&
           expect(csr.csr_addr == 0x300 && csr.value == 0x1800,
                  "CSR helper replay should preserve CSR address and value");
}

bool test_helper_replay_classifies_atomic_and_vector_helpers() {
    const uint32_t kAmoAddW =
        encode_amo(0x00, true, false, 6, 10, 0x2, 5);  // amoadd.w.aq x5, x6, (x10)
    const DbtHelperReplayPlan atomic =
        plan_dbt_helper_replay(translate_single_helper(kAmoAddW, kHelperData, 0x12345678));

    const DbtHelperReplayPlan vset =
        plan_dbt_helper_replay(translate_single_helper(encode_vsetcfg(4, 4)));
    const DbtHelperReplayPlan vle =
        plan_dbt_helper_replay(translate_single_helper(encode_vle(3, 10, 12), kHelperData));
    const DbtHelperReplayPlan vse =
        plan_dbt_helper_replay(translate_single_helper(encode_vse(4, 10, 16), kHelperData));
    const DbtHelperReplayPlan vdot =
        plan_dbt_helper_replay(translate_single_helper(encode_vdot(5, 1, 2)));

    return expect(atomic.ok, "atomic helper replay should be plannable") &&
           expect(atomic.kind == DbtHelperReplayKind::AtomicMemory,
                  "atomic helper replay should classify atomic memory helper") &&
           expect(atomic.atomic_op == DbtAtomicHelperOp::Add &&
                      atomic.reads_memory && atomic.writes_memory && atomic.writes_gpr,
                  "atomic helper replay should expose op and memory/GPR effects") &&
           expect(atomic.atomic_aq && !atomic.atomic_rl && atomic.serializing &&
                      atomic.commit_at_boundary && atomic.non_speculative,
                  "atomic helper replay should preserve ordering contract") &&
           expect(vset.ok && vset.kind == DbtHelperReplayKind::VectorConfig,
                  "vector set-config helper replay should classify vector config") &&
           expect(vset.changes_vector_config && vset.vector_sew_bytes == 4 &&
                      vset.vector_vl == 4,
                  "vector set-config helper replay should preserve config") &&
           expect(vle.ok && vle.kind == DbtHelperReplayKind::VectorMemoryLoad,
                  "vector load helper replay should classify vector memory load") &&
           expect(vle.reads_memory && vle.writes_vector && vle.addr == kHelperData + 12,
                  "vector load helper replay should expose memory read and vector write") &&
           expect(vse.ok && vse.kind == DbtHelperReplayKind::VectorMemoryStore,
                  "vector store helper replay should classify vector memory store") &&
           expect(vse.writes_memory && !vse.writes_vector && vse.addr == kHelperData + 16,
                  "vector store helper replay should expose memory write") &&
           expect(vdot.ok && vdot.kind == DbtHelperReplayKind::VectorAlu,
                  "vector ALU helper replay should classify vector ALU") &&
           expect(vdot.writes_vector && vdot.vector_op == DbtVectorHelperOp::Dot &&
                      vdot.rd == 5 && vdot.vector_vs1 == 1 && vdot.vector_vs2 == 2,
                  "vector ALU helper replay should preserve register operands") &&
           expect(dbt_helper_replay_kind_name(DbtHelperReplayKind::AtomicMemory) ==
                      std::string("atomic-memory"),
                  "helper replay kind name should expose atomic-memory") &&
           expect(dbt_helper_replay_kind_name(DbtHelperReplayKind::VectorAlu) ==
                      std::string("vector-alu"),
                  "helper replay kind name should expose vector-alu");
}

}  // namespace

int main() {
    if (!test_helper_replay_rejects_non_helper_units()) {
        return 1;
    }
    if (!test_helper_replay_classifies_scalar_helpers()) {
        return 1;
    }
    if (!test_helper_replay_classifies_atomic_and_vector_helpers()) {
        return 1;
    }
    std::puts("dbt_helper_replay_smoke: PASS");
    return 0;
}
