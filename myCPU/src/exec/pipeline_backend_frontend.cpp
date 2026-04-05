#include "pipeline_backend.h"

#include <optional>

#include "../cpu.h"
#include "pipeline_hazards.h"

namespace {

bool is_serializing_system_opcode(uint32_t opcode) {
    return opcode == 0x73;
}

PipelineStallReason stall_reason_for_load_status(const LsqLoadStatus& status) {
    switch (status.state) {
    case LsqLoadState::BlockedByUnresolvedStore:
        return PipelineStallReason::BlockedByUnresolvedStore;
    case LsqLoadState::BlockedByOverlappingStore:
        return PipelineStallReason::BlockedByOverlappingStore;
    default:
        return PipelineStallReason::None;
    }
}

std::optional<LsqLoadRequest> decode_load_lsq_request(const StageSlot& slot) {
    if (slot.insn.opcode != 0x03) {
        return std::nullopt;
    }

    LsqLoadRequest request{
        .sequence_id = slot.sequence_id.value,
        .rd = slot.insn.rd,
    };
    switch (slot.insn.funct3) {
    case 0:
        request.size = 1;
        request.sign_extend = true;
        return request;
    case 1:
        request.size = 2;
        request.sign_extend = true;
        return request;
    case 2:
        request.size = 4;
        request.sign_extend = true;
        return request;
    case 3:
        request.size = 8;
        return request;
    case 4:
        request.size = 1;
        return request;
    case 5:
        request.size = 2;
        return request;
    case 6:
        request.size = 4;
        return request;
    default:
        return std::nullopt;
    }
}

std::optional<LsqStoreRequest> decode_store_lsq_request(const StageSlot& slot) {
    if (slot.insn.opcode != 0x23) {
        return std::nullopt;
    }

    LsqStoreRequest request{
        .sequence_id = slot.sequence_id.value,
        .non_speculative = true,
    };
    switch (slot.insn.funct3) {
    case 0:
        request.size = 1;
        return request;
    case 1:
        request.size = 2;
        return request;
    case 2:
        request.size = 4;
        return request;
    case 3:
        request.size = 8;
        return request;
    default:
        return std::nullopt;
    }
}

}  // namespace

bool PipelineBackend::sources_ready(const StageSlot& slot) const {
    const auto phys_ready = [&](uint32_t phys) {
        return phys == 0 || state_.phys_regs().is_ready(phys);
    };

    return phys_ready(slot.rs1_phys) &&
           phys_ready(slot.rs2_phys) &&
           phys_ready(slot.ecall_a7_phys);
}

bool PipelineBackend::is_serializing_system_slot(const StageSlot& slot) const {
    if (!slot.valid || !is_serializing_system_opcode(slot.insn.opcode)) {
        return false;
    }

    const std::optional<RobEntry> rob_head = state_.rob().peek_head();
    return !rob_head.has_value() || rob_head->index.value != slot.rob_index.value;
}

void PipelineBackend::step_id() {
    if (state_.redirect_pending) {
        state_.next_if_id = {};
        return;
    }

    state_.next_if_id = state_.if_id;

    if (state_.next_id_ex.slot.valid) {
        if (state_.if_id.slot.valid) {
            state_.note_stall(PipelineStallReason::DecodeBackpressure);
        }
        return;
    }

    if (!state_.if_id.slot.valid) {
        return;
    }

    StageSlot decoded_slot = state_.if_id.slot;
    decoded_slot.insn.raw = decoded_slot.raw;
    decode(decoded_slot.raw, &decoded_slot.insn);

    decoded_slot.rs1_phys =
        pipeline_hazards::reads_rs1(decoded_slot.insn)
            ? state_.rename_map().map_source(decoded_slot.insn.rs1)
            : 0;
    decoded_slot.rs2_phys =
        pipeline_hazards::reads_rs2(decoded_slot.insn)
            ? state_.rename_map().map_source(decoded_slot.insn.rs2)
            : 0;
    decoded_slot.ecall_a7_phys =
        decoded_slot.insn.raw == 0x00000073U ? state_.rename_map().map_source(17) : 0;

    if (!sources_ready(decoded_slot)) {
        state_.note_stall(PipelineStallReason::SourceOperandsNotReady);
        return;
    }

    const auto load_request = decode_load_lsq_request(decoded_slot);
    const auto store_request = load_request.has_value()
                                   ? std::optional<LsqStoreRequest>{}
                                   : decode_store_lsq_request(decoded_slot);
    decoded_slot.rs1v = state_.phys_regs().read(decoded_slot.rs1_phys);
    decoded_slot.rs2v = state_.phys_regs().read(decoded_slot.rs2_phys);
    if (load_request.has_value()) {
        const uint64_t load_addr = decoded_slot.rs1v + static_cast<uint64_t>(decoded_slot.insn.imm);
        LsqLoadStatus load_status =
            state_.lsq().classify_load(decoded_slot.sequence_id.value,
                                       load_addr,
                                       load_request->size);
        if (load_status.blocks_issue()) {
            load_status.load_sequence_id = decoded_slot.sequence_id.value;
            state_.lsq_observed_load_status = load_status;
            state_.note_stall(stall_reason_for_load_status(load_status));
            return;
        }
    }
    if (pipeline_hazards::writes_rd(decoded_slot.insn)) {
        const RenameDestResult renamed_dest =
            state_.rename_map().rename_dest(decoded_slot.insn.rd);
        decoded_slot.rd_phys = renamed_dest.phys;
        decoded_slot.previous_rd_phys = renamed_dest.previous_phys;
        state_.phys_regs().set_pending(decoded_slot.rd_phys);
    }
    decoded_slot.rob_index = state_.rob().allocate({
        .sequence_id = decoded_slot.sequence_id.value,
        .pc = decoded_slot.pc,
        .raw = decoded_slot.raw,
        .arch_rd = static_cast<uint8_t>(
            pipeline_hazards::writes_rd(decoded_slot.insn) ? decoded_slot.insn.rd : 0),
        .phys_rd = decoded_slot.rd_phys,
        .previous_phys_rd = decoded_slot.previous_rd_phys,
        .lsq_index = decoded_slot.lsq_index,
    });
    if (store_request.has_value()) {
        decoded_slot.lsq_index = state_.lsq().enqueue_store(*store_request);
    }
    state_.next_id_ex.slot = decoded_slot;
    state_.next_if_id = {};
}

void PipelineBackend::step_if() {
    if (cpu_.core().halted() || state_.next_if_id.slot.valid ||
        state_.pending_fetch_fault.valid) {
        return;
    }

    const uint64_t fetch_pc = state_.fetch_pc;
    const AddressSpace::AccessResult fetch =
        cpu_.address_space().fetch32_result(bus_, fetch_pc);
    if (!fetch.ok) {
        state_.pending_fetch_fault = fetch.fault;
        state_.pending_fetch_fault_pc = fetch_pc;
        return;
    }

    const PredictorQueryResult prediction =
        predictor_.query(fetch_pc, static_cast<uint32_t>(fetch.value));
    state_.next_if_id.slot.valid = true;
    state_.next_if_id.slot.sequence_id.value = state_.allocate_sequence();
    state_.next_if_id.slot.pc = fetch_pc;
    state_.next_if_id.slot.raw = static_cast<uint32_t>(fetch.value);
    state_.next_if_id.slot.prediction = prediction;
    state_.fetch_pc = prediction.valid && prediction.predicted_taken
                          ? prediction.predicted_target
                          : fetch_pc + 4;
}
