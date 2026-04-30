#include <array>
#include <cstdint>
#include <cstdio>
#include <string>
#include <vector>

#include "../../src/cpu.h"
#include "../../src/exec/dbt_block_plan.h"
#include "../../src/exec/dbt_host_emitter.h"
#include "../../src/exec/dbt_ir_eval.h"
#include "../../src/exec/dbt_ir_lowering.h"
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

DbtIrLoweringResult lower_program(const std::vector<uint32_t>& program,
                                  CPU& cpu,
                                  Bus& bus,
                                  Ram& ram,
                                  DbtTranslationUnit* out_unit = nullptr) {
    write_program(ram, program);
    const uint64_t block_end =
        kEntry + static_cast<uint64_t>((program.size() - 1) * sizeof(uint32_t));
    const DbtBlockPlan plan = plan_dbt_block(cpu, bus, kEntry, block_end);
    DbtTranslationUnit unit = translate_dbt_block(plan);
    if (out_unit != nullptr) {
        *out_unit = unit;
    }
    return lower_dbt_ir_unit(unit);
}

bool expect_gprs_equal(const std::array<uint64_t, 32>& lhs,
                       const std::array<uint64_t, 32>& rhs,
                       const char* prefix) {
    for (uint32_t i = 0; i < lhs.size(); ++i) {
        if (lhs[i] != rhs[i]) {
            std::fprintf(stderr,
                         "%s gpr mismatch at x%u: lhs=%llu rhs=%llu\n",
                         prefix,
                         i,
                         static_cast<unsigned long long>(lhs[i]),
                         static_cast<unsigned long long>(rhs[i]));
            return false;
        }
    }
    return true;
}

bool expect_emitted_program_matches_reference(const std::vector<uint32_t>& program,
                                              const char* label) {
    Ram ram;
    Bus bus(ram);
    CPU cpu;
    cpu_init(cpu, kEntry);

    DbtTranslationUnit unit;
    const DbtIrLoweringResult lowered = lower_program(program, cpu, bus, ram, &unit);
    const std::array<uint64_t, 32> input_gprs = snapshot_gprs(cpu);
    DbtHostExecutable emitted = emit_dbt_host_block(lowered);

    std::array<uint64_t, 32> emitted_gprs = input_gprs;
    const uint64_t next_pc = execute_dbt_host_block(emitted, emitted_gprs.data(), cpu.core().pc());

    Ram ref_ram;
    Bus ref_bus(ref_ram);
    CPU ref_cpu;
    cpu_init(ref_cpu, kEntry);
    write_program(ref_ram, program);
    for (size_t i = 0; i < program.size(); ++i) {
        cpu_step(ref_cpu, ref_bus);
    }
    const std::array<uint64_t, 32> expected_gprs = snapshot_gprs(ref_cpu);

    release_dbt_host_executable(emitted);

    return expect(lowered.ok, label) &&
           expect(unit.ok, "translator should accept executable matrix program") &&
           expect(emitted.ok, "host emitter should accept executable matrix program") &&
           expect(next_pc == ref_cpu.core().pc(),
                  "emitted matrix program should return reference next PC") &&
           expect_gprs_equal(emitted_gprs, expected_gprs, label);
}

bool test_host_emitter_executes_pure_integer_lowered_block() {
    const std::vector<uint32_t> program{
        addi(1, 0, 7),
        auipc(7, 0x00012000),
        lui(8, 0x12345000),
        addi(2, 1, -2),
        add(3, 1, 2),
        sub(4, 3, 1),
        xori(5, 4, 0x3f),
        slli(6, 5, 1),
        addi(0, 6, 1),
    };

    Ram ram;
    Bus bus(ram);
    CPU cpu;
    cpu_init(cpu, kEntry);

    DbtTranslationUnit unit;
    const DbtIrLoweringResult lowered = lower_program(program, cpu, bus, ram, &unit);
    const DbtIrEvaluationInput input{
        .gpr = snapshot_gprs(cpu),
        .pc = cpu.core().pc(),
    };
    const DbtIrEvaluationResult expected = evaluate_dbt_ir_unit(unit, input);
    DbtHostExecutable emitted = emit_dbt_host_block(lowered);

    std::array<uint64_t, 32> emitted_gprs = input.gpr;
    const uint64_t next_pc = execute_dbt_host_block(emitted, emitted_gprs.data(), input.pc);
    const std::string line = format_dbt_host_executable(emitted);
    const DbtHostExecutableStats stats = emitted.stats;

    release_dbt_host_executable(emitted);

    return expect(lowered.ok, "lowered source block should be valid") &&
           expect(expected.ok, "IR evaluator should produce reference dry-run result") &&
           expect(stats.instructions_emitted == lowered.instructions.size(),
                  "host emitter should account for all lowered ops") &&
           expect(emitted.ok, "host emitter should accept pure integer lowered block") &&
           expect(emitted.backend == DbtHostEmitterBackend::X86_64SysV,
                  "host emitter should expose x86-64 SysV backend") &&
           expect(emitted.generated_host_code && emitted.requested_executable_memory &&
                      !emitted.executed_guest_code,
                  "host emitter should generate code and executable memory without running by itself") &&
           expect(next_pc == expected.next_pc,
                  "emitted host block should return fallthrough PC") &&
           expect(emitted_gprs[0] == 0,
                  "emitted host block should preserve hardwired x0") &&
           expect_gprs_equal(emitted_gprs,
                             expected.gpr,
                             "host-emitter") &&
           expect(line.find("host-emitter: ok=true") != std::string::npos,
                  "host emitter formatter should expose stable prefix") &&
           expect(line.find("backend=x86_64-sysv") != std::string::npos,
                  "host emitter formatter should expose backend name");
}

