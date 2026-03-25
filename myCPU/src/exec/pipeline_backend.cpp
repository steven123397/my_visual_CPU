#include "pipeline_backend.h"

#include <cstdio>

#include "../cpu.h"
#include "memory_ops.h"
#include "../isa/execution_context.h"
#include "../isa/instruction_semantics.h"
#include "../mem/bus.h"

PipelineBackend::PipelineBackend(CPU& cpu, Bus& bus) : cpu_(cpu), bus_(bus) {
    reset_stage_state();
}

void PipelineBackend::step() {
    cpu_.trap().sync_platform_events(bus_.tick());

    next_if_id_ = {};
    next_id_ex_ = {};
    next_ex_mem_ = {};
    next_mem_wb_ = {};
    last_cycle_stalled_ = false;
    redirect_pending_ = false;
    redirect_target_ = 0;
    last_cycle_redirected_ = false;
    last_cycle_trap_flush_ = false;
    last_cycle_committed_ = false;

    const bool wb_flushed = step_wb();
    if (!wb_flushed) {
        if (!try_commit_fetch_fault()) {
            try_service_interrupt_at_commit_boundary();
        }
    }
    step_mem();
    step_ex();
    step_id();
    step_if();

    commit_next_state();
    cpu_.core().advance_cycle();
}

bool PipelineBackend::pipeline_empty() const {
    return !if_id_.slot.valid && !id_ex_.slot.valid && !ex_mem_.slot.valid && !mem_wb_.slot.valid;
}

bool PipelineBackend::reads_rs1(const Insn& insn) const {
    switch (insn.opcode) {
    case 0x13:
    case 0x1B:
    case 0x33:
    case 0x3B:
    case 0x67:
    case 0x63:
    case 0x03:
    case 0x23:
        return true;
    case 0x73:
        switch (insn.funct3) {
        case 1:
        case 2:
        case 3:
            return true;
        default:
            return false;
        }
    default:
        return false;
    }
}

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

bool wb_commit_needs_flush(const InsnEffects& effects) {
    return effects.control.halt || effects.control.trap_return != TrapReturnKind::None;
}

const char* opcode_name(const Insn& insn) {
    switch (insn.opcode) {
    case 0x03:
        switch (insn.funct3) {
        case 0x0:
            return "lb";
        case 0x1:
            return "lh";
        case 0x2:
            return "lw";
        case 0x3:
            return "ld";
        case 0x4:
            return "lbu";
        case 0x5:
            return "lhu";
        case 0x6:
            return "lwu";
        default:
            return "load";
        }
    case 0x0F:
        return "fence";
    case 0x13:
        switch (insn.funct3) {
        case 0x0:
            return "addi";
        case 0x1:
            return "slli";
        case 0x2:
            return "slti";
        case 0x3:
            return "sltiu";
        case 0x4:
            return "xori";
        case 0x5:
            return ((insn.raw >> 30) & 1U) ? "srai" : "srli";
        case 0x6:
            return "ori";
        case 0x7:
            return "andi";
        default:
            return "op-imm";
        }
    case 0x17:
        return "auipc";
    case 0x1B:
        return "op-imm-32";
    case 0x23:
        switch (insn.funct3) {
        case 0x0:
            return "sb";
        case 0x1:
            return "sh";
        case 0x2:
            return "sw";
        case 0x3:
            return "sd";
        default:
            return "store";
        }
    case 0x33:
        if (insn.funct7 == 0x01) {
            switch (insn.funct3) {
            case 0x0:
                return "mul";
            case 0x4:
                return "div";
            case 0x6:
                return "rem";
            default:
                return "op";
            }
        }
        switch (insn.funct3) {
        case 0x0:
            return ((insn.raw >> 30) & 1U) ? "sub" : "add";
        case 0x1:
            return "sll";
        case 0x2:
            return "slt";
        case 0x3:
            return "sltu";
        case 0x4:
            return "xor";
        case 0x5:
            return ((insn.raw >> 30) & 1U) ? "sra" : "srl";
        case 0x6:
            return "or";
        case 0x7:
            return "and";
        default:
            return "op";
        }
    case 0x37:
        return "lui";
    case 0x3B:
        return "op-32";
    case 0x63:
        switch (insn.funct3) {
        case 0x0:
            return "beq";
        case 0x1:
            return "bne";
        case 0x4:
            return "blt";
        case 0x5:
            return "bge";
        case 0x6:
            return "bltu";
        case 0x7:
            return "bgeu";
        default:
            return "branch";
        }
    case 0x67:
        return "jalr";
    case 0x6F:
        return "jal";
    case 0x73:
        if (insn.raw == 0x00000073U) {
            return "ecall";
        }
        if (insn.raw == 0x00100073U) {
            return "ebreak";
        }
        if (insn.raw == 0x30200073U) {
            return "mret";
        }
        if (insn.raw == 0x10200073U) {
            return "sret";
        }
        switch (insn.funct3) {
        case 0x1:
            return "csrrw";
        case 0x2:
            return "csrrs";
        case 0x3:
            return "csrrc";
        case 0x5:
            return "csrrwi";
        case 0x6:
            return "csrrsi";
        case 0x7:
            return "csrrci";
        default:
            return "system";
        }
    default:
        return "unknown";
    }
}

