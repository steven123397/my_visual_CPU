#include "pipeline_backend.h"

#include "../arch/csr_file.h"
#include "../cpu.h"
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
}

void PipelineBackend::step() {
    cpu_.trap().sync_platform_events(bus_.tick());

    state_.begin_cycle(cpu_.trap().has_serviceable_interrupt());

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
    if (!state_.mem_wb.slot.valid) {
        return false;
    }

    const CommitBoundaryResult result =
        apply_commit_boundary(cpu_,
                              bus_,
                              CommitBoundaryInput{
                                  .pc = state_.mem_wb.slot.pc,
                                  .next_pc = state_.mem_wb.slot.pc + 4,
                                  .effects = state_.mem_wb.slot.effects,
                              });
    if (result.platform_state_changed) {
        cpu_.trap().sync_platform_events(bus_.peek_events());
    }
    if (state_.mem_wb.slot.valid && state_.mem_wb.slot.sequence_id.value != 0) {
        state_.record_retire({
            .sequence_id = state_.mem_wb.slot.sequence_id.value,
            .pc = state_.mem_wb.slot.pc,
            .raw = state_.mem_wb.slot.raw,
            .trap = result.trap_taken,
            .redirect = state_.mem_wb.slot.effects.control.redirect_pc ||
                        state_.mem_wb.slot.effects.control.trap_return != TrapReturnKind::None,
        });
    }
    state_.committed = result.retired;
    if (result.trap_flush) {
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
    if (state_.if_id.slot.valid || state_.id_ex.slot.valid || state_.ex_mem.slot.valid) {
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

    state_.next_mem_wb.slot = state_.ex_mem.slot;
    InsnEffects& effects = state_.next_mem_wb.slot.effects;
    if (effects.trap.valid) {
        return;
    }

    switch (effects.mem.kind) {
    case MemoryRequest::Kind::Load: {
        const AddressSpace::AccessResult result =
            cpu_.address_space().load_result(bus_, effects.mem.addr, effects.mem.size);
        if (!result.ok) {
            effects.trap = result.fault;
            effects.rd_write = {};
            return;
        }
        effects.rd_write.enable = true;
        effects.rd_write.rd = effects.mem.rd;
        effects.rd_write.value = extend_loaded_value(result.value, effects.mem.size, effects.mem.sign_extend);
        return;
    }
    case MemoryRequest::Kind::Store: {
        const AddressSpace::AccessResult result =
            cpu_.address_space().store_result(bus_, effects.mem.addr, effects.mem.store_value, effects.mem.size);
        if (!result.ok) {
            effects.trap = result.fault;
        }
        effects.mem.kind = MemoryRequest::Kind::None;
        return;
    }
    case MemoryRequest::Kind::None:
        return;
    }
}

void PipelineBackend::step_ex() {
    if (!state_.id_ex.slot.valid) {
        return;
    }

    state_.next_ex_mem.slot = state_.id_ex.slot;

    ExecutionContext ctx(cpu_, bus_);
    SemanticInputs inputs;
    const PipelineForwardingSources forwarding{
        .ex_mem = &state_.ex_mem.slot,
        .mem_wb = &state_.mem_wb.slot,
    };
    inputs.pc = state_.id_ex.slot.pc;
    inputs.rs1v = pipeline_hazards::resolve_ex_operand(
        forwarding, state_.id_ex.slot.insn, true, state_.id_ex.slot.rs1v);
    inputs.rs2v = pipeline_hazards::resolve_ex_operand(
        forwarding, state_.id_ex.slot.insn, false, state_.id_ex.slot.rs2v);
    if (state_.id_ex.slot.insn.opcode == 0x73 && state_.id_ex.slot.insn.funct3 != 0) {
        inputs.has_csrv = true;
        inputs.csrv = resolve_ex_csr_value(state_.id_ex.slot.insn);
    }
    if (state_.id_ex.slot.insn.raw == 0x00000073U) {
        uint64_t a7v = cpu_.core().read_gpr(17);
        const PipelineForwardingSources ecall_forwarding{
            .ex_mem = &state_.ex_mem.slot,
            .mem_wb = &state_.mem_wb.slot,
        };
        a7v = pipeline_hazards::resolve_register_value(ecall_forwarding, 17, a7v);
        inputs.has_ecall_a7 = true;
        inputs.ecall_a7 = a7v;
    }
    state_.next_ex_mem.slot.effects = InstructionSemantics::execute(state_.id_ex.slot.insn, ctx, inputs);

    if (!state_.next_ex_mem.slot.effects.trap.valid && is_control_flow_opcode(state_.id_ex.slot.insn.opcode)) {
        const bool actual_taken = state_.next_ex_mem.slot.effects.control.redirect_pc;
        const uint64_t actual_target =
            actual_taken ? state_.next_ex_mem.slot.effects.control.target_pc : state_.id_ex.slot.pc + 4;
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

    if (!state_.if_id.slot.valid) {
        return;
    }

    StageSlot decoded_slot = state_.if_id.slot;
    decoded_slot.insn.raw = decoded_slot.raw;
    decode(decoded_slot.raw, &decoded_slot.insn);

    if (pipeline_hazards::has_decode_hazard(state_.id_ex.slot, decoded_slot.insn)) {
        state_.stalled = true;
        return;
    }

    decoded_slot.rs1v = cpu_.core().read_gpr(decoded_slot.insn.rs1);
    decoded_slot.rs2v = cpu_.core().read_gpr(decoded_slot.insn.rs2);
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
    if (!state_.mem_wb.slot.valid && !state_.pipeline_empty()) {
        return false;
    }
    // Match the functional backend's sequencing: an interrupt that only
    // became serviceable because the just-retired instruction updated CSR
    // state must wait until the next architectural step. Older pending
    // interrupts may still preempt here to avoid starvation in tight loops.
    if (state_.committed && !state_.interrupt_serviceable_at_cycle_start) {
        return false;
    }
    if (!cpu_.trap().service_pending_interrupts()) {
        return false;
    }

    state_.flush(cpu_.core().pc());
    return true;
}

void PipelineBackend::commit_next_state() {
    state_.commit_next_state();
}

const char* PipelineBackend::name() const {
    return "pipeline";
}

BackendDebugSnapshot PipelineBackend::debug_snapshot() const {
    BackendDebugSnapshot snapshot;
    const PredictorStats predictor_stats = predictor_.stats();
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
    snapshot.pipeline.committed = state_.committed;
    snapshot.pipeline.empty = state_.pipeline_empty();
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
