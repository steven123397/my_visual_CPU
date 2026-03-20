#include "system_ops.h"

#include "../cpu.h"

namespace {

constexpr uint64_t CAUSE_ILLEGAL_INSN = 2;
constexpr uint64_t CAUSE_BREAKPOINT = 3;
constexpr uint64_t CAUSE_ECALL_M = 11;

void write_rd(CPU& cpu, uint8_t rd, uint64_t value) {
    cpu.core().write_gpr(rd, value);
}

}  // namespace

bool execute_system_instruction(CPU& cpu, const Insn& insn) {
    CoreState& core = cpu.core();
    const uint64_t pc = core.pc();
    const uint32_t csr_addr = insn.raw >> 20;
    const uint64_t rs1v = core.read_gpr(insn.rs1);
    uint64_t old = 0;

    switch (insn.funct3) {
    case 0:
        if (insn.raw == 0x00000073) {
            if (core.read_gpr(17) == 93) {
                core.set_halted(true);
            } else {
                cpu.trap().enter_exception(CAUSE_ECALL_M, 0);
            }
            return false;
        }
        if (insn.raw == 0x00100073) {
            cpu.trap().enter_exception(CAUSE_BREAKPOINT, pc);
            return false;
        }
        if (insn.raw == 0x30200073) {
            cpu.trap().return_from_mret();
            return false;
        }
        cpu.trap().enter_exception(CAUSE_ILLEGAL_INSN, insn.raw);
        return false;
    case 1:
        old = csr_read(cpu, csr_addr);
        write_rd(cpu, insn.rd, old);
        csr_write(cpu, csr_addr, rs1v);
        return true;
    case 2:
        old = csr_read(cpu, csr_addr);
        write_rd(cpu, insn.rd, old);
        csr_write(cpu, csr_addr, old | rs1v);
        return true;
    case 3:
        old = csr_read(cpu, csr_addr);
        write_rd(cpu, insn.rd, old);
        csr_write(cpu, csr_addr, old & ~rs1v);
        return true;
    case 5:
        old = csr_read(cpu, csr_addr);
        write_rd(cpu, insn.rd, old);
        csr_write(cpu, csr_addr, insn.rs1);
        return true;
    case 6:
        old = csr_read(cpu, csr_addr);
        write_rd(cpu, insn.rd, old);
        csr_write(cpu, csr_addr, old | insn.rs1);
        return true;
    case 7:
        old = csr_read(cpu, csr_addr);
        write_rd(cpu, insn.rd, old);
        csr_write(cpu, csr_addr, old & ~static_cast<uint64_t>(insn.rs1));
        return true;
    default:
        cpu.trap().enter_exception(CAUSE_ILLEGAL_INSN, insn.raw);
        return false;
    }
}
