#include <cstdio>

#include "../../src/cpu.h"
#include "../../src/isa/execution_context.h"
#include "../../src/isa/instruction_semantics.h"
#include "../../src/mem/bus.h"
#include "../../src/mem/ram.h"

namespace {

bool expect(bool condition, const char* message) {
    if (!condition) {
        std::fprintf(stderr, "%s\n", message);
        return false;
    }
    return true;
}

}  // namespace

int main() {
    Ram ram;
    Bus bus(ram);
    CPU cpu;
    cpu_init(cpu, 0x80000000);
    ExecutionContext ctx(cpu, bus);

    cpu.core().write_gpr(1, 5);
    Insn addi{};
    addi.raw = 0x00708293U;
    addi.opcode = 0x13;
    addi.rd = 5;
    addi.funct3 = 0;
    addi.rs1 = 1;
    addi.imm = 7;

    if (!expect(InstructionSemantics::supports(addi), "addi should be supported by instruction semantics")) {
        return 1;
    }
    const InsnEffects addi_effects = InstructionSemantics::execute(addi, ctx);
    if (!expect(addi_effects.rd_write.enable, "addi should produce a register write")) {
        return 1;
    }
    if (!expect(addi_effects.rd_write.rd == 5, "addi should target x5")) {
        return 1;
    }
    if (!expect(addi_effects.rd_write.value == 12, "addi should compute x1 + imm")) {
        return 1;
    }
    if (!expect(!addi_effects.control.redirect_pc, "addi should not redirect pc")) {
        return 1;
    }
    if (!expect(!addi_effects.trap.valid, "addi should not raise a trap")) {
        return 1;
    }

    cpu.core().set_pc(0x80000040);
    Insn jal{};
    jal.raw = 0x010000efU;
    jal.opcode = 0x6F;
    jal.rd = 1;
    jal.imm = 16;

    if (!expect(InstructionSemantics::supports(jal), "jal should be supported by instruction semantics")) {
        return 1;
    }
    const InsnEffects jal_effects = InstructionSemantics::execute(jal, ctx);
    if (!expect(jal_effects.rd_write.enable, "jal should write the link register")) {
        return 1;
    }
    if (!expect(jal_effects.rd_write.rd == 1, "jal should target x1")) {
        return 1;
    }
    if (!expect(jal_effects.rd_write.value == 0x80000044ULL, "jal should write pc + 4")) {
        return 1;
    }
    if (!expect(jal_effects.control.redirect_pc, "jal should redirect pc")) {
        return 1;
    }
    if (!expect(jal_effects.control.target_pc == 0x80000050ULL, "jal should redirect to pc + imm")) {
        return 1;
    }
    if (!expect(!jal_effects.trap.valid, "jal should not raise a trap")) {
        return 1;
    }

    Insn illegal_branch{};
    illegal_branch.raw = 0xffffffffU;
    illegal_branch.opcode = 0x63;
    illegal_branch.funct3 = 2;
    illegal_branch.rs1 = 1;
    illegal_branch.rs2 = 2;
    illegal_branch.imm = 8;

    if (!expect(InstructionSemantics::supports(illegal_branch), "branch family should be routed through instruction semantics")) {
        return 1;
    }
    const InsnEffects illegal_effects = InstructionSemantics::execute(illegal_branch, ctx);
    if (!expect(illegal_effects.trap.valid, "illegal branch encoding should raise a trap effect")) {
        return 1;
    }
    if (!expect(illegal_effects.trap.cause == 2, "illegal branch trap cause should be illegal instruction")) {
        return 1;
    }
    if (!expect(illegal_effects.trap.tval == illegal_branch.raw, "illegal branch trap tval should preserve raw instruction")) {
        return 1;
    }

    Insn load{};
    load.raw = 0x0040a303U;
    load.opcode = 0x03;
    load.rd = 6;
    load.funct3 = 2;
    load.rs1 = 1;
    load.imm = 4;

    if (!expect(InstructionSemantics::supports(load), "load should be supported by instruction semantics")) {
        return 1;
    }
    const InsnEffects load_effects = InstructionSemantics::execute(load, ctx);
    if (!expect(load_effects.mem.kind == MemoryRequest::Kind::Load, "load should produce a load memory request")) {
        return 1;
    }
    if (!expect(load_effects.mem.addr == 9, "load should compute base plus offset")) {
        return 1;
    }
    if (!expect(load_effects.mem.size == 4, "lw should request 4 bytes")) {
        return 1;
    }
    if (!expect(load_effects.mem.sign_extend, "lw should request sign extension")) {
        return 1;
    }
    if (!expect(load_effects.mem.rd == 6, "load should preserve destination register")) {
        return 1;
    }
    if (!expect(!load_effects.trap.valid, "legal load should not raise a trap")) {
        return 1;
    }

    Insn store{};
    store.raw = 0x0020a223U;
    store.opcode = 0x23;
    store.funct3 = 2;
    store.rs1 = 1;
    store.rs2 = 2;
    store.imm = 8;
    cpu.core().write_gpr(2, 0x11223344ULL);

    const InsnEffects store_effects = InstructionSemantics::execute(store, ctx);
    if (!expect(store_effects.mem.kind == MemoryRequest::Kind::Store, "store should produce a store memory request")) {
        return 1;
    }
    if (!expect(store_effects.mem.addr == 13, "store should compute base plus offset")) {
        return 1;
    }
    if (!expect(store_effects.mem.size == 4, "sw should request 4 bytes")) {
        return 1;
    }
    if (!expect(store_effects.mem.store_value == 0x11223344ULL, "store should preserve the source register value")) {
        return 1;
    }
    if (!expect(!store_effects.trap.valid, "legal store should not raise a trap")) {
        return 1;
    }

    Insn fld{};
    fld.raw = 0x0080b287U;
    fld.opcode = 0x07;
    fld.rd = 5;
    fld.funct3 = 3;
    fld.rs1 = 1;
    fld.imm = 8;

    if (!expect(InstructionSemantics::supports(fld), "fld should be supported by instruction semantics")) {
        return 1;
    }
    const InsnEffects fld_effects = InstructionSemantics::execute(fld, ctx);
    if (!expect(fld_effects.mem.kind == MemoryRequest::Kind::Load, "fld should produce a load memory request")) {
        return 1;
    }
    if (!expect(fld_effects.mem.rd == 5, "fld should preserve destination f register")) {
        return 1;
    }
    if (!expect(fld_effects.mem.size == 8, "fld should request 8 bytes")) {
        return 1;
    }
    if (!expect(fld_effects.mem.target == MemoryRequest::Target::Float,
                "fld should target the floating-point register file")) {
        return 1;
    }
    if (!expect(!fld_effects.trap.valid, "legal fld should not raise a trap")) {
        return 1;
    }

    Insn fsd{};
    fsd.raw = 0x0050b427U;
    fsd.opcode = 0x27;
    fsd.funct3 = 3;
    fsd.rs1 = 1;
    fsd.rs2 = 5;
    fsd.imm = 8;
    cpu.core().write_fpr(5, 0x8877665544332211ULL);

    const InsnEffects fsd_effects = InstructionSemantics::execute(fsd, ctx);
    if (!expect(fsd_effects.mem.kind == MemoryRequest::Kind::Store, "fsd should produce a store memory request")) {
        return 1;
    }
    if (!expect(fsd_effects.mem.size == 8, "fsd should request 8 bytes")) {
        return 1;
    }
    if (!expect(fsd_effects.mem.store_value == 0x8877665544332211ULL,
                "fsd should preserve the source f register bits")) {
        return 1;
    }
    if (!expect(fsd_effects.mem.target == MemoryRequest::Target::Float,
                "fsd should identify the floating-point register file")) {
        return 1;
    }
    if (!expect(!fsd_effects.trap.valid, "legal fsd should not raise a trap")) {
        return 1;
    }

    return 0;
}
