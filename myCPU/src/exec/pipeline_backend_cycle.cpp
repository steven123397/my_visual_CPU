#include "pipeline_backend.h"

#include <optional>

#include "../arch/csr_file.h"
#include "../cpu.h"
#include "../mem/bus.h"
#include "pipeline_commit_boundary.h"

namespace {

uint64_t active_trap_cause(const CPU& cpu) {
    switch (cpu.core().privilege_mode()) {
    case PrivilegeMode::Supervisor:
        return cpu.csr().read(CSR_SCAUSE, cpu.core());
    case PrivilegeMode::Machine:
        return cpu.csr().read(CSR_MCAUSE, cpu.core());
    case PrivilegeMode::User:
    default:
        return 0;
    }
}

PhysicalRegionInfo observed_region(const Bus& bus, uint64_t paddr, uint64_t bytes) {
    const PhysicalSpanInfo span = bus.describe_span(paddr, bytes);
    if (span.ok) {
        return span.region;
    }
    return bus.describe_region(paddr, 1);
}

bool is_control_flow_raw(uint32_t raw) {
    const uint32_t opcode = raw & 0x7FU;
    return opcode == 0x63U || opcode == 0x67U || opcode == 0x6FU;
}

ExecutionMemoryObservation fault_memory_observation(uint64_t pc, uint32_t raw, bool write, uint64_t bytes) {
    return ExecutionMemoryObservation{
        .valid = true,
        .pc_valid = true,
        .pc = pc,
        .raw = raw,
        .region = make_unmapped_region_info(),
        .write = write,
        .fault = true,
        .paddr_valid = false,
        .paddr = 0,
        .bytes = bytes,
    };
}

std::optional<ExecutionMemoryObservation> make_scalar_memory_observation(CPU& cpu,
                                                                         Bus& bus,
                                                                         const LsqEntry& entry,
                                                                         uint64_t pc,
                                                                         uint32_t raw,
                                                                         bool fault) {
    const AddressSpace::TranslateResult translated =
        cpu.address_space().translate_result(bus,
                                             entry.address,
                                             entry.kind == LsqEntryKind::Load ? AccessType::Load
                                                                              : AccessType::Store,
                                             false);
    if (!translated.ok) {
        if (fault) {
            return fault_memory_observation(pc,
                                            raw,
                                            entry.kind == LsqEntryKind::Store,
                                            static_cast<uint64_t>(entry.size));
        }
        return std::nullopt;
    }

    return ExecutionMemoryObservation{
        .valid = true,
        .pc_valid = true,
        .pc = pc,
        .raw = raw,
        .region = observed_region(bus, translated.paddr, static_cast<uint64_t>(entry.size)),
        .write = entry.kind == LsqEntryKind::Store,
        .fault = fault,
        .paddr_valid = translated.ok,
        .paddr = translated.paddr,
        .bytes = static_cast<uint64_t>(entry.size),
    };
}

std::optional<ExecutionMemoryObservation> make_vector_memory_observation(CPU& cpu,
                                                                         Bus& bus,
                                                                         const VectorRequest& request,
                                                                         uint64_t pc,
                                                                         uint32_t raw,
                                                                         bool fault) {
    if (request.kind != VectorRequest::Kind::Load && request.kind != VectorRequest::Kind::Store) {
        return std::nullopt;
    }
    const uint8_t sew_bytes = request.sew_bytes != 0 ? request.sew_bytes : cpu.core().vector().sew_bytes();
    const uint8_t vl = request.vl != 0 ? request.vl : cpu.core().vector().vl();
    const uint64_t bytes = static_cast<uint64_t>(sew_bytes) * static_cast<uint64_t>(vl);
    if (bytes == 0) {
        return std::nullopt;
    }

    const AddressSpace::TranslateResult translated =
        cpu.address_space().translate_result(bus,
                                             request.addr,
                                             request.kind == VectorRequest::Kind::Load ? AccessType::Load
                                                                                       : AccessType::Store,
                                             false);
    if (!translated.ok) {
        if (fault) {
            return fault_memory_observation(pc,
                                            raw,
                                            request.kind == VectorRequest::Kind::Store,
                                            bytes);
        }
        return std::nullopt;
    }

    return ExecutionMemoryObservation{
        .valid = true,
        .pc_valid = true,
        .pc = pc,
        .raw = raw,
        .region = observed_region(bus, translated.paddr, bytes),
        .write = request.kind == VectorRequest::Kind::Store,
        .fault = fault,
        .paddr_valid = translated.ok,
        .paddr = translated.paddr,
        .bytes = bytes,
    };
}

ExecutionTrapObservation make_trap_observation(const CPU& cpu,
                                               uint64_t pc,
                                               uint32_t raw) {
    const uint64_t cause = active_trap_cause(cpu);
    return ExecutionTrapObservation{
        .pc = pc,
        .raw = raw,
        .cause = cause,
        .privilege = cpu.core().privilege_mode(),
        .interrupt = (cause >> 63) != 0,
    };
}

}  // namespace

