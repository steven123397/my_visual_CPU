#include "system_ops.h"

#include "../cpu.h"
#include "../isa/effects.h"
#include "../isa/execution_context.h"
#include "../isa/instruction_semantics.h"

namespace {

constexpr uint64_t CAUSE_ILLEGAL_INSN = 2;
constexpr uint64_t CAUSE_ECALL_U = 8;
constexpr uint64_t CAUSE_ECALL_S = 9;
constexpr uint64_t CAUSE_BREAKPOINT = 3;
constexpr uint64_t CAUSE_ECALL_M = 11;

TrapRequest trap_request(uint64_t cause, uint64_t tval) {
    TrapRequest trap;
    trap.valid = true;
    trap.cause = cause;
    trap.tval = tval;
    return trap;
}

TrapRequest illegal_instruction_trap(uint32_t raw) {
    return trap_request(CAUSE_ILLEGAL_INSN, raw);
}

void set_rd(InsnEffects& effects, uint8_t rd, uint64_t value) {
    effects.rd_write.enable = true;
    effects.rd_write.rd = rd;
    effects.rd_write.value = value;
}

bool is_sfence_vma(const Insn& insn) {
    return (insn.raw & 0xFE007FFFU) == 0x12000073U;
}

bool is_ecall(const Insn& insn) {
    return insn.opcode == 0x73 &&
           insn.funct3 == 0 &&
           insn.rd == 0 &&
           insn.rs1 == 0 &&
           insn.imm == 0;
}

bool is_ebreak(const Insn& insn) {
    return insn.opcode == 0x73 &&
           insn.funct3 == 0 &&
           insn.rd == 0 &&
           insn.rs1 == 0 &&
           insn.imm == 1;
}

bool is_wfi(const Insn& insn) {
    return insn.raw == 0x10500073U;
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
    const uint32_t csr = addr & 0xFFF;
    if ((csr >= CSR_HPMCOUNTER3 && csr <= CSR_HPMCOUNTER31) ||
        (csr >= CSR_MHPMCOUNTER3 && csr <= CSR_MHPMCOUNTER31)) {
        return 1ULL << (csr - (csr >= CSR_MHPMCOUNTER3 && csr <= CSR_MHPMCOUNTER31 ? CSR_MHPMCOUNTER3 : CSR_HPMCOUNTER3) + 3);
    }
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

bool counter_read_allowed(ExecutionContext& ctx, uint32_t addr) {
    const uint64_t mask = counter_access_mask(addr);
    if (mask == 0) {
        return true;
    }

    const CoreState& core = ctx.cpu().core();
    const CsrFile& csr = ctx.cpu().csr();

    switch (core.privilege_mode()) {
    case PrivilegeMode::Machine:
        return true;
    case PrivilegeMode::Supervisor:
        return (csr.read(CSR_MCOUNTEREN, core) & mask) != 0;
    case PrivilegeMode::User:
        return (csr.read(CSR_MCOUNTEREN, core) & mask) != 0 &&
               (csr.read(CSR_SCOUNTEREN, core) & mask) != 0;
    }

    return false;
}

bool csr_access_allowed(ExecutionContext& ctx, uint32_t addr, bool write) {
    CPU& cpu = ctx.cpu();
    if (!cpu.csr().is_implemented(addr)) {
        return false;
    }

    const uint32_t required_privilege = (addr >> 8) & 0x3;
    const uint32_t current_privilege = static_cast<uint32_t>(cpu.core().privilege_mode());
    if (required_privilege == 2 || current_privilege < required_privilege) {
        return false;
    }

    if (!write && !counter_read_allowed(ctx, addr)) {
        return false;
    }

    if (write && addr == CSR_MISA) {
        return false;
    }

    const bool read_only = ((addr >> 10) & 0x3) == 0x3;
    return !write || !read_only;
}

}  // namespace

InsnEffects build_system_effects(const Insn& insn, ExecutionContext& ctx, const SemanticInputs& inputs) {
    CPU& cpu = ctx.cpu();
    CoreState& core = ctx.core();
    InsnEffects effects;
    const uint32_t csr_addr = insn.raw >> 20;
    uint64_t old = 0;

    switch (insn.funct3) {
    case 0:
        if (is_ecall(insn)) {
            const uint64_t ecall_a7 = inputs.has_ecall_a7 ? inputs.ecall_a7 : core.read_gpr(17);
            if (ecall_a7 == 93) {
                effects.control.halt = true;
                return effects;
            }
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
            effects.trap = trap_request(cause, 0);
            effects.retired = false;
            return effects;
        }
        if (is_ebreak(insn)) {
            effects.trap = trap_request(CAUSE_BREAKPOINT, inputs.pc);
            effects.retired = false;
            return effects;
        }
        if (insn.raw == 0x30200073) {
            if (core.privilege_mode() != PrivilegeMode::Machine) {
                effects.trap = illegal_instruction_trap(insn.raw);
                effects.retired = false;
                return effects;
            }
            effects.control.trap_return = TrapReturnKind::Mret;
            return effects;
        }
        if (insn.raw == 0x10200073) {
            if (core.privilege_mode() == PrivilegeMode::User) {
                effects.trap = illegal_instruction_trap(insn.raw);
                effects.retired = false;
                return effects;
            }
            effects.control.trap_return = TrapReturnKind::Sret;
            return effects;
        }
        if (is_sfence_vma(insn)) {
            if (core.privilege_mode() == PrivilegeMode::User) {
                effects.trap = illegal_instruction_trap(insn.raw);
                effects.retired = false;
                return effects;
            }
            effects.control.flush_tlb = true;
            return effects;
        }
        if (is_wfi(insn)) {
            if (core.privilege_mode() == PrivilegeMode::User) {
                effects.trap = illegal_instruction_trap(insn.raw);
                effects.retired = false;
                return effects;
            }
            return effects;
        }
        effects.trap = illegal_instruction_trap(insn.raw);
        effects.retired = false;
        return effects;
    case 1:
        if (!csr_access_allowed(ctx, csr_addr, csr_instruction_writes(insn))) {
            effects.trap = illegal_instruction_trap(insn.raw);
            effects.retired = false;
            return effects;
        }
        old = inputs.has_csrv ? inputs.csrv : csr_read(cpu, csr_addr);
        set_rd(effects, insn.rd, old);
        effects.csr_write.enable = true;
        effects.csr_write.addr = csr_addr;
        effects.csr_write.value = inputs.rs1v;
        return effects;
    case 2:
        if (!csr_access_allowed(ctx, csr_addr, csr_instruction_writes(insn))) {
            effects.trap = illegal_instruction_trap(insn.raw);
            effects.retired = false;
            return effects;
        }
        old = inputs.has_csrv ? inputs.csrv : csr_read(cpu, csr_addr);
        set_rd(effects, insn.rd, old);
        if (csr_instruction_writes(insn)) {
            effects.csr_write.enable = true;
            effects.csr_write.addr = csr_addr;
            effects.csr_write.value = old | inputs.rs1v;
        }
        return effects;
    case 3:
        if (!csr_access_allowed(ctx, csr_addr, csr_instruction_writes(insn))) {
            effects.trap = illegal_instruction_trap(insn.raw);
            effects.retired = false;
            return effects;
        }
        old = inputs.has_csrv ? inputs.csrv : csr_read(cpu, csr_addr);
        set_rd(effects, insn.rd, old);
        if (csr_instruction_writes(insn)) {
            effects.csr_write.enable = true;
            effects.csr_write.addr = csr_addr;
            effects.csr_write.value = old & ~inputs.rs1v;
        }
        return effects;
    case 5:
        if (!csr_access_allowed(ctx, csr_addr, csr_instruction_writes(insn))) {
            effects.trap = illegal_instruction_trap(insn.raw);
            effects.retired = false;
            return effects;
        }
        old = inputs.has_csrv ? inputs.csrv : csr_read(cpu, csr_addr);
        set_rd(effects, insn.rd, old);
        effects.csr_write.enable = true;
        effects.csr_write.addr = csr_addr;
        effects.csr_write.value = insn.rs1;
        return effects;
    case 6:
        if (!csr_access_allowed(ctx, csr_addr, csr_instruction_writes(insn))) {
            effects.trap = illegal_instruction_trap(insn.raw);
            effects.retired = false;
            return effects;
        }
        old = inputs.has_csrv ? inputs.csrv : csr_read(cpu, csr_addr);
        set_rd(effects, insn.rd, old);
        if (csr_instruction_writes(insn)) {
            effects.csr_write.enable = true;
            effects.csr_write.addr = csr_addr;
            effects.csr_write.value = old | insn.rs1;
        }
        return effects;
    case 7:
        if (!csr_access_allowed(ctx, csr_addr, csr_instruction_writes(insn))) {
            effects.trap = illegal_instruction_trap(insn.raw);
            effects.retired = false;
            return effects;
        }
        old = inputs.has_csrv ? inputs.csrv : csr_read(cpu, csr_addr);
        set_rd(effects, insn.rd, old);
        if (csr_instruction_writes(insn)) {
            effects.csr_write.enable = true;
            effects.csr_write.addr = csr_addr;
            effects.csr_write.value = old & ~static_cast<uint64_t>(insn.rs1);
        }
        return effects;
    default:
        effects.trap = illegal_instruction_trap(insn.raw);
        effects.retired = false;
        return effects;
    }
}
