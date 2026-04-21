#include "pipeline_backend.h"

#include <cstdio>
#include <optional>
#include <string>

#include "../cpu.h"

namespace {

constexpr const char* kPredictorModeName = "bimodal-2bit";

std::string hex_u32(uint32_t value) {
    char buffer[16];
    std::snprintf(buffer, sizeof(buffer), "0x%08x", value);
    return buffer;
}

std::string opcode_name(const Insn& insn) {
    if (insn.raw == 0x00000073U) {
        return "ecall";
    }
    if (insn.raw == 0x30200073U) {
        return "mret";
    }
    if (insn.raw == 0x10200073U) {
        return "sret";
    }

    switch (insn.opcode) {
    case 0x03:
        return "load";
    case 0x13:
    case 0x1B:
        return "op-imm";
    case 0x23:
        return "store";
    case 0x33:
    case 0x3B:
        return "op";
    case 0x37:
        return "lui";
    case 0x17:
        return "auipc";
    case 0x63:
        return "branch";
    case 0x67:
        return "jalr";
    case 0x6F:
        return "jal";
    case 0x73:
        return insn.funct3 == 0 ? "system" : "csr";
    default:
        return "insn";
    }
}

const char* lsq_load_state_name(LsqLoadState state) {
    switch (state) {
    case LsqLoadState::None:
        return "none";
    case LsqLoadState::BlockedByUnresolvedStore:
        return "blocked_by_unresolved_store";
    case LsqLoadState::BlockedByOverlappingStore:
        return "blocked_by_overlapping_store";
    case LsqLoadState::ReplayRequired:
        return "replay_required";
    default:
        return "unknown";
    }
}

const char* stall_reason_name(PipelineStallReason reason) {
    switch (reason) {
    case PipelineStallReason::None:
        return "none";
    case PipelineStallReason::DecodeBackpressure:
        return "decode_backpressure";
    case PipelineStallReason::SourceOperandsNotReady:
        return "source_operands_not_ready";
    case PipelineStallReason::BlockedByUnresolvedStore:
        return "blocked_by_unresolved_store";
    case PipelineStallReason::BlockedByOverlappingStore:
        return "blocked_by_overlapping_store";
    case PipelineStallReason::SerializingSystemWaitForRobHead:
        return "serializing_system_wait_for_rob_head";
    case PipelineStallReason::VectorStateBusy:
        return "vector_state_busy";
    case PipelineStallReason::NonRamLoadWaitForRobHead:
        return "non_ram_load_waiting_for_rob_head";
    case PipelineStallReason::MemoryPathBusy:
        return "memory_path_busy";
    default:
        return "unknown";
    }
}

std::string format_stage_text(const StageSlot& slot) {
    if (!slot.valid) {
        return {};
    }

    Insn insn = slot.insn;
    if (insn.raw != slot.raw) {
        decode(slot.raw, &insn);
        insn.raw = slot.raw;
    }
    return opcode_name(insn) + " " + hex_u32(slot.raw);
}

}  // namespace

PipelineBackend::PipelineBackend(CPU& cpu, Bus& bus) : cpu_(cpu), bus_(bus) {
    state_.reset(cpu_.core().pc());
    state_.reset_ooo_state(cpu_.core());
}

const char* PipelineBackend::name() const {
    return "pipeline";
}

PipelineCoreState& PipelineBackend::testing_state() {
    return state_;
}

const PipelineCoreState& PipelineBackend::testing_state() const {
    return state_;
}