DebugStageSnapshot make_stage_snapshot(const StageSlot& slot) {
    DebugStageSnapshot snapshot;
    snapshot.valid = slot.valid;
    snapshot.pc = slot.pc;
    snapshot.raw = slot.raw;
    if (!slot.valid) {
        return snapshot;
    }

    Insn insn = slot.insn;
    if (insn.raw == 0 && slot.raw != 0) {
        insn.raw = slot.raw;
        decode(slot.raw, &insn);
    }

    char buffer[64];
    std::snprintf(buffer, sizeof(buffer), "%s 0x%08x", opcode_name(insn), slot.raw);
    snapshot.text = buffer;
    return snapshot;
}

}  // namespace

bool PipelineBackend::reads_rs2(const Insn& insn) const {
    switch (insn.opcode) {
    case 0x33:
    case 0x3B:
    case 0x63:
    case 0x23:
        return true;
    default:
        return false;
    }
}

uint8_t PipelineBackend::inflight_rd(const StageSlot& slot) const {
    if (!slot.valid) {
        return 0;
    }
    if (!slot.effects.rd_write.enable) {
        switch (slot.insn.opcode) {
        case 0x37:
        case 0x17:
        case 0x13:
        case 0x1B:
        case 0x33:
        case 0x3B:
        case 0x6F:
        case 0x67:
        case 0x03:
            return slot.insn.rd;
        default:
            return 0;
        }
    }
    if (slot.effects.mem.kind == MemoryRequest::Kind::Load) {
        return slot.effects.mem.rd;
    }
    return slot.effects.rd_write.rd;
}

bool PipelineBackend::has_decode_hazard(const Insn& insn) const {
    const uint8_t rs1 = reads_rs1(insn) ? insn.rs1 : 0;
    const uint8_t rs2 = reads_rs2(insn) ? insn.rs2 : 0;
    if (!is_load_slot(id_ex_.slot)) {
        return false;
    }

    const uint8_t id_ex_rd = inflight_rd(id_ex_.slot);
    if (id_ex_rd == 0) {
        return false;
    }

    if (rs1 != 0 && rs1 == id_ex_rd) {
        return true;
    }
    if (rs2 != 0 && rs2 == id_ex_rd) {
        return true;
    }
    return false;
}

bool PipelineBackend::is_load_slot(const StageSlot& slot) const {
    return slot.valid && slot.insn.opcode == 0x03;
}

bool PipelineBackend::forward_operand_from_slot(const StageSlot& slot, uint8_t rs, uint64_t& value) const {
    if (!slot.valid || rs == 0) {
        return false;
    }

    if (slot.effects.rd_write.enable && slot.effects.rd_write.rd == rs) {
        value = slot.effects.rd_write.value;
        return true;
    }

    return false;
}

uint64_t PipelineBackend::resolve_ex_operand(const Insn& insn, bool use_rs1, uint64_t latched_value) const {
    const uint8_t rs = use_rs1 ? insn.rs1 : insn.rs2;
    uint64_t value = latched_value;

    if (forward_operand_from_slot(ex_mem_.slot, rs, value)) {
        return value;
    }
    if (forward_operand_from_slot(mem_wb_.slot, rs, value)) {
        return value;
    }
    return value;
}

