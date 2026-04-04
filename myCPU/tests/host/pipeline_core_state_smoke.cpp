#include "pipeline_smoke_common.h"

int main() {
    PipelineCoreState state;
    const uint64_t first_sequence = state.allocate_sequence();
    state.record_retire({
        .sequence_id = first_sequence,
        .pc = kEntry,
        .raw = kAddiX1,
    });
    state.if_id.slot.valid = true;
    state.pending_fetch_fault = {.valid = true, .cause = 1, .tval = kEntry};
    state.pending_fetch_fault_pc = kEntry;
    state.redirect_pending = true;
    state.redirect_target = kTrapVector;
    state.flush(kEntry + 0x20);
    if (!expect(state.fetch_pc == kEntry + 0x20,
                "pipeline core state flush should retarget fetch pc")) {
        return 1;
    }
    if (!expect(!state.if_id.slot.valid && !state.pending_fetch_fault.valid &&
                    !state.redirect_pending,
                "pipeline core state flush should clear in-flight stage and redirect state")) {
        return 1;
    }
    if (!expect(state.last_sequence_id() == first_sequence &&
                    state.retire_trace().size() == 1,
                "pipeline core state flush should preserve sequence history")) {
        return 1;
    }

    state.reset(kEntry + 0x40);
    if (!expect(state.fetch_pc == kEntry + 0x40,
                "pipeline core state reset should install the reset fetch pc")) {
        return 1;
    }
    if (!expect(state.last_sequence_id() == 0 &&
                    state.retire_trace().empty(),
                "pipeline core state reset should clear sequence history")) {
        return 1;
    }

    state.next_if_id.slot.valid = true;
    state.next_id_ex.slot.valid = true;
    state.stalled = true;
    state.trap_flush = true;
    state.committed = true;
    state.redirect_pending = true;
    state.redirect_target = kTrapVector;
    state.begin_cycle(true);
    if (!expect(state.interrupt_serviceable_at_cycle_start &&
                    !state.next_if_id.slot.valid &&
                    !state.next_id_ex.slot.valid &&
                    !state.stalled &&
                    !state.trap_flush &&
                    !state.committed &&
                    !state.redirect_pending &&
                    state.redirect_target == 0,
                "pipeline core state begin_cycle should reset per-cycle transient state")) {
        return 1;
    }

    state.next_if_id.slot.valid = true;
    state.next_id_ex.slot.valid = true;
    state.commit_next_state();
    if (!expect(state.if_id.slot.valid && state.id_ex.slot.valid &&
                    !state.pipeline_empty(),
                "pipeline core state commit_next_state should rotate next registers into the active pipe")) {
        return 1;
    }

    return 0;
}
