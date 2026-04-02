#include "pipeline_commit_boundary.h"

#include "memory_ops.h"

#include "../cpu.h"
#include "../mem/bus.h"

namespace {

CommitBoundaryResult trap_result(const CPU& cpu) {
    return CommitBoundaryResult{
        .retired = false,
        .trap_taken = true,
        .trap_flush = true,
        .redirect = true,
        .next_pc = cpu.core().pc(),
    };
}

}  // namespace

CommitBoundaryResult apply_commit_boundary(CPU& cpu,
                                           Bus& bus,
                                           const CommitBoundaryInput& input) {
    CommitBoundaryResult result;
    result.next_pc = input.next_pc;

    auto enter_precise_trap = [&](const TrapRequest& trap) {
        cpu.core().set_pc(input.pc);
        cpu.trap().enter_exception(trap.cause, trap.tval);
        return trap_result(cpu);
    };

    InsnEffects effects = input.effects;
    if (effects.trap.valid) {
        return enter_precise_trap(effects.trap);
    }

    if (effects.mem.kind == MemoryRequest::Kind::Load && !effects.rd_write.enable) {
        const AddressSpace::AccessResult access =
            cpu.address_space().load_result(bus, effects.mem.addr, effects.mem.size);
        if (!access.ok) {
            return enter_precise_trap(access.fault);
        }
        effects.rd_write.enable = true;
        effects.rd_write.rd = effects.mem.rd;
        effects.rd_write.value = extend_loaded_value(access.value,
                                                     effects.mem.size,
                                                     effects.mem.sign_extend);
    } else if (effects.mem.kind == MemoryRequest::Kind::Store) {
        const AddressSpace::AccessResult access =
            cpu.address_space().store_result(bus,
                                             effects.mem.addr,
                                             effects.mem.store_value,
                                             effects.mem.size);
        if (!access.ok) {
            return enter_precise_trap(access.fault);
        }
        result.platform_state_changed = bus.last_access().valid && bus.last_access().mmio;
    }

    if (effects.csr_write.enable) {
        cpu.csr().write(effects.csr_write.addr, effects.csr_write.value, cpu.core());
    }
    if (effects.rd_write.enable) {
        cpu.core().write_gpr(effects.rd_write.rd, effects.rd_write.value);
    }
    if (effects.control.flush_tlb) {
        cpu.address_space().flush_tlb();
    }
    if (effects.control.halt) {
        cpu.core().set_halted(true);
    }

    switch (effects.control.trap_return) {
    case TrapReturnKind::Mret:
        cpu.trap().return_from_mret();
        result.redirect = true;
        result.trap_flush = true;
        break;
    case TrapReturnKind::Sret:
        cpu.trap().return_from_sret();
        result.redirect = true;
        result.trap_flush = true;
        break;
    case TrapReturnKind::None:
        cpu.core().set_pc(effects.control.redirect_pc ? effects.control.target_pc : input.next_pc);
        result.redirect = effects.control.redirect_pc;
        break;
    }

    if (effects.retired) {
        cpu.core().advance_instret();
        result.retired = true;
    }

    if (effects.control.halt) {
        result.trap_flush = true;
    }

    result.next_pc = cpu.core().pc();
    return result;
}
