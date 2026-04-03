#include "pipeline_backend.h"

#include "../arch/csr_file.h"
#include "../cpu.h"
#include "../../include/platform_mmio.h"
#include "../isa/execution_context.h"
#include "../isa/instruction_semantics.h"
#include "../mem/bus.h"
#include "memory_ops.h"
#include "pipeline_commit_boundary.h"
#include "pipeline_hazards.h"

namespace {

constexpr const char* kPredictorModeName = "bimodal-2bit";

enum class CounterCsrKind : uint8_t {
    None,
    Cycle,
    Time,
    Instret,
};

CounterCsrKind classify_counter_csr(uint32_t addr) {
    switch (addr & 0xFFFU) {
    case CSR_CYCLE:
    case CSR_MCYCLE:
        return CounterCsrKind::Cycle;
    case CSR_TIME:
        return CounterCsrKind::Time;
    case CSR_INSTRET:
    case CSR_MINSTRET:
        return CounterCsrKind::Instret;
    default:
        return CounterCsrKind::None;
    }
}

bool is_control_flow_opcode(uint32_t opcode) {
    return opcode == 0x63 || opcode == 0x67 || opcode == 0x6F;
}

bool is_serializing_system_opcode(uint32_t opcode) {
    return opcode == 0x73;
}

bool is_ram_access(uint64_t addr, int size) {
    if (size <= 0) {
        return false;
    }
    const uint64_t end = addr + static_cast<uint64_t>(size);
    return addr >= MEM_BASE && end > addr && end <= MEM_BASE + MEM_SIZE;
}

bool range_contains(uint64_t addr, int size, uint64_t base, uint64_t span) {
    if (size <= 0) {
        return false;
    }
    const uint64_t end = addr + static_cast<uint64_t>(size);
    return addr >= base && end > addr && end <= base + span;
}

bool is_known_mmio_access(uint64_t addr, int size) {
    return range_contains(addr, size, UART_BASE, UART_SIZE) ||
           range_contains(addr, size, STORAGE_BASE, STORAGE_SIZE) ||
           range_contains(addr, size, CLINT_BASE, CLINT_SIZE) ||
           range_contains(addr, size, PLIC_BASE, PLIC_SIZE);
}

bool needs_memory_issue_delay(uint64_t addr, int size) {
    return is_ram_access(addr, size) || !is_known_mmio_access(addr, size);
}

bool prediction_matches(const PredictorQueryResult& prediction,
                        bool actual_taken,
                        uint64_t actual_target) {
    if (!prediction.valid) {
        return !actual_taken;
    }
    if (prediction.predicted_taken != actual_taken) {
        return false;
    }
    return !actual_taken || prediction.predicted_target == actual_target;
}

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

void PipelineBackend::publish_completed_slot(const StageSlot& slot) {
    if (!state_.next_mem_wb.slot.valid) {
        state_.next_mem_wb.slot = slot;
    }
}

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

uint64_t PipelineBackend::resolve_ex_counter_value(uint32_t addr) const {
    const CounterCsrKind kind = classify_counter_csr(addr);
    if (kind == CounterCsrKind::None) {
        return cpu_.csr().read(addr, cpu_.core());
    }

    if (kind == CounterCsrKind::Time) {
        uint64_t value = cpu_.csr().read(addr, cpu_.core());
        if (state_.ex_mem.slot.valid) {
            value += 1;
        }
        return value;
    }

    const auto ex_mem_writes_same_counter = [&]() {
        return state_.ex_mem.slot.valid &&
               state_.ex_mem.slot.effects.csr_write.enable &&
               classify_counter_csr(state_.ex_mem.slot.effects.csr_write.addr) == kind;
    };

    if (ex_mem_writes_same_counter()) {
        uint64_t value = state_.ex_mem.slot.effects.csr_write.value;
        if (state_.ex_mem.slot.effects.retired) {
            value += 1;
        }
        return value;
    }

    uint64_t value = cpu_.csr().read(addr, cpu_.core());
    if (kind == CounterCsrKind::Cycle) {
        if (state_.mem_wb.slot.valid) {
            value += 1;
        }
        if (state_.ex_mem.slot.valid) {
            value += 1;
        }
        return value;
    }

    if (state_.ex_mem.slot.valid && state_.ex_mem.slot.effects.retired) {
        value += 1;
    }
    return value;
}

uint64_t PipelineBackend::resolve_ex_csr_value(const Insn& insn) const {
    const uint32_t addr = insn.raw >> 20;
    if (classify_counter_csr(addr) != CounterCsrKind::None) {
        return resolve_ex_counter_value(addr);
    }

    CoreState projected_core = cpu_.core();
    CsrFile projected_csr = cpu_.csr();
    if (state_.ex_mem.slot.valid && state_.ex_mem.slot.effects.csr_write.enable) {
        projected_csr.write(state_.ex_mem.slot.effects.csr_write.addr,
                            state_.ex_mem.slot.effects.csr_write.value,
                            projected_core);
    }
    return projected_csr.read(addr, projected_core);
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

void PipelineBackend::step_mem() {
    if (!state_.ex_mem.slot.valid) {
        return;
    }

    if (state_.ex_mem_cycles_remaining != 0) {
        state_.next_ex_mem = state_.ex_mem;
        state_.next_ex_mem_cycles_remaining = static_cast<uint8_t>(state_.ex_mem_cycles_remaining - 1);
        return;
    }

    state_.next_mem_wb.slot = state_.ex_mem.slot;
    InsnEffects& effects = state_.next_mem_wb.slot.effects;
    if (effects.trap.valid) {
        state_.rob().mark_ready(state_.next_mem_wb.slot.rob_index, {
            .has_fault = true,
            .cause = effects.trap.cause,
            .tval = effects.trap.tval,
            .effects = effects,
            .lsq_index = state_.next_mem_wb.slot.lsq_index,
        });
        return;
    }

    switch (effects.mem.kind) {
    case MemoryRequest::Kind::Load: {
        const std::optional<LsqForwardResult> forwarded =
            state_.lsq().forwardable_load(state_.next_mem_wb.slot.sequence_id.value,
                                          effects.mem.addr,
                                          effects.mem.size);
        if (forwarded.has_value()) {
            effects.rd_write.enable = true;
            effects.rd_write.rd = effects.mem.rd;
            effects.rd_write.value = extend_loaded_value(forwarded->value,
                                                         effects.mem.size,
                                                         effects.mem.sign_extend);
        } else {
            const AddressSpace::AccessResult result =
                cpu_.address_space().load_result(bus_, effects.mem.addr, effects.mem.size);
            if (!result.ok) {
                effects.trap = result.fault;
                effects.rd_write = {};
                state_.rob().mark_ready(state_.next_mem_wb.slot.rob_index, {
                    .has_fault = true,
                    .cause = result.fault.cause,
                    .tval = result.fault.tval,
                    .effects = effects,
                    .lsq_index = state_.next_mem_wb.slot.lsq_index,
                });
                return;
            }
            effects.rd_write.enable = true;
            effects.rd_write.rd = effects.mem.rd;
            effects.rd_write.value = extend_loaded_value(result.value, effects.mem.size, effects.mem.sign_extend);
        }
        if (state_.next_mem_wb.slot.lsq_index.value != 0) {
            state_.lsq().mark_data_ready(state_.next_mem_wb.slot.lsq_index, effects.rd_write.value);
            state_.lsq().mark_order_ready(state_.next_mem_wb.slot.lsq_index);
        }
        effects.mem.kind = MemoryRequest::Kind::None;
        if (state_.next_mem_wb.slot.rd_phys != 0) {
            state_.phys_regs().write(state_.next_mem_wb.slot.rd_phys, effects.rd_write.value);
        }
        state_.rob().mark_ready(state_.next_mem_wb.slot.rob_index, {
            .value_ready = state_.next_mem_wb.slot.rd_phys != 0,
            .value = effects.rd_write.value,
            .redirect = effects.control.redirect_pc,
            .redirect_target = effects.control.target_pc,
            .effects = effects,
            .lsq_index = state_.next_mem_wb.slot.lsq_index,
        });
        return;
    }
    case MemoryRequest::Kind::Store: {
        if (state_.next_mem_wb.slot.lsq_index.value != 0) {
            state_.lsq().mark_order_ready(state_.next_mem_wb.slot.lsq_index);
        }
        state_.rob().mark_ready(state_.next_mem_wb.slot.rob_index, {
            .effects = effects,
            .lsq_index = state_.next_mem_wb.slot.lsq_index,
        });
        return;
    }
    case MemoryRequest::Kind::None:
        state_.rob().mark_ready(state_.next_mem_wb.slot.rob_index, {
            .value_ready = state_.next_mem_wb.slot.rd_phys != 0 && effects.rd_write.enable,
            .value = effects.rd_write.value,
            .has_fault = effects.trap.valid,
            .cause = effects.trap.cause,
            .tval = effects.trap.tval,
            .redirect = effects.control.redirect_pc,
            .redirect_target = effects.control.target_pc,
            .effects = effects,
            .lsq_index = state_.next_mem_wb.slot.lsq_index,
        });
        return;
    }
}

void PipelineBackend::step_ex() {
    if (!state_.id_ex.slot.valid) {
        return;
    }

    if (is_serializing_system_slot(state_.id_ex.slot)) {
        state_.next_id_ex.slot = state_.id_ex.slot;
        state_.stalled = true;
        return;
    }

    ExecutionContext ctx(cpu_, bus_);
    SemanticInputs inputs;
    const PipelineForwardingSources forwarding{
        .ex_mem = &state_.ex_mem.slot,
        .mem_wb = &state_.mem_wb.slot,
    };
    inputs.pc = state_.id_ex.slot.pc;
    inputs.rs1v = pipeline_hazards::resolve_ex_operand(forwarding, state_.id_ex.slot.rs1_phys, state_.id_ex.slot.rs1v);
    inputs.rs2v = pipeline_hazards::resolve_ex_operand(forwarding, state_.id_ex.slot.rs2_phys, state_.id_ex.slot.rs2v);
    if (state_.id_ex.slot.insn.opcode == 0x73 && state_.id_ex.slot.insn.funct3 != 0) {
        inputs.has_csrv = true;
        inputs.csrv = resolve_ex_csr_value(state_.id_ex.slot.insn);
    }
    if (state_.id_ex.slot.insn.raw == 0x00000073U) {
        inputs.has_ecall_a7 = true;
        inputs.ecall_a7 = pipeline_hazards::resolve_register_value(
            forwarding,
            state_.id_ex.slot.ecall_a7_phys,
            state_.phys_regs().read(state_.id_ex.slot.ecall_a7_phys));
    }
    StageSlot completed_slot = state_.id_ex.slot;
    completed_slot.effects = InstructionSemantics::execute(state_.id_ex.slot.insn, ctx, inputs);
    if (!completed_slot.effects.trap.valid) {
        switch (completed_slot.effects.mem.kind) {
        case MemoryRequest::Kind::Load:
            if (!is_ram_access(completed_slot.effects.mem.addr, completed_slot.effects.mem.size)) {
                const std::optional<RobEntry> rob_head = state_.rob().peek_head();
                if (!rob_head.has_value() || rob_head->index.value != completed_slot.rob_index.value) {
                    state_.next_id_ex.slot = state_.id_ex.slot;
                    state_.stalled = true;
                    return;
                }
            }
            if (state_.ex_mem.slot.valid || state_.next_ex_mem.slot.valid) {
                state_.next_id_ex.slot = state_.id_ex.slot;
                state_.stalled = true;
                return;
            }
            if (completed_slot.lsq_index.value == 0) {
                completed_slot.lsq_index = state_.lsq().enqueue_load({
                    .sequence_id = completed_slot.sequence_id.value,
                    .rd = completed_slot.effects.mem.rd,
                    .size = completed_slot.effects.mem.size,
                    .sign_extend = completed_slot.effects.mem.sign_extend,
                    .non_speculative = completed_slot.effects.mem.non_speculative,
                });
            }
            state_.lsq().mark_address_ready(completed_slot.lsq_index,
                                            completed_slot.effects.mem.addr);
            state_.next_ex_mem.slot = completed_slot;
            state_.next_ex_mem_cycles_remaining =
                needs_memory_issue_delay(completed_slot.effects.mem.addr,
                                         completed_slot.effects.mem.size)
                    ? 1
                    : 0;
            break;
        case MemoryRequest::Kind::Store:
            if (state_.ex_mem.slot.valid || state_.next_ex_mem.slot.valid) {
                state_.next_id_ex.slot = state_.id_ex.slot;
                state_.stalled = true;
                return;
            }
            if (completed_slot.lsq_index.value != 0) {
                state_.lsq().mark_address_ready(completed_slot.lsq_index,
                                                completed_slot.effects.mem.addr);
                state_.lsq().mark_data_ready(completed_slot.lsq_index,
                                             completed_slot.effects.mem.store_value);
            }
            state_.next_ex_mem.slot = completed_slot;
            state_.next_ex_mem_cycles_remaining =
                needs_memory_issue_delay(completed_slot.effects.mem.addr,
                                         completed_slot.effects.mem.size)
                    ? 1
                    : 0;
            break;
        case MemoryRequest::Kind::None:
            break;
        }
    }
    if (completed_slot.rd_phys != 0 && completed_slot.effects.rd_write.enable) {
        state_.phys_regs().write(completed_slot.rd_phys, completed_slot.effects.rd_write.value);
    }
    if (completed_slot.effects.mem.kind == MemoryRequest::Kind::None) {
        state_.rob().mark_ready(completed_slot.rob_index, {
            .value_ready = completed_slot.rd_phys != 0 && completed_slot.effects.rd_write.enable,
            .value = completed_slot.effects.rd_write.value,
            .has_fault = completed_slot.effects.trap.valid,
            .cause = completed_slot.effects.trap.cause,
            .tval = completed_slot.effects.trap.tval,
            .redirect = completed_slot.effects.control.redirect_pc,
            .redirect_target = completed_slot.effects.control.target_pc,
            .effects = completed_slot.effects,
            .lsq_index = completed_slot.lsq_index,
        });
        publish_completed_slot(completed_slot);
    }

    if (!completed_slot.effects.trap.valid && is_control_flow_opcode(state_.id_ex.slot.insn.opcode)) {
        const bool actual_taken = completed_slot.effects.control.redirect_pc;
        const uint64_t actual_target =
            actual_taken ? completed_slot.effects.control.target_pc : state_.id_ex.slot.pc + 4;
        const bool correct = prediction_matches(state_.id_ex.slot.prediction, actual_taken, actual_target);

        last_prediction_valid_ = state_.id_ex.slot.prediction.valid;
        last_prediction_taken_ = state_.id_ex.slot.prediction.predicted_taken;
        last_prediction_correct_ = correct;
        last_prediction_pc_ = state_.id_ex.slot.pc;
        last_prediction_target_ =
            state_.id_ex.slot.prediction.valid ? state_.id_ex.slot.prediction.predicted_target : 0;
        last_mispredict_valid_ = !correct;
        last_mispredict_pc_ = !correct ? state_.id_ex.slot.pc : 0;
        last_mispredict_target_ = !correct ? actual_target : 0;

        predictor_.update({
            .pc = state_.id_ex.slot.pc,
            .raw = state_.id_ex.slot.raw,
            .prediction = state_.id_ex.slot.prediction,
            .taken = actual_taken,
            .target = actual_target,
        });

        if (!correct) {
            state_.redirect_pending = true;
            state_.redirect_target = actual_target;
            state_.next_if_id = {};
            state_.next_id_ex = {};
            state_.pending_fetch_fault = {};
            state_.pending_fetch_fault_pc = 0;
            state_.fetch_pc = state_.redirect_target;
        }
    }
}

void PipelineBackend::step_id() {
    if (state_.redirect_pending) {
        state_.next_if_id = {};
        return;
    }

    state_.next_if_id = state_.if_id;

    if (state_.next_id_ex.slot.valid) {
        if (state_.if_id.slot.valid) {
            state_.stalled = true;
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
        pipeline_hazards::reads_rs1(decoded_slot.insn) ? state_.rename_map().map_source(decoded_slot.insn.rs1) : 0;
    decoded_slot.rs2_phys =
        pipeline_hazards::reads_rs2(decoded_slot.insn) ? state_.rename_map().map_source(decoded_slot.insn.rs2) : 0;
    decoded_slot.ecall_a7_phys =
        decoded_slot.insn.raw == 0x00000073U ? state_.rename_map().map_source(17) : 0;

    if (!sources_ready(decoded_slot)) {
        state_.stalled = true;
        return;
    }

    const auto load_request = decode_load_lsq_request(decoded_slot);
    const auto store_request = load_request.has_value() ? std::optional<LsqStoreRequest>{} : decode_store_lsq_request(decoded_slot);
    decoded_slot.rs1v = state_.phys_regs().read(decoded_slot.rs1_phys);
    decoded_slot.rs2v = state_.phys_regs().read(decoded_slot.rs2_phys);
    if (load_request.has_value()) {
        const uint64_t load_addr = decoded_slot.rs1v + static_cast<uint64_t>(decoded_slot.insn.imm);
        LsqLoadStatus load_status =
            state_.lsq().classify_load(decoded_slot.sequence_id.value, load_addr, load_request->size);
        if (load_status.blocks_issue()) {
            load_status.load_sequence_id = decoded_slot.sequence_id.value;
            state_.lsq_observed_load_status = load_status;
            state_.stalled = true;
            return;
        }
    }
    if (pipeline_hazards::writes_rd(decoded_slot.insn)) {
        const RenameDestResult renamed_dest = state_.rename_map().rename_dest(decoded_slot.insn.rd);
        decoded_slot.rd_phys = renamed_dest.phys;
        decoded_slot.previous_rd_phys = renamed_dest.previous_phys;
        state_.phys_regs().set_pending(decoded_slot.rd_phys);
    }
    decoded_slot.rob_index = state_.rob().allocate({
        .sequence_id = decoded_slot.sequence_id.value,
        .pc = decoded_slot.pc,
        .raw = decoded_slot.raw,
        .arch_rd = static_cast<uint8_t>(pipeline_hazards::writes_rd(decoded_slot.insn) ? decoded_slot.insn.rd : 0),
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
    if (cpu_.core().halted() || state_.next_if_id.slot.valid || state_.pending_fetch_fault.valid) {
        return;
    }

    const uint64_t fetch_pc = state_.fetch_pc;
    const AddressSpace::AccessResult fetch = cpu_.address_space().fetch32_result(bus_, fetch_pc);
    if (!fetch.ok) {
        state_.pending_fetch_fault = fetch.fault;
        state_.pending_fetch_fault_pc = fetch_pc;
        return;
    }

    const PredictorQueryResult prediction = predictor_.query(fetch_pc, static_cast<uint32_t>(fetch.value));
    state_.next_if_id.slot.valid = true;
    state_.next_if_id.slot.sequence_id.value = state_.allocate_sequence();
    state_.next_if_id.slot.pc = fetch_pc;
    state_.next_if_id.slot.raw = static_cast<uint32_t>(fetch.value);
    state_.next_if_id.slot.prediction = prediction;
    state_.fetch_pc = prediction.valid && prediction.predicted_taken ? prediction.predicted_target : fetch_pc + 4;
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
