#include "pipeline_backend.h"

#include <optional>

#include "../cpu.h"
#include "../isa/instruction_semantics.h"
#include "../mem/bus.h"
#include "pipeline_hazards.h"
#include "vector_ops.h"

namespace {

uint8_t instruction_size(const Insn& insn) {
    return insn.size != 0 ? insn.size : 4;
}

bool is_serializing_system_insn(const Insn& insn) {
    return insn.opcode == 0x73 || is_serializing_vector_insn(insn);
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

LsqAddressInfo translate_load_address(CPU& cpu, Bus& bus, uint64_t addr, int size) {
    LsqAddressInfo info;
    const uint64_t page_offset = addr & 0xfffULL;
    info.crosses_page = page_offset + static_cast<uint64_t>(size) > 0x1000ULL;
    if (info.crosses_page) {
        return info;
    }

    const AddressSpace::TranslateResult translated =
        cpu.address_space().translate_result(bus, addr, AccessType::Load, false);
    if (!translated.ok) {
        info.translation_fault = true;
        return info;
    }
    info.paddr_valid = true;
    info.paddr = translated.paddr;
    const PhysicalSpanInfo span = bus.describe_span(translated.paddr, static_cast<uint64_t>(size));
    info.region_valid = span.ok;
    info.region = span.ok ? span.region : bus.describe_region(translated.paddr, 1);
    return info;
}

std::optional<LsqLoadRequest> decode_load_lsq_request(const StageSlot& slot) {
    const InstructionMemoryDescriptor memory = InstructionSemantics::describe_memory(slot.insn);
    if (!memory.valid || memory.kind != MemoryRequest::Kind::Load) {
        return std::nullopt;
    }

    LsqLoadRequest request{
        .sequence_id = slot.sequence_id.value,
        .rd = slot.insn.rd,
        .size = memory.size,
        .sign_extend = memory.sign_extend,
        .non_speculative = memory.non_speculative,
    };
    return request;
}

std::optional<LsqStoreRequest> decode_store_lsq_request(const StageSlot& slot) {
    const InstructionMemoryDescriptor memory = InstructionSemantics::describe_memory(slot.insn);
    if (!memory.valid || memory.kind != MemoryRequest::Kind::Store) {
        return std::nullopt;
    }

    LsqStoreRequest request{
        .sequence_id = slot.sequence_id.value,
        .size = memory.size,
        .non_speculative = memory.non_speculative,
    };
    return request;
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
    if (!slot.valid || !is_serializing_system_insn(slot.insn)) {
        return false;
    }

    const std::optional<RobEntry> rob_head = state_.rob().peek_head();
    return !rob_head.has_value() || rob_head->index.value != slot.rob_index.value;
}

OlderVectorDependency PipelineBackend::older_vector_dependency(const StageSlot& slot) const {
    if (!slot.valid || !is_non_memory_vector_alu_insn(slot.insn)) {
        return {};
    }
    return state_.rob().inspect_older_vector_dependencies(slot.sequence_id.value,
                                                          slot.insn.rs1,
                                                          slot.insn.rs2);
}

bool PipelineBackend::vector_state_busy(const StageSlot& slot) const {
    return older_vector_dependency(slot).blocks;
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
    decoded_slot.insn_size = instruction_size(decoded_slot.insn);

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

    const InstructionRegisterDescriptor register_descriptor =
        InstructionSemantics::describe_registers(decoded_slot.insn);
    const uint8_t fp_rs1 = register_descriptor.rs1 == RegisterOperandKind::Fpr ? decoded_slot.insn.rs1 : 0xffU;
    const uint8_t fp_rs2 = register_descriptor.rs2 == RegisterOperandKind::Fpr ? decoded_slot.insn.rs2 : 0xffU;
    const uint8_t fp_rs3 = register_descriptor.rs3 == RegisterOperandKind::Fpr ? decoded_slot.insn.rs3 : 0xffU;
    if (state_.rob().has_older_fp_pending(decoded_slot.sequence_id.value, fp_rs1, fp_rs2, fp_rs3)) {
        state_.note_stall(PipelineStallReason::SourceOperandsNotReady);
        return;
    }

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
        const LsqAddressInfo load_info =
            translate_load_address(cpu_, bus_, load_addr, load_request->size);
        LsqLoadStatus load_status =
            state_.lsq().classify_load(decoded_slot.sequence_id.value,
                                       load_addr,
                                       load_request->size,
                                       load_info);
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
        .insn_size = decoded_slot.insn_size,
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
    const AddressSpace::AccessResult first_half =
        cpu_.address_space().fetch16_result(bus_, fetch_pc);
    if (!first_half.ok) {
        state_.pending_fetch_fault = first_half.fault;
        state_.pending_fetch_fault_pc = fetch_pc;
        return;
    }

    uint32_t raw = static_cast<uint32_t>(first_half.value & 0xffffU);
    if ((raw & 0x3U) == 0x3U) {
        const AddressSpace::AccessResult full_word =
            cpu_.address_space().fetch32_result(bus_, fetch_pc);
        if (!full_word.ok) {
            state_.pending_fetch_fault = full_word.fault;
            state_.pending_fetch_fault_pc = fetch_pc;
            return;
        }
        raw = static_cast<uint32_t>(full_word.value);
    }

    Insn fetched_insn{};
    decode(raw, &fetched_insn);
    fetched_insn.raw = raw;
    const uint8_t size = instruction_size(fetched_insn);
    const PredictorQueryResult prediction =
        predictor_.query(fetch_pc, raw);
    state_.next_if_id.slot.valid = true;
    state_.next_if_id.slot.sequence_id.value = state_.allocate_sequence();
    state_.next_if_id.slot.pc = fetch_pc;
    state_.next_if_id.slot.raw = raw;
    state_.next_if_id.slot.insn_size = size;
    state_.next_if_id.slot.prediction = prediction;
    state_.fetch_pc = prediction.valid && prediction.predicted_taken
                          ? prediction.predicted_target
                          : fetch_pc + size;
}