void PipelineBackend::step() {
    cpu_.trap().sync_platform_events(bus_.tick());

    state_.begin_cycle(cpu_.trap().has_serviceable_interrupt());

    if (try_replay_flush()) {
        commit_next_state();
        cpu_.core().advance_cycle();
        return;
    }

    const bool wb_flushed = step_wb();
    bool deferred_interrupt = false;
    bool committed_fetch_fault = false;
    bool serviced_interrupt = false;
    if (state_.committed && !state_.interrupt_serviceable_at_cycle_start &&
        cpu_.trap().has_serviceable_interrupt()) {
        // A just-retired instruction changed CSR/privilege state and made an
        // interrupt newly serviceable. Flush any younger post-commit work and
        // let the interrupt become the next architectural boundary, matching
        // the functional backend even across trap returns.
        state_.rollback_to_committed_state(cpu_.core());
        state_.flush(cpu_.core().pc());
        deferred_interrupt = true;
    }

    // Keep fetch-fault delivery precise: when WB already retired an older
    // instruction this cycle, defer a younger pending fetch fault until the
    // next cycle instead of collapsing both events into one snapshot.
    if (!wb_flushed && !deferred_interrupt) {
        if (!state_.committed) {
            committed_fetch_fault = try_commit_fetch_fault();
        }
        if (!committed_fetch_fault) {
            serviced_interrupt = try_service_interrupt_at_commit_boundary();
        }
    }
    if (deferred_interrupt || committed_fetch_fault || serviced_interrupt) {
        state_.trap_flush = true;
    }
    if (!deferred_interrupt) {
        step_mem();
        step_ex();
        step_id();
        step_if();
    }

    commit_next_state();
    cpu_.core().advance_cycle();
}

bool PipelineBackend::try_replay_flush() {
    const LsqLoadStatus replay = state_.lsq().active_replay();
    if (!replay.replay_required()) {
        return false;
    }

    state_.rollback_to_committed_state(cpu_.core());
    state_.flush(cpu_.core().pc());
    state_.replay_flush = true;
    return true;
}

