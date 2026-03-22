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

bool is_sfence_vma(const Insn& insn) {
    return (insn.raw & 0xFE007FFFU) == 0x12000073U;
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

uint64_t counter_access_mask(uint32_t addr) {
    switch (addr & 0xFFF) {
    case CSR_CYCLE:
        return 1ULL << 0;
    case CSR_TIME:
        return 1ULL << 1;
    case CSR_INSTRET:
        return 1ULL << 2;
    default:
        return 0;
    }
}

bool counter_read_allowed(const CPU& cpu, uint32_t addr) {
    const uint64_t mask = counter_access_mask(addr);
    if (mask == 0) {
        return true;
    }

    switch (cpu.core().privilege_mode()) {
    case PrivilegeMode::Machine:
        return true;
    case PrivilegeMode::Supervisor:
        return (cpu.csr().read(CSR_MCOUNTEREN, cpu.core()) & mask) != 0;
    case PrivilegeMode::User:
        return (cpu.csr().read(CSR_MCOUNTEREN, cpu.core()) & mask) != 0 &&
               (cpu.csr().read(CSR_SCOUNTEREN, cpu.core()) & mask) != 0;
    }

    return false;
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

    if (!write && !counter_read_allowed(cpu, addr)) {
        return false;
    }

    const bool read_only = ((addr >> 10) & 0x3) == 0x3;
    return !write || !read_only;
}

}  // namespace

bool execute_system_instruction(CPU& cpu, const Insn& insn, bool& retired) {
    CoreState& core = cpu.core();
    const uint64_t pc = core.pc();
    const uint32_t csr_addr = insn.raw >> 20;
    const uint64_t rs1v = core.read_gpr(insn.rs1);
    uint64_t old = 0;
    retired = false;

    switch (insn.funct3) {
    case 0:
        if (insn.raw == 0x00000073) {
            if (core.read_gpr(17) == 93) {
                core.set_halted(true);
                retired = true;
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
            retired = true;
            return false;
        }
        if (insn.raw == 0x10200073) {
            if (core.privilege_mode() == PrivilegeMode::User) {
                cpu.trap().enter_exception(CAUSE_ILLEGAL_INSN, insn.raw);
                return false;
            }
            cpu.trap().return_from_sret();
            retired = true;
            return false;
        }
        if (is_sfence_vma(insn)) {
            if (core.privilege_mode() == PrivilegeMode::User) {
                cpu.trap().enter_exception(CAUSE_ILLEGAL_INSN, insn.raw);
                return false;
            }
            cpu.address_space().flush_tlb();
            retired = true;
            return true;
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
        retired = true;
        return true;
    case 2:
        if (!csr_access_allowed(cpu, csr_addr, csr_instruction_writes(insn))) {
            cpu.trap().enter_exception(CAUSE_ILLEGAL_INSN, insn.raw);
            return false;
        }
        old = csr_read(cpu, csr_addr);
        write_rd(cpu, insn.rd, old);
        csr_write(cpu, csr_addr, old | rs1v);
        retired = true;
        return true;
    case 3:
        if (!csr_access_allowed(cpu, csr_addr, csr_instruction_writes(insn))) {
            cpu.trap().enter_exception(CAUSE_ILLEGAL_INSN, insn.raw);
            return false;
        }
        old = csr_read(cpu, csr_addr);
        write_rd(cpu, insn.rd, old);
        csr_write(cpu, csr_addr, old & ~rs1v);
        retired = true;
        return true;
    case 5:
        if (!csr_access_allowed(cpu, csr_addr, csr_instruction_writes(insn))) {
            cpu.trap().enter_exception(CAUSE_ILLEGAL_INSN, insn.raw);
            return false;
        }
        old = csr_read(cpu, csr_addr);
        write_rd(cpu, insn.rd, old);
        csr_write(cpu, csr_addr, insn.rs1);
        retired = true;
        return true;
    case 6:
        if (!csr_access_allowed(cpu, csr_addr, csr_instruction_writes(insn))) {
            cpu.trap().enter_exception(CAUSE_ILLEGAL_INSN, insn.raw);
            return false;
        }
        old = csr_read(cpu, csr_addr);
        write_rd(cpu, insn.rd, old);
        csr_write(cpu, csr_addr, old | insn.rs1);
        retired = true;
        return true;
    case 7:
        if (!csr_access_allowed(cpu, csr_addr, csr_instruction_writes(insn))) {
            cpu.trap().enter_exception(CAUSE_ILLEGAL_INSN, insn.raw);
            return false;
        }
        old = csr_read(cpu, csr_addr);
        write_rd(cpu, insn.rd, old);
        csr_write(cpu, csr_addr, old & ~static_cast<uint64_t>(insn.rs1));
        retired = true;
        return true;
    default:
        cpu.trap().enter_exception(CAUSE_ILLEGAL_INSN, insn.raw);
        return false;
    }
}