uint64_t PipelineBackend::resolve_ex_counter_value(uint32_t addr) const {
    const CounterCsrKind kind = classify_counter_csr(addr);
    if (kind == CounterCsrKind::None) {
        return cpu_.csr().read(addr, cpu_.core());
    }

    if (kind == CounterCsrKind::Time) {
        uint64_t value = cpu_.csr().read(addr, cpu_.core());
        if (ex_mem_.slot.valid) {
            value += 1;
        }
        return value;
    }

    const auto ex_mem_writes_same_counter = [&]() {
        return ex_mem_.slot.valid &&
            ex_mem_.slot.effects.csr_write.enable &&
            classify_counter_csr(ex_mem_.slot.effects.csr_write.addr) == kind;
    };

    if (ex_mem_writes_same_counter()) {
        uint64_t value = ex_mem_.slot.effects.csr_write.value;
        if (ex_mem_.slot.effects.retired) {
            value += 1;
        }
        return value;
    }

    uint64_t value = cpu_.csr().read(addr, cpu_.core());
    if (kind == CounterCsrKind::Cycle) {
        if (mem_wb_.slot.valid) {
            value += 1;
        }
        if (ex_mem_.slot.valid) {
            value += 1;
        }
        return value;
    }

    if (ex_mem_.slot.valid && ex_mem_.slot.effects.retired) {
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
    if (ex_mem_.slot.valid && ex_mem_.slot.effects.csr_write.enable) {
        projected_csr.write(ex_mem_.slot.effects.csr_write.addr, ex_mem_.slot.effects.csr_write.value, projected_core);
    }
    return projected_csr.read(addr, projected_core);
}

void PipelineBackend::reset_stage_state() {
    if_id_ = {};
    id_ex_ = {};
    ex_mem_ = {};
    mem_wb_ = {};
    next_if_id_ = {};
    next_id_ex_ = {};
    next_ex_mem_ = {};
    next_mem_wb_ = {};
    fetch_pc_ = cpu_.core().pc();
    pending_fetch_fault_ = {};
    pending_fetch_fault_pc_ = 0;
}

bool PipelineBackend::step_wb() {
    if (!mem_wb_.slot.valid) {
        return false;
    }

    if (mem_wb_.slot.effects.trap.valid) {
        cpu_.trap().enter_exception(mem_wb_.slot.effects.trap.cause, mem_wb_.slot.effects.trap.tval);
        reset_stage_state();
        fetch_pc_ = cpu_.core().pc();
        return true;
    }

    if (mem_wb_.slot.effects.csr_write.enable) {
        cpu_.csr().write(
            mem_wb_.slot.effects.csr_write.addr, mem_wb_.slot.effects.csr_write.value, cpu_.core());
    }
    if (mem_wb_.slot.effects.rd_write.enable) {
        cpu_.core().write_gpr(mem_wb_.slot.effects.rd_write.rd, mem_wb_.slot.effects.rd_write.value);
    }
    if (mem_wb_.slot.effects.control.flush_tlb) {
        cpu_.address_space().flush_tlb();
    }
    if (mem_wb_.slot.effects.control.halt) {
        cpu_.core().set_halted(true);
    }

    switch (mem_wb_.slot.effects.control.trap_return) {
    case TrapReturnKind::Mret:
        cpu_.trap().return_from_mret();
        break;
    case TrapReturnKind::Sret:
        cpu_.trap().return_from_sret();
        break;
    case TrapReturnKind::None:
        cpu_.core().set_pc(
            mem_wb_.slot.effects.control.redirect_pc ? mem_wb_.slot.effects.control.target_pc : mem_wb_.slot.pc + 4);
        break;
    }
    if (mem_wb_.slot.effects.retired) {
        cpu_.core().advance_instret();
        last_cycle_committed_ = true;
    }

    if (wb_commit_needs_flush(mem_wb_.slot.effects)) {
        last_cycle_trap_flush_ = true;
        reset_stage_state();
        fetch_pc_ = cpu_.core().pc();
        return true;
    }

    return false;
}

bool PipelineBackend::try_commit_fetch_fault() {
    if (!pending_fetch_fault_.valid) {
        return false;
    }
    if (if_id_.slot.valid || id_ex_.slot.valid || ex_mem_.slot.valid) {
        return false;
    }

    cpu_.core().set_pc(pending_fetch_fault_pc_);
    cpu_.trap().enter_exception(pending_fetch_fault_.cause, pending_fetch_fault_.tval);
    reset_stage_state();
    fetch_pc_ = cpu_.core().pc();
    return true;
}

void PipelineBackend::step_mem() {
    if (!ex_mem_.slot.valid) {
        return;
    }

    next_mem_wb_.slot = ex_mem_.slot;
    InsnEffects& effects = next_mem_wb_.slot.effects;
    if (effects.trap.valid) {
        return;
    }

    switch (effects.mem.kind) {
    case MemoryRequest::Kind::Load: {
        const AddressSpace::AccessResult result = cpu_.address_space().load_result(bus_, effects.mem.addr, effects.mem.size);
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
        return;
    }
    case MemoryRequest::Kind::None:
        return;
    }
}

void PipelineBackend::step_ex() {
    if (!id_ex_.slot.valid) {
        return;
    }

    next_ex_mem_.slot = id_ex_.slot;

    ExecutionContext ctx(cpu_, bus_);
    SemanticInputs inputs;
    inputs.pc = id_ex_.slot.pc;
    inputs.rs1v = resolve_ex_operand(id_ex_.slot.insn, true, id_ex_.slot.rs1v);
    inputs.rs2v = resolve_ex_operand(id_ex_.slot.insn, false, id_ex_.slot.rs2v);
    if (id_ex_.slot.insn.opcode == 0x73 && id_ex_.slot.insn.funct3 != 0) {
        inputs.has_csrv = true;
        inputs.csrv = resolve_ex_csr_value(id_ex_.slot.insn);
    }
    if (id_ex_.slot.insn.raw == 0x00000073U) {
        uint64_t a7v = cpu_.core().read_gpr(17);
        if (!forward_operand_from_slot(ex_mem_.slot, 17, a7v)) {
            forward_operand_from_slot(mem_wb_.slot, 17, a7v);
        }
        inputs.has_ecall_a7 = true;
        inputs.ecall_a7 = a7v;
    }
    next_ex_mem_.slot.effects = InstructionSemantics::execute(id_ex_.slot.insn, ctx, inputs);

    if (!next_ex_mem_.slot.effects.trap.valid && next_ex_mem_.slot.effects.control.redirect_pc) {
        redirect_pending_ = true;
        redirect_target_ = next_ex_mem_.slot.effects.control.target_pc;
        last_cycle_redirected_ = true;
        next_if_id_ = {};
        next_id_ex_ = {};
        fetch_pc_ = redirect_target_;
    }
}

void PipelineBackend::step_id() {
    if (redirect_pending_) {
        next_if_id_ = {};
        return;
    }

    next_if_id_ = if_id_;

    if (!if_id_.slot.valid) {
        return;
    }

    StageSlot decoded_slot = if_id_.slot;
    decoded_slot.insn.raw = decoded_slot.raw;
    decode(decoded_slot.raw, &decoded_slot.insn);

    if (has_decode_hazard(decoded_slot.insn)) {
        last_cycle_stalled_ = true;
        return;
    }

    decoded_slot.rs1v = cpu_.core().read_gpr(decoded_slot.insn.rs1);
    decoded_slot.rs2v = cpu_.core().read_gpr(decoded_slot.insn.rs2);
    next_id_ex_.slot = decoded_slot;
    next_if_id_ = {};
}

void PipelineBackend::step_if() {
    if (cpu_.core().halted() || next_if_id_.slot.valid || pending_fetch_fault_.valid) {
        return;
    }

    const AddressSpace::AccessResult fetch = cpu_.address_space().fetch32_result(bus_, fetch_pc_);
    if (!fetch.ok) {
        pending_fetch_fault_ = fetch.fault;
        pending_fetch_fault_pc_ = fetch_pc_;
        return;
    }

    next_if_id_.slot.valid = true;
    next_if_id_.slot.pc = fetch_pc_;
    next_if_id_.slot.raw = static_cast<uint32_t>(fetch.value);
    fetch_pc_ += 4;
}

bool PipelineBackend::try_service_interrupt_at_commit_boundary() {
    if (!mem_wb_.slot.valid && !pipeline_empty()) {
        return false;
    }
    if (!cpu_.trap().service_pending_interrupts()) {
        return false;
    }

    reset_stage_state();
    fetch_pc_ = cpu_.core().pc();
    return true;
}

void PipelineBackend::commit_next_state() {
    if_id_ = next_if_id_;
    id_ex_ = next_id_ex_;
    ex_mem_ = next_ex_mem_;
    mem_wb_ = next_mem_wb_;
}

BackendDebugSnapshot PipelineBackend::debug_snapshot() const {
    BackendDebugSnapshot snapshot;
    snapshot.backend_name = name();
    snapshot.pipeline.if_stage = make_stage_snapshot(if_id_.slot);
    snapshot.pipeline.id_stage = make_stage_snapshot(id_ex_.slot);
    snapshot.pipeline.ex_stage = make_stage_snapshot(ex_mem_.slot);
    snapshot.pipeline.mem_stage = make_stage_snapshot(mem_wb_.slot);
    snapshot.pipeline.wb_stage = make_stage_snapshot(mem_wb_.slot);
    snapshot.pipeline.stalled = last_cycle_stalled_;
    snapshot.pipeline.redirected = last_cycle_redirected_;
    snapshot.pipeline.redirect_target = redirect_target_;
    snapshot.pipeline.pending_fetch_fault = pending_fetch_fault_.valid;
    snapshot.pipeline.trap_flush = last_cycle_trap_flush_;
    snapshot.pipeline.committed = last_cycle_committed_;
    snapshot.pipeline.empty = pipeline_empty();
    return snapshot;
}

const char* PipelineBackend::name() const {
    return "pipeline";
}
