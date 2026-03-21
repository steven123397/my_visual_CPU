#include "system_ops.h"

#include "../cpu.h"

namespace {

constexpr uint64_t CAUSE_ILLEGAL_INSN = 2;
constexpr uint64_t CAUSE_ECALL_U = 8;
constexpr uint64_t CAUSE_ECALL_S = 9;
constexpr uint64_t CAUSE_BREAKPOINT = 3;
constexpr uint64_t CAUSE_ECALL_M = 11;

void write_rd(CPU& cpu, uint8_t rd, uint64_t value) {
    cpu.core().write_gpr(rd, value);
}

bool csr_instruction_writes(const Insn& insn) {
    switch (insn.funct3) {
    case 1:
    case 5:
        return true;
    case 2:
    case 3:
    case 6:
    case 7:
        return insn.rs1 != 0;
    default:
        return false;
    }
}

bool csr_access_allowed(const CPU& cpu, uint32_t addr, bool write) {
    if (!cpu.csr().is_implemented(addr)) {
        return false;
    }

    const uint32_t required_privilege = (addr >> 8) & 0x3;
    const uint32_t current_privilege = static_cast<uint32_t>(cpu.core().privilege_mode());
    if (required_privilege == 2 || current_privilege < required_privilege) {
        return false;
    }

    const bool read_only = ((addr >> 10) & 0x3) == 0x3;
    return !write || !read_only;
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
                uint64_t cause = CAUSE_ECALL_M;
                switch (core.privilege_mode()) {
                case PrivilegeMode::User:
                    cause = CAUSE_ECALL_U;
                    break;
                case PrivilegeMode::Supervisor:
                    cause = CAUSE_ECALL_S;
                    break;
                case PrivilegeMode::Machine:
                    cause = CAUSE_ECALL_M;
                    break;
                }
                cpu.trap().enter_exception(cause, 0);
            }
            return false;
        }
        if (insn.raw == 0x00100073) {
            cpu.trap().enter_exception(CAUSE_BREAKPOINT, pc);
            return false;
        }
        if (insn.raw == 0x30200073) {
            if (core.privilege_mode() != PrivilegeMode::Machine) {
                cpu.trap().enter_exception(CAUSE_ILLEGAL_INSN, insn.raw);
                return false;
            }
            cpu.trap().return_from_mret();
            return false;
        }
        if (insn.raw == 0x10200073) {
            if (core.privilege_mode() == PrivilegeMode::User) {
                cpu.trap().enter_exception(CAUSE_ILLEGAL_INSN, insn.raw);
                return false;
            }
            cpu.trap().return_from_sret();
            return false;
        }
        cpu.trap().enter_exception(CAUSE_ILLEGAL_INSN, insn.raw);
        return false;
    case 1:
        if (!csr_access_allowed(cpu, csr_addr, csr_instruction_writes(insn))) {
            cpu.trap().enter_exception(CAUSE_ILLEGAL_INSN, insn.raw);
            return false;
        }
        old = csr_read(cpu, csr_addr);
        write_rd(cpu, insn.rd, old);
        csr_write(cpu, csr_addr, rs1v);
        return true;
    case 2:
        if (!csr_access_allowed(cpu, csr_addr, csr_instruction_writes(insn))) {
            cpu.trap().enter_exception(CAUSE_ILLEGAL_INSN, insn.raw);
            return false;
        }
        old = csr_read(cpu, csr_addr);
        write_rd(cpu, insn.rd, old);
        csr_write(cpu, csr_addr, old | rs1v);
        return true;
    case 3:
        if (!csr_access_allowed(cpu, csr_addr, csr_instruction_writes(insn))) {
            cpu.trap().enter_exception(CAUSE_ILLEGAL_INSN, insn.raw);
            return false;
        }
        old = csr_read(cpu, csr_addr);
        write_rd(cpu, insn.rd, old);
        csr_write(cpu, csr_addr, old & ~rs1v);
        return true;
    case 5:
        if (!csr_access_allowed(cpu, csr_addr, csr_instruction_writes(insn))) {
            cpu.trap().enter_exception(CAUSE_ILLEGAL_INSN, insn.raw);
            return false;
        }
        old = csr_read(cpu, csr_addr);
        write_rd(cpu, insn.rd, old);
        csr_write(cpu, csr_addr, insn.rs1);
        return true;
    case 6:
        if (!csr_access_allowed(cpu, csr_addr, csr_instruction_writes(insn))) {
            cpu.trap().enter_exception(CAUSE_ILLEGAL_INSN, insn.raw);
            return false;
        }
        old = csr_read(cpu, csr_addr);
        write_rd(cpu, insn.rd, old);
        csr_write(cpu, csr_addr, old | insn.rs1);
        return true;
    case 7:
        if (!csr_access_allowed(cpu, csr_addr, csr_instruction_writes(insn))) {
            cpu.trap().enter_exception(CAUSE_ILLEGAL_INSN, insn.raw);
            return false;
        }
        old = csr_read(cpu, csr_addr);
        write_rd(cpu, insn.rd, old);
        csr_write(cpu, csr_addr, old & ~static_cast<uint64_t>(insn.rs1));
        return true;
    default:
        cpu.trap().enter_exception(CAUSE_ILLEGAL_INSN, insn.raw);
        return false;
    }
}