bool test_host_emitter_executes_logic_shift_compare_matrix() {
    return expect_emitted_program_matches_reference(
        {
            addi(1, 0, -8),
            addi(2, 0, 7),
            xori(3, 1, 0x7f),
            ori(4, 3, 0x30),
            andi(5, 4, 0x7f),
            slti(6, 1, -1),
            sltiu(7, 1, 1),
            slli(8, 2, 5),
            srli(9, 8, 2),
            srai(10, 1, 2),
            bit_xor(11, 3, 5),
            bit_or(12, 11, 8),
            bit_and(13, 12, 4),
            slt(14, 1, 2),
            sltu(15, 1, 2),
            sll(16, 2, 6),
            srl(17, 8, 2),
            sra(18, 1, 2),
        },
        "host-emitter-logic-shift-compare");
}

bool test_host_emitter_executes_u_type_and_word_matrix() {
    return expect_emitted_program_matches_reference(
        {
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
        },
        "host-emitter-u-type-word");
}

bool test_host_emitter_rejects_unsupported_lowered_ops_without_prefix_code() {
    DbtIrLoweringResult lowered{
        .ok = true,
        .start_pc = kEntry,
        .end_pc = kEntry + 4,
    };
    lowered.instructions.push_back(DbtLoweredInstruction{
        .opcode = DbtLoweredOpcode::Compute,
        .pc = kEntry,
        .alu = DbtLoweredAluOp::Add,
        .lhs_kind = DbtLoweredOperandKind::Gpr,
        .rhs_kind = DbtLoweredOperandKind::Immediate,
        .rd = 1,
        .rs1 = 0,
        .imm = 1,
        .next_pc = kEntry + 4,
        .writes_gpr = true,
    });
    lowered.instructions.push_back(DbtLoweredInstruction{
        .opcode = DbtLoweredOpcode::Compute,
        .pc = kEntry + 4,
        .alu = DbtLoweredAluOp::None,
        .lhs_kind = DbtLoweredOperandKind::Gpr,
        .rhs_kind = DbtLoweredOperandKind::None,
        .rd = 2,
        .rs1 = 1,
        .next_pc = kEntry + 8,
        .writes_gpr = true,
    });

    DbtHostExecutable emitted = emit_dbt_host_block(lowered);
    const DbtHostExecutableStats stats = emitted.stats;

    return expect(!emitted.ok, "unsupported lowered op should reject host emission") &&
           expect(emitted.reject_reason == "unsupported-lowered-op",
                  "unsupported lowered op should expose stable reject reason") &&
           expect(emitted.reject_pc == kEntry + 4,
                  "unsupported lowered op should preserve offending PC") &&
           expect(stats.instructions_emitted == 0 && emitted.code_size == 0 &&
                      emitted.memory.data == nullptr,
                  "host emitter should not expose prefix code on rejection");
}

bool test_host_emitter_rejects_rejected_lowering_and_stable_names() {
    DbtIrLoweringResult lowered{
        .ok = false,
        .start_pc = kEntry,
        .end_pc = kEntry,
        .reject_kind = DbtRejectKind::MemoryLoad,
        .reject_pc = kEntry,
        .reject_raw = 0x00002083U,
        .reject_reason = "helper-required",
    };
    DbtHostExecutable emitted = emit_dbt_host_block(lowered);

    return expect(!emitted.ok, "rejected lowering should not emit host code") &&
           expect(emitted.reject_kind == DbtRejectKind::MemoryLoad,
                  "host emitter should preserve rejected lowering kind") &&
           expect(emitted.reject_reason == "helper-required",
                  "host emitter should preserve rejected lowering reason") &&
           expect(dbt_host_emitter_backend_name(DbtHostEmitterBackend::X86_64SysV) ==
                      std::string("x86_64-sysv"),
                  "host emitter backend names should be stable");
}

}  // namespace

int main() {
    if (!test_host_emitter_executes_pure_integer_lowered_block()) {
        return 1;
    }
    if (!test_host_emitter_executes_logic_shift_compare_matrix()) {
        return 1;
    }
    if (!test_host_emitter_executes_u_type_and_word_matrix()) {
        return 1;
    }
    if (!test_host_emitter_rejects_unsupported_lowered_ops_without_prefix_code()) {
        return 1;
    }
    if (!test_host_emitter_rejects_rejected_lowering_and_stable_names()) {
        return 1;
    }
    std::puts("dbt_host_emitter_smoke: PASS");
    return 0;
}