bool PipelineBackend::step_wb() {
    const std::optional<RobEntry> rob_head = state_.rob().peek_head();
    if (!rob_head.has_value() || !rob_head->ready) {
        return false;
    }
    const std::optional<LsqEntry> lsq_entry =
        rob_head->lsq_index.value != 0 ? state_.lsq().peek(rob_head->lsq_index)
                                       : std::optional<LsqEntry>{};

    CommitBoundaryInput commit_input{
        .pc = rob_head->pc,
        .next_pc = rob_head->pc + 4,
        .effects = rob_head->effects,
    };
    if (commit_input.effects.mem.kind == MemoryRequest::Kind::Load) {
        commit_input.effects.mem.kind = MemoryRequest::Kind::None;
    }
    commit_input.effects.rd_write = {};
    const CommitBoundaryResult result =
        apply_commit_boundary(cpu_, bus_, commit_input);
    if (lsq_entry.has_value()) {
        const std::optional<ExecutionMemoryObservation> observation =
            make_scalar_memory_observation(cpu_,
                                           bus_,
                                           *lsq_entry,
                                           rob_head->pc,
                                           rob_head->raw,
                                           result.trap_taken);
        if (observation.has_value()) {
            state_.record_memory(*observation);
        }
    } else if (const std::optional<ExecutionMemoryObservation> observation =
                   make_vector_memory_observation(cpu_,
                                                  bus_,
                                                  rob_head->effects.vector,
                                                  rob_head->pc,
                                                  rob_head->raw,
                                                  result.trap_taken);
               observation.has_value()) {
        state_.record_memory(*observation);
    }
    if (result.trap_taken) {
        state_.record_trap(make_trap_observation(cpu_, rob_head->pc, rob_head->raw));
    }
    if (result.platform_state_changed) {
        cpu_.trap().sync_platform_events(bus_.peek_events());
    }
    if (result.retired && rob_head->lsq_index.value != 0) {
        state_.lsq().retire_entry(rob_head->lsq_index);
    }
    if (result.retired && rob_head->arch_rd != 0 && rob_head->phys_rd != 0) {
        state_.rename_map().commit_dest(rob_head->arch_rd, rob_head->phys_rd);
        cpu_.core().write_gpr(rob_head->arch_rd, state_.phys_regs().read(rob_head->phys_rd));
    }
    if (result.retired && rob_head->effects.fp_write.enable) {
        cpu_.core().write_fpr(rob_head->effects.fp_write.rd, rob_head->effects.fp_write.value);
    }
    if (rob_head->sequence_id != 0) {
        state_.record_retire({
            .sequence_id = rob_head->sequence_id,
            .pc = rob_head->pc,
            .raw = rob_head->raw,
            .trap = result.trap_taken,
            .redirect = rob_head->effects.control.redirect_pc ||
                        rob_head->effects.control.trap_return != TrapReturnKind::None,
            .cycle_valid = true,
            .cycle = cpu_.core().cycle(),
            .target_pc_valid = is_control_flow_raw(rob_head->raw),
            .target_pc = result.next_pc,
        });
    }
    if (result.retired) {
        state_.rob().commit_head();
    }
    state_.committed = result.retired;
    if (result.trap_flush) {
        state_.rollback_to_committed_state(cpu_.core());
        state_.flush(cpu_.core().pc());
        state_.trap_flush = true;
        return true;
    }

    return false;
}

bool PipelineBackend::try_commit_fetch_fault() {
    if (!state_.pending_fetch_fault.valid) {
        return false;
    }
    if (state_.if_id.slot.valid || state_.id_ex.slot.valid || state_.ex_mem.slot.valid ||
        state_.rob().size() != 0) {
        return false;
    }

    cpu_.core().set_pc(state_.pending_fetch_fault_pc);
    cpu_.trap().enter_exception(state_.pending_fetch_fault.cause, state_.pending_fetch_fault.tval);
    state_.record_trap(make_trap_observation(cpu_, state_.pending_fetch_fault_pc, 0));
    state_.flush(cpu_.core().pc());
    return true;
}

bool PipelineBackend::try_service_interrupt_at_commit_boundary() {
    // A cycle-start serviceable interrupt may preempt immediately after the
    // current head retires; any younger speculative work must be squashed
    // instead of being allowed to reach the next commit.
    if (!state_.committed) {
        if (!state_.pipeline_empty()) {
            return false;
        }
    } else if (!state_.interrupt_serviceable_at_cycle_start) {
        // Match the functional backend's sequencing: an interrupt that only
        // became serviceable because the just-retired instruction updated CSR
        // state must wait until the next architectural step.
        return false;
    }
    const uint64_t interrupted_pc = cpu_.core().pc();
    if (!cpu_.trap().service_pending_interrupts()) {
        return false;
    }

    state_.record_trap(make_trap_observation(cpu_, interrupted_pc, 0));
    state_.rollback_to_committed_state(cpu_.core());
    state_.flush(cpu_.core().pc());
    return true;
}

void PipelineBackend::commit_next_state() {
    state_.commit_next_state();
}
