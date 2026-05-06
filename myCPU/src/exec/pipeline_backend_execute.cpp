#include "pipeline_backend.h"

#include <optional>

#include "../arch/csr_file.h"
#include "../cpu.h"
#include "../isa/execution_context.h"
#include "../isa/instruction_semantics.h"
#include "../mem/bus.h"
#include "memory_ops.h"
#include "pipeline_hazards.h"
#include "vector_ops.h"
#include "floating_ops.h"

namespace {

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

PhysicalRegionInfo describe_access_region(const Bus& bus, uint64_t addr, int size) {
    return bus.describe_region(addr, size);
}

bool is_ram_access(const Bus& bus, uint64_t addr, int size) {
    return describe_access_region(bus, addr, size).kind == PhysicalRegionKind::Ram;
}

bool is_known_mmio_access(const Bus& bus, uint64_t addr, int size) {
    return describe_access_region(bus, addr, size).kind == PhysicalRegionKind::Mmio;
}

bool needs_memory_issue_delay(const Bus& bus, uint64_t addr, int size) {
    return is_ram_access(bus, addr, size) || !is_known_mmio_access(bus, addr, size);
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

}  // namespace

void PipelineBackend::publish_completed_slot(const StageSlot& slot) {
    if (!state_.next_mem_wb.slot.valid) {
        state_.next_mem_wb.slot = slot;
    }
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

void PipelineBackend::step_mem() {
    if (!state_.ex_mem.slot.valid) {
        return;
    }

    if (state_.ex_mem_cycles_remaining != 0) {
        state_.next_ex_mem = state_.ex_mem;
        state_.next_ex_mem_cycles_remaining =
            static_cast<uint8_t>(state_.ex_mem_cycles_remaining - 1);
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
            state_.lsq().forwardable_load(bus_,
                                          state_.next_mem_wb.slot.sequence_id.value,
                                          effects.mem.addr,
                                          effects.mem.size);
        if (forwarded.has_value()) {
            if (effects.mem.target == MemoryRequest::Target::Float) {
                effects.fp_write.enable = true;
                effects.fp_write.rd = effects.mem.rd;
                effects.fp_write.value = forwarded->value;
            } else {
                effects.rd_write.enable = true;
                effects.rd_write.rd = effects.mem.rd;
                effects.rd_write.value = extend_loaded_value(forwarded->value,
                                                             effects.mem.size,
                                                             effects.mem.sign_extend);
            }
        } else {
            const AddressSpace::AccessResult result =
                cpu_.address_space().load_result(bus_, effects.mem.addr, effects.mem.size);
            if (!result.ok) {
                effects.trap = result.fault;
                effects.rd_write = {};
                effects.fp_write = {};
                state_.rob().mark_ready(state_.next_mem_wb.slot.rob_index, {
                    .has_fault = true,
                    .cause = result.fault.cause,
                    .tval = result.fault.tval,
                    .effects = effects,
                    .lsq_index = state_.next_mem_wb.slot.lsq_index,
                });
                return;
            }
            if (effects.mem.target == MemoryRequest::Target::Float) {
                effects.fp_write.enable = true;
                effects.fp_write.rd = effects.mem.rd;
                effects.fp_write.value = result.value;
            } else {
                effects.rd_write.enable = true;
                effects.rd_write.rd = effects.mem.rd;
                effects.rd_write.value = extend_loaded_value(result.value,
                                                             effects.mem.size,
                                                             effects.mem.sign_extend);
            }
        }
        if (state_.next_mem_wb.slot.lsq_index.value != 0) {
            state_.lsq().mark_data_ready(
                state_.next_mem_wb.slot.lsq_index,
                effects.mem.target == MemoryRequest::Target::Float ? effects.fp_write.value : effects.rd_write.value);
            state_.lsq().mark_order_ready(state_.next_mem_wb.slot.lsq_index);
        }
        effects.mem.kind = MemoryRequest::Kind::None;
        if (effects.mem.target == MemoryRequest::Target::Integer &&
            state_.next_mem_wb.slot.rd_phys != 0) {
            state_.phys_regs().write(state_.next_mem_wb.slot.rd_phys, effects.rd_write.value);
        }
        state_.rob().mark_ready(state_.next_mem_wb.slot.rob_index, {
            .value_ready = effects.mem.target == MemoryRequest::Target::Integer &&
                           state_.next_mem_wb.slot.rd_phys != 0,
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
        state_.note_stall(PipelineStallReason::SerializingSystemWaitForRobHead);
        return;
    }
    const OlderVectorDependency vector_dependency =
        older_vector_dependency(state_.id_ex.slot);
    if (vector_dependency.blocks) {
        state_.next_id_ex.slot = state_.id_ex.slot;
        state_.note_stall(PipelineStallReason::VectorStateBusy);
        return;
    }

    ExecutionContext ctx(cpu_, bus_);
    SemanticInputs inputs;
    const PipelineForwardingSources forwarding{
        .ex_mem = &state_.ex_mem.slot,
        .mem_wb = &state_.mem_wb.slot,
    };
    inputs.pc = state_.id_ex.slot.pc;
    inputs.rs1v = pipeline_hazards::resolve_ex_operand(forwarding,
                                                       state_.id_ex.slot.rs1_phys,
                                                       state_.id_ex.slot.rs1v);
    inputs.rs2v = pipeline_hazards::resolve_ex_operand(forwarding,
                                                       state_.id_ex.slot.rs2_phys,
                                                       state_.id_ex.slot.rs2v);
    if (floating_rs1_from_fpr(state_.id_ex.slot.insn)) {
        inputs.rs1v = cpu_.core().read_fpr(state_.id_ex.slot.insn.rs1);
    }
    if (floating_rs2_from_fpr(state_.id_ex.slot.insn)) {
        inputs.rs2v = cpu_.core().read_fpr(state_.id_ex.slot.insn.rs2);
    }
    if (floating_rs3_from_fpr(state_.id_ex.slot.insn)) {
        inputs.rs3v = cpu_.core().read_fpr(state_.id_ex.slot.insn.rs3);
    }
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
    if (!completed_slot.effects.trap.valid &&
        is_non_memory_vector_alu(completed_slot.effects.vector)) {
        VectorState execute_vector_state = cpu_.core().vector();
        if (vector_dependency.vs1_valid) {
            execute_vector_state.write_reg(completed_slot.effects.vector.vs1,
                                           vector_dependency.vs1);
        }
        if (vector_dependency.vs2_valid) {
            execute_vector_state.write_reg(completed_slot.effects.vector.vs2,
                                           vector_dependency.vs2);
        }
        const VectorComputeResult vector_compute =
            compute_vector_alu_result(execute_vector_state, completed_slot.effects.vector);
        if (!vector_compute.ok) {
            completed_slot.effects.trap = vector_compute.trap;
            completed_slot.effects.retired = false;
        } else {
            completed_slot.effects.vector.result_valid = true;
            completed_slot.effects.vector.result = vector_compute.result;
        }
    }
    if (!completed_slot.effects.trap.valid) {
        switch (completed_slot.effects.mem.kind) {
        case MemoryRequest::Kind::Load:
            if (!is_ram_access(bus_,
                               completed_slot.effects.mem.addr,
                               completed_slot.effects.mem.size)) {
                const std::optional<RobEntry> rob_head = state_.rob().peek_head();
                if (!rob_head.has_value() ||
                    rob_head->index.value != completed_slot.rob_index.value) {
                    state_.next_id_ex.slot = state_.id_ex.slot;
                    state_.note_stall(PipelineStallReason::NonRamLoadWaitForRobHead);
                    return;
                }
            }
            if (state_.ex_mem.slot.valid || state_.next_ex_mem.slot.valid) {
                state_.next_id_ex.slot = state_.id_ex.slot;
                state_.note_stall(PipelineStallReason::MemoryPathBusy);
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
                needs_memory_issue_delay(bus_,
                                         completed_slot.effects.mem.addr,
                                         completed_slot.effects.mem.size)
                    ? 1
                    : 0;
            break;
        case MemoryRequest::Kind::Store:
            if (state_.ex_mem.slot.valid || state_.next_ex_mem.slot.valid) {
                state_.next_id_ex.slot = state_.id_ex.slot;
                state_.note_stall(PipelineStallReason::MemoryPathBusy);
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
                needs_memory_issue_delay(bus_,
                                         completed_slot.effects.mem.addr,
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
            .value_ready = completed_slot.rd_phys != 0 &&
                           completed_slot.effects.rd_write.enable,
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

    if (!completed_slot.effects.trap.valid &&
        is_control_flow_opcode(state_.id_ex.slot.insn.opcode)) {
        const bool actual_taken = completed_slot.effects.control.redirect_pc;
        const uint64_t actual_target =
            actual_taken ? completed_slot.effects.control.target_pc
                         : state_.id_ex.slot.pc + 4;
        const bool correct =
            prediction_matches(state_.id_ex.slot.prediction, actual_taken, actual_target);

        last_prediction_valid_ = state_.id_ex.slot.prediction.valid;
        last_prediction_taken_ = state_.id_ex.slot.prediction.predicted_taken;
        last_prediction_correct_ = correct;
        last_prediction_pc_ = state_.id_ex.slot.pc;
        last_prediction_target_ =
            state_.id_ex.slot.prediction.valid
                ? state_.id_ex.slot.prediction.predicted_target
                : 0;
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
