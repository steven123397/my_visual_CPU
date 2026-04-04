#include "pipeline_backend.h"

#include <optional>

#include "../cpu.h"
#include "../mem/bus.h"
#include "pipeline_commit_boundary.h"

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
    if (rob_head->sequence_id != 0) {
        state_.record_retire({
            .sequence_id = rob_head->sequence_id,
            .pc = rob_head->pc,
            .raw = rob_head->raw,
            .trap = result.trap_taken,
            .redirect = rob_head->effects.control.redirect_pc ||
                        rob_head->effects.control.trap_return != TrapReturnKind::None,
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
    if (!cpu_.trap().service_pending_interrupts()) {
        return false;
    }

    state_.rollback_to_committed_state(cpu_.core());
    state_.flush(cpu_.core().pc());
    return true;
}

void PipelineBackend::commit_next_state() {
    state_.commit_next_state();
}