BackendDebugSnapshot PipelineBackend::debug_snapshot() const {
    BackendDebugSnapshot snapshot;
    const PredictorStats predictor_stats = predictor_.stats();
    const std::optional<RobEntry> rob_head = state_.rob().peek_head();
    const std::optional<LsqEntry> lsq_head = state_.lsq().peek_oldest();
    LsqLoadStatus visible_lsq_status = state_.lsq().active_replay();
    if (visible_lsq_status.state == LsqLoadState::None) {
        visible_lsq_status = state_.lsq_observed_load_status;
    }
    snapshot.backend_name = name();
    snapshot.pipeline.if_stage = build_fetch_stage_snapshot();
    snapshot.pipeline.id_stage = build_stage_snapshot(state_.if_id.slot);
    snapshot.pipeline.ex_stage = build_stage_snapshot(state_.id_ex.slot);
    snapshot.pipeline.mem_stage = build_stage_snapshot(state_.ex_mem.slot);
    snapshot.pipeline.wb_stage = build_stage_snapshot(state_.mem_wb.slot);
    snapshot.pipeline.last_sequence_id = state_.last_sequence_id();
    snapshot.pipeline.retire_trace = state_.retire_trace();
    snapshot.pipeline.stalled = state_.stalled;
    snapshot.pipeline.stall_reason = stall_reason_name(state_.stall_reason);
    snapshot.pipeline.redirected = state_.redirect_pending;
    snapshot.pipeline.redirect_target = state_.redirect_target;
    snapshot.pipeline.pending_fetch_fault = state_.pending_fetch_fault.valid;
    snapshot.pipeline.trap_flush = state_.trap_flush;
    snapshot.pipeline.replay_flush = state_.replay_flush;
    snapshot.pipeline.committed = state_.committed;
    snapshot.pipeline.empty = state_.pipeline_empty();
    snapshot.pipeline.ooo.rob_depth = state_.rob().size();
    snapshot.pipeline.ooo.rob_head_sequence_id = rob_head.has_value() ? rob_head->sequence_id : 0;
    snapshot.pipeline.ooo.lsq_depth = state_.lsq().size();
    snapshot.pipeline.ooo.lsq_head_sequence_id = lsq_head.has_value() ? lsq_head->sequence_id : 0;
    snapshot.pipeline.ooo.lsq_load_state = lsq_load_state_name(visible_lsq_status.state);
    snapshot.pipeline.ooo.lsq_load_sequence_id = visible_lsq_status.load_sequence_id;
    snapshot.pipeline.ooo.lsq_store_sequence_id = visible_lsq_status.store_sequence_id;
    snapshot.pipeline.predictor.mode = kPredictorModeName;
    snapshot.pipeline.predictor.last_prediction_valid = last_prediction_valid_;
    snapshot.pipeline.predictor.last_prediction_taken = last_prediction_taken_;
    snapshot.pipeline.predictor.last_prediction_correct = last_prediction_correct_;
    snapshot.pipeline.predictor.last_prediction_pc = last_prediction_pc_;
    snapshot.pipeline.predictor.last_prediction_target = last_prediction_target_;
    snapshot.pipeline.predictor.last_mispredict_valid = last_mispredict_valid_;
    snapshot.pipeline.predictor.last_mispredict_pc = last_mispredict_pc_;
    snapshot.pipeline.predictor.last_mispredict_target = last_mispredict_target_;
    snapshot.pipeline.predictor.total_predictions = predictor_stats.total_predictions;
    snapshot.pipeline.predictor.correct_predictions = predictor_stats.correct_predictions;
    snapshot.pipeline.predictor.mispredictions = predictor_stats.mispredictions;
    snapshot.profile = state_.execution_profile();
    return snapshot;
}

DebugStageSnapshot PipelineBackend::build_fetch_stage_snapshot() const {
    DebugStageSnapshot snapshot;
    if (cpu_.core().halted() || state_.pending_fetch_fault.valid) {
        return snapshot;
    }

    snapshot.valid = true;
    snapshot.sequence_id = 0;
    snapshot.pc = state_.fetch_pc;
    snapshot.raw = 0;
    snapshot.text = "fetch";
    return snapshot;
}

DebugStageSnapshot PipelineBackend::build_stage_snapshot(const StageSlot& slot) const {
    DebugStageSnapshot snapshot;
    if (!slot.valid) {
        return snapshot;
    }

    snapshot.valid = true;
    snapshot.sequence_id = slot.sequence_id.value;
    snapshot.pc = slot.pc;
    snapshot.raw = slot.raw;
    snapshot.text = format_stage_text(slot);
    return snapshot;
}
