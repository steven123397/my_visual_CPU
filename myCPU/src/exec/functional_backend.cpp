#include "functional_backend.h"

#include <optional>

#include "../arch/csr_file.h"
#include "../cpu.h"
#include "../isa/execution_context.h"
#include "../isa/instruction_semantics.h"
#include "../mem/bus.h"
#include "pipeline_commit_boundary.h"

extern "C" {
#include "../decode.h"
}

namespace {

constexpr uint64_t CAUSE_ILLEGAL_INSN = 2;

uint8_t instruction_size(const Insn& insn) {
    return insn.size != 0 ? insn.size : 4;
}

bool is_control_flow_raw(uint32_t raw) {
    const uint32_t opcode = raw & 0x7FU;
    return opcode == 0x63U || opcode == 0x67U || opcode == 0x6FU;
}

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

std::optional<ExecutionMemoryObservation> make_scalar_memory_observation(CPU& cpu,
                                                                         Bus& bus,
                                                                         const MemoryRequest& request,
                                                                         uint64_t pc,
                                                                         uint32_t raw,
                                                                         bool fault) {
    if (request.kind != MemoryRequest::Kind::Load && request.kind != MemoryRequest::Kind::Store) {
        return std::nullopt;
    }

    const AddressSpace::TranslateResult translated =
        cpu.address_space().translate_result(bus,
                                             request.addr,
                                             request.kind == MemoryRequest::Kind::Load ? AccessType::Load
                                                                                       : AccessType::Store,
                                             false);
    if (!translated.ok) {
        if (fault) {
            return fault_memory_observation(pc,
                                            raw,
                                            request.kind == MemoryRequest::Kind::Store,
                                            static_cast<uint64_t>(request.size));
        }
        return std::nullopt;
    }

    return ExecutionMemoryObservation{
        .valid = true,
        .pc_valid = true,
        .pc = pc,
        .raw = raw,
        .region = observed_region(bus, translated.paddr, static_cast<uint64_t>(request.size)),
        .write = request.kind == MemoryRequest::Kind::Store,
        .fault = fault,
        .paddr_valid = true,
        .paddr = translated.paddr,
        .bytes = static_cast<uint64_t>(request.size),
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
        .paddr_valid = true,
        .paddr = translated.paddr,
        .bytes = bytes,
    };
}

ExecutionTrapObservation make_trap_observation(const CPU& cpu, uint64_t pc, uint32_t raw) {
    const uint64_t cause = active_trap_cause(cpu);
    return ExecutionTrapObservation{
        .pc = pc,
        .raw = raw,
        .cause = cause,
        .privilege = cpu.core().privilege_mode(),
        .interrupt = (cause >> 63) != 0,
    };
}

InsnEffects illegal_instruction_effect(uint32_t raw) {
    InsnEffects effects;
    effects.trap.valid = true;
    effects.trap.cause = CAUSE_ILLEGAL_INSN;
    effects.trap.tval = raw;
    effects.retired = false;
    return effects;
}

InsnEffects build_effects(CPU& cpu, Bus& bus, const Insn& insn) {
    if (!InstructionSemantics::supports(insn)) {
        return illegal_instruction_effect(insn.raw);
    }

    ExecutionContext ctx(cpu, bus);
    return InstructionSemantics::execute(insn, ctx);
}

bool handle_interrupt_and_record_profile(CPU& cpu, Bus& bus, ExecutionProfile& profile) {
    const uint64_t interrupted_pc = cpu.core().pc();
    const PrivilegeMode interrupted_privilege = cpu.core().privilege_mode();
    const uint64_t interrupted_mcause = cpu.csr().read(CSR_MCAUSE, cpu.core());
    const uint64_t interrupted_scause = cpu.csr().read(CSR_SCAUSE, cpu.core());

    cpu.trap().handle_platform_events(bus.tick());

    const bool trap_taken = cpu.core().pc() != interrupted_pc ||
                            cpu.core().privilege_mode() != interrupted_privilege ||
                            cpu.csr().read(CSR_MCAUSE, cpu.core()) != interrupted_mcause ||
                            cpu.csr().read(CSR_SCAUSE, cpu.core()) != interrupted_scause;
    if (trap_taken) {
        profile.record_trap(make_trap_observation(cpu, interrupted_pc, 0));
    }
    return trap_taken;
}

}  // namespace

FunctionalBackend::FunctionalBackend(CPU& cpu, Bus& bus) : cpu_(cpu), bus_(bus) {}

