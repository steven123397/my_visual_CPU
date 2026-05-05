#include <cstdio>

#include "../../src/cpu.h"
#include "../../src/isa/execution_context.h"
#include "../../src/isa/instruction_semantics.h"
#include "../../src/mem/bus.h"
#include "../../src/mem/ram.h"

extern "C" {
#include "../../src/decode.h"
}

namespace {

constexpr uint64_t kEntry = 0x80000000ULL;

bool expect(bool condition, const char* message) {
    if (!condition) {
        std::fprintf(stderr, "%s\n", message);
        return false;
    }
    return true;
}

void write_halfword(Ram& ram, uint64_t addr, uint16_t value) {
    ram.store(addr, value, 2);
}

bool test_decode_examples() {
    Insn addi4spn{};
    decode(0x0088U, &addi4spn);
    if (!expect(addi4spn.size == 2, "c.addi4spn should decode as a 16-bit instruction") ||
        !expect(addi4spn.opcode == 0x13 && addi4spn.rd == 10 && addi4spn.rs1 == 2 && addi4spn.imm == 64,
                "c.addi4spn should expand to addi a0, sp, 64")) {
        return false;
    }

    Insn jalr{};
    decode(0x9502U, &jalr);
    if (!expect(jalr.size == 2, "c.jalr should stay 16-bit after decode") ||
        !expect(jalr.opcode == 0x67 && jalr.rd == 1 && jalr.rs1 == 10 && jalr.imm == 0,
                "c.jalr should expand to jalr ra, 0(a0)")) {
        return false;
    }

    Insn beqz{};
    decode(0xC111U, &beqz);
    return expect(beqz.size == 2, "c.beqz should stay 16-bit after decode") &&
           expect(beqz.opcode == 0x63 && beqz.funct3 == 0 && beqz.rs1 == 10 && beqz.rs2 == 0 && beqz.imm == 4,
                  "c.beqz should expand to beq a0, zero, 4");
}

bool test_compressed_jalr_link_uses_pc_plus_2() {
    Ram ram;
    Bus bus(ram);
    CPU cpu;
    cpu_init(cpu, kEntry);
    ExecutionContext ctx(cpu, bus);

    cpu.core().set_pc(kEntry + 0x20);
    cpu.core().write_gpr(10, kEntry + 0x80);

    Insn insn{};
    decode(0x9502U, &insn);
    const InsnEffects effects = InstructionSemantics::execute(insn, ctx);

    return expect(effects.rd_write.enable, "c.jalr should write the link register") &&
           expect(effects.rd_write.rd == 1, "c.jalr should target x1") &&
           expect(effects.rd_write.value == kEntry + 0x22, "c.jalr should link with pc + 2") &&
           expect(effects.control.redirect_pc, "c.jalr should redirect control flow") &&
           expect(effects.control.target_pc == kEntry + 0x80, "c.jalr target should come from rs1");
}

bool test_cpu_step_handles_halfword_pc_progression() {
    Ram ram;
    Bus bus(ram);
    CPU cpu;
    cpu_init(cpu, kEntry);

    write_halfword(ram, kEntry + 0, 0x157DU);  // c.addi a0, -1
    write_halfword(ram, kEntry + 2, 0xA011U);  // c.j +4
    write_halfword(ram, kEntry + 4, 0x2585U);  // c.addiw a1, 1 (skipped)
    write_halfword(ram, kEntry + 6, 0x2585U);  // c.addiw a1, 1

    cpu.core().write_gpr(10, 5);

    cpu_step(cpu, bus);
    if (!expect(cpu.core().read_gpr(10) == 4, "c.addi should retire through the functional CPU step") ||
        !expect(cpu.core().pc() == kEntry + 2, "compressed fall-through should advance pc by 2")) {
        return false;
    }

    cpu_step(cpu, bus);
    if (!expect(cpu.core().pc() == kEntry + 6, "c.j should branch to the halfword-aligned target")) {
        return false;
    }

    cpu_step(cpu, bus);
    return expect(cpu.core().read_gpr(11) == 1, "c.addiw at the branch target should execute") &&
           expect(cpu.core().pc() == kEntry + 8, "compressed target instruction should still retire with pc + 2");
}

bool test_compressed_fsd_and_fld_use_fpr_bits() {
    Ram ram;
    Bus bus(ram);
    CPU cpu;
    cpu_init(cpu, kEntry);

    const uint64_t slot = kEntry + 0x100;
    cpu.core().write_gpr(10, slot - 112);
    cpu.core().write_fpr(8, 0x8877665544332211ULL);

    write_halfword(ram, kEntry + 0, 0xb920U);  // c.fsd fs0, 112(a0)
    cpu_step(cpu, bus);
    if (!expect(ram.load(slot, 8) == 0x8877665544332211ULL, "c.fsd should store raw fpr bits to memory") ||
        !expect(cpu.core().pc() == kEntry + 2, "c.fsd should advance pc by 2")) {
        return false;
    }

    cpu.core().write_fpr(9, 0);
    write_halfword(ram, kEntry + 2, 0x3c64U);  // c.fld fs1, 248(s0)
    cpu.core().write_gpr(8, slot - 248);
    cpu_step(cpu, bus);
    return expect(cpu.core().read_fpr(9) == 0x8877665544332211ULL,
                  "c.fld should load raw memory bits into the destination fpr") &&
           expect(cpu.core().pc() == kEntry + 4, "c.fld should advance pc by 2");
}

}  // namespace

int main() {
    return test_decode_examples() &&
                   test_compressed_jalr_link_uses_pc_plus_2() &&
                   test_cpu_step_handles_halfword_pc_progression() &&
                   test_compressed_fsd_and_fld_use_fpr_bits()
               ? 0
               : 1;
}
