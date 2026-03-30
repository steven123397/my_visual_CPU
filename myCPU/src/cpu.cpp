#include "cpu.h"

#include "exec/memory_ops.h"
#include "isa/execution_context.h"
#include "isa/instruction_semantics.h"
#include "mem/bus.h"

extern "C" {
#include "decode.h"
}

namespace {

constexpr uint64_t CAUSE_ILLEGAL_INSN = 2;

bool apply_instruction_effects(CPU& cpu, Bus& bus, const InsnEffects& effects, uint64_t next_pc) {
    CoreState& core = cpu.core();

    if (effects.trap.valid) {
        cpu.trap().enter_exception(effects.trap.cause, effects.trap.tval);
        return false;
    }
    if (effects.mem.kind != MemoryRequest::Kind::None) {
        if (!apply_memory_effects(cpu, bus, effects)) {
            return false;
        }
    }
    if (effects.csr_write.enable) {
        cpu.csr().write(effects.csr_write.addr, effects.csr_write.value, core);
    }
    if (effects.rd_write.enable) {
        core.write_gpr(effects.rd_write.rd, effects.rd_write.value);
    }
    if (effects.control.flush_tlb) {
        cpu.address_space().flush_tlb();
    }
    if (effects.control.halt) {
        core.set_halted(true);
    }
    switch (effects.control.trap_return) {
    case TrapReturnKind::Mret:
        cpu.trap().return_from_mret();
        break;
    case TrapReturnKind::Sret:
        cpu.trap().return_from_sret();
        break;
    case TrapReturnKind::None:
        core.set_pc(effects.control.redirect_pc ? effects.control.target_pc : next_pc);
        break;
    }
    return effects.retired;
}

bool execute(CPU& cpu, Bus& bus, Insn* in) {
    const uint64_t next_pc = cpu.core().pc() + 4;

    if (InstructionSemantics::supports(*in)) {
        ExecutionContext ctx(cpu, bus);
        return apply_instruction_effects(cpu, bus, InstructionSemantics::execute(*in, ctx), next_pc);
    }
    cpu.trap().enter_exception(CAUSE_ILLEGAL_INSN, in->raw);
    return false;
}

}  // namespace

CPU::CPU() : trap_(core_, csr_), address_space_(core_, csr_, trap_) {
    csr_.bind_address_space(&address_space_);
}

CoreState& CPU::core() {
    return core_;
}

const CoreState& CPU::core() const {
    return core_;
}

CsrFile& CPU::csr() {
    return csr_;
}

const CsrFile& CPU::csr() const {
    return csr_;
}

TrapController& CPU::trap() {
    return trap_;
}

const TrapController& CPU::trap() const {
    return trap_;
}

AddressSpace& CPU::address_space() {
    return address_space_;
}

const AddressSpace& CPU::address_space() const {
    return address_space_;
}

void cpu_init(CPU& cpu, uint64_t entry) {
    cpu.core().reset(entry);
    cpu.csr().reset();
    cpu.address_space().flush_tlb();
}

uint64_t csr_read(const CPU& cpu, uint32_t addr) {
    return cpu.csr().read(addr, cpu.core());
}

void csr_write(CPU& cpu, uint32_t addr, uint64_t val) {
    cpu.csr().write(addr, val, cpu.core());
}

void cpu_step(CPU& cpu, Bus& bus) {
    cpu.trap().handle_platform_events(bus.tick());

    uint32_t raw = 0;
    if (!cpu.address_space().fetch32(bus, raw)) {
        cpu.core().advance_cycle();
        return;
    }
    Insn insn;
    decode(raw, &insn);
    if (execute(cpu, bus, &insn)) {
        cpu.core().advance_instret();
    }

    cpu.core().advance_cycle();
}