void FunctionalBackend::step() {
    handle_interrupt_and_record_profile(cpu_, bus_, profile_);

    const uint64_t pc = cpu_.core().pc();
    const AddressSpace::AccessResult first_half = cpu_.address_space().fetch16_result(bus_);
    if (!first_half.ok) {
        cpu_.trap().enter_exception(first_half.fault.cause, first_half.fault.tval);
        profile_.record_trap(make_trap_observation(cpu_, pc, 0));
        cpu_.core().advance_cycle();
        return;
    }

    uint32_t raw = static_cast<uint16_t>(first_half.value);
    if ((raw & 0x3U) == 0x3U) {
        const AddressSpace::AccessResult full_word = cpu_.address_space().fetch32_result(bus_);
        if (!full_word.ok) {
            cpu_.trap().enter_exception(full_word.fault.cause, full_word.fault.tval);
            profile_.record_trap(make_trap_observation(cpu_, pc, 0));
            cpu_.core().advance_cycle();
            return;
        }
        raw = static_cast<uint32_t>(full_word.value);
    }

    Insn insn{};
    decode(raw, &insn);
    insn.raw = raw;
    const InsnEffects effects = build_effects(cpu_, bus_, insn);
    std::optional<MemoryRecord> record;
    const bool scalar = effects.mem.kind != MemoryRequest::Kind::None;
    const bool atomic = effects.atomic.kind != AtomicRequest::Kind::None;
    const bool vector_memory = effects.vector.kind == VectorRequest::Kind::Load || effects.vector.kind == VectorRequest::Kind::Store;
    if (memory_receiver_) {
        if ((scalar && effects.mem.target == MemoryRequest::Target::Float) || vector_memory) {
            memory_receiver_->unsupported();
        } else if (scalar || atomic) {
            record.emplace();
            auto& r = *record;
            r.pc = pc;
            r.raw = raw;
            r.instruction_bytes = instruction_size(insn);
            r.privilege = cpu_.core().privilege_mode();
            r.start_cycle = cpu_.core().cycle();
            r.start_instret = cpu_.core().instret();
            r.operation = scalar ? (effects.mem.kind == MemoryRequest::Kind::Load ? MemoryOperation::Load : MemoryOperation::Store)
                                 : effects.atomic.kind == AtomicRequest::Kind::LoadReserved ? MemoryOperation::LR
                                 : effects.atomic.kind == AtomicRequest::Kind::StoreConditional ? MemoryOperation::SC : MemoryOperation::AMO;
            const auto context = cpu_.address_space().inspection_context(
                r.operation == MemoryOperation::Load || r.operation == MemoryOperation::LR ? AccessType::Load : AccessType::Store);
            r.satp = context.satp;
            r.effective_privilege = context.effective_privilege;
            r.vaddr = scalar ? effects.mem.addr : effects.atomic.addr;
            r.bytes = scalar ? effects.mem.size : effects.atomic.size;
            r.atomic_kind = effects.atomic.kind;
            if (r.operation != MemoryOperation::Load && r.operation != MemoryOperation::LR)
                r.store_value = scalar ? effects.mem.store_value : effects.atomic.store_value;
            cpu_.address_space().bind_memory_record(&r);
        }
    }
    const uint64_t next_pc = pc + instruction_size(insn);
    const CommitBoundaryResult result = apply_commit_boundary(
        cpu_,
        bus_,
        CommitBoundaryInput{
            .pc = pc,
            .next_pc = next_pc,
            .effects = effects,
        });

    cpu_.address_space().bind_memory_record(nullptr);
    if (record) {
        record->retired = result.retired;
        if (result.trap_taken) {
            const bool supervisor = cpu_.core().privilege_mode() == PrivilegeMode::Supervisor;
            record->trap = {true, active_trap_cause(cpu_), cpu_.csr().read(supervisor ? CSR_STVAL : CSR_MTVAL, cpu_.core())};
        }
    }

    if (const std::optional<ExecutionMemoryObservation> memory =
            make_atomic_memory_observation(bus_, effects.atomic, pc, raw, result);
        memory.has_value()) {
        profile_.record_memory(*memory);
    } else if (const std::optional<ExecutionMemoryObservation> memory =
            make_scalar_memory_observation(cpu_, bus_, effects.mem, pc, raw, result.trap_taken);
        memory.has_value()) {
        profile_.record_memory(*memory);
    } else if (const std::optional<ExecutionMemoryObservation> memory =
                   make_vector_memory_observation(cpu_, bus_, effects.vector, pc, raw, result.trap_taken);
               memory.has_value()) {
        profile_.record_memory(*memory);
    }

    if (result.trap_taken) {
        profile_.record_trap(make_trap_observation(cpu_, pc, raw));
    }
    if (result.retired) {
        profile_.record_retire(ExecutionRetireObservation{
            .pc = pc,
            .raw = raw,
            .trap = false,
            .redirect = effects.control.redirect_pc || effects.control.trap_return != TrapReturnKind::None,
            .cycle_valid = true,
            .cycle = cpu_.core().cycle(),
            .target_pc_valid = is_control_flow_raw(raw),
            .target_pc = result.next_pc,
        });
    }

    cpu_.core().advance_cycle();
    if (record) {
        record->end_cycle = cpu_.core().cycle();
        record->end_instret = cpu_.core().instret();
        if (record->representable) memory_receiver_->receive(*record);
        else memory_receiver_->unsupported();
    }
}

const char* FunctionalBackend::name() const {
    return "functional";
}

BackendDebugSnapshot FunctionalBackend::debug_snapshot() const {
    BackendDebugSnapshot snapshot;
    snapshot.backend_name = name();
    snapshot.pipeline.empty = true;
    snapshot.profile = profile_.snapshot();
    return snapshot;
}
