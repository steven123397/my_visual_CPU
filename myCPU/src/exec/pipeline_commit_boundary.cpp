#include "pipeline_commit_boundary.h"

#include "../isa/atomic_contract.h"
#include "memory_ops.h"
#include "vector_ops.h"

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

void mark_floating_state_dirty(CPU& cpu) {
    const uint64_t mstatus = cpu.csr().read(CSR_MSTATUS, cpu.core());
    cpu.csr().write(CSR_MSTATUS, (mstatus & ~MSTATUS_FS_MASK) | MSTATUS_FS_DIRTY, cpu.core());
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

    if (effects.mem.kind == MemoryRequest::Kind::Load &&
        !effects.rd_write.enable &&
        !effects.fp_write.enable) {
        const AddressSpace::AccessResult access =
            cpu.address_space().load_result(bus, effects.mem.addr, effects.mem.size);
        if (!access.ok) {
            return enter_precise_trap(access.fault);
        }
        if (effects.mem.target == MemoryRequest::Target::Float) {
            effects.fp_write.enable = true;
            effects.fp_write.rd = effects.mem.rd;
            effects.fp_write.value = access.value;
        } else {
            effects.rd_write.enable = true;
            effects.rd_write.rd = effects.mem.rd;
            effects.rd_write.value = extend_loaded_value(access.value,
                                                         effects.mem.size,
                                                         effects.mem.sign_extend);
        }
    } else if (effects.mem.kind == MemoryRequest::Kind::Store) {
        const AddressSpace::AccessResult access =
            cpu.address_space().store_result(bus,
                                             effects.mem.addr,
                                             effects.mem.store_value,
                                             effects.mem.size);
        if (!access.ok) {
            return enter_precise_trap(access.fault);
        }
        invalidate_reservation_for_store(cpu, bus, effects.mem.addr, effects.mem.size);
        result.platform_state_changed = bus.last_access().valid && bus.last_access().mmio;
    }

    if (effects.atomic.kind != AtomicRequest::Kind::None) {
        const AtomicApplyResult atomic_result =
            apply_atomic_request(cpu, bus, effects.atomic);
        result.atomic_memory_observed = atomic_result.memory_observed;
        result.atomic_write_observed = atomic_result.write_observed;
        result.atomic_paddr_valid = atomic_result.paddr_valid;
        result.atomic_paddr = atomic_result.paddr;
        result.atomic_bytes = atomic_result.bytes;
        if (!atomic_result.ok) {
            CommitBoundaryResult trap = enter_precise_trap(atomic_result.trap);
            trap.atomic_memory_observed = atomic_result.memory_observed;
            trap.atomic_write_observed = atomic_result.write_observed;
            trap.atomic_paddr_valid = atomic_result.paddr_valid;
            trap.atomic_paddr = atomic_result.paddr;
            trap.atomic_bytes = atomic_result.bytes;
            return trap;
        }
        effects.rd_write = atomic_result.rd_write;
        result.platform_state_changed |= atomic_result.platform_state_changed;
    }

    if (effects.vector.kind != VectorRequest::Kind::None) {
        const VectorApplyResult vector_result =
            apply_vector_request(cpu, bus, effects.vector);
        if (!vector_result.ok) {
            return enter_precise_trap(vector_result.trap);
        }
        result.platform_state_changed |= vector_result.platform_state_changed;
    }

    if (effects.csr_write.enable) {
        cpu.csr().write(effects.csr_write.addr, effects.csr_write.value, cpu.core());
    }
    if (effects.rd_write.enable) {
        cpu.core().write_gpr(effects.rd_write.rd, effects.rd_write.value);
    }
    if (effects.fp_write.enable) {
        cpu.core().write_fpr(effects.fp_write.rd, effects.fp_write.value);
    }
    if (effects.floating_state_touched) {
        mark_floating_state_dirty(cpu);
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
