#include "cpu.h"

#include "exec/control_flow_ops.h"
#include "exec/integer_ops.h"
#include "exec/memory_ops.h"
#include "exec/system_ops.h"
#include "mem/bus.h"

extern "C" {
#include "decode.h"
}

namespace {

constexpr uint64_t CAUSE_ILLEGAL_INSN = 2;

void execute(CPU& cpu, Bus& bus, Insn* in) {
    CoreState& core = cpu.core();
    const uint64_t pc = core.pc();
    const uint64_t rs1v = core.read_gpr(in->rs1);
    const uint64_t rs2v = core.read_gpr(in->rs2);
    const int64_t imm = in->imm;
    uint64_t next_pc = pc + 4;

    switch (in->opcode) {
    case 0x37:
    case 0x17:
    case 0x13:
    case 0x1B:
    case 0x33:
    case 0x3B:
    case 0x0F:
        if (!execute_integer_instruction(cpu, *in, rs1v, rs2v, imm, pc)) {
            return;
        }
        break;
    case 0x6F:
    case 0x67:
    case 0x63:
        if (!execute_control_flow_instruction(cpu, *in, rs1v, rs2v, imm, pc, next_pc)) {
            return;
        }
        break;
    case 0x03:
    case 0x23:
        if (!execute_memory_instruction(cpu, bus, *in, rs1v, rs2v, imm)) {
            return;
        }
        break;
    case 0x73: {
        if (!execute_system_instruction(cpu, *in)) {
            return;
        }
        break;
    }
    default:
        cpu.trap().enter_exception(CAUSE_ILLEGAL_INSN, in->raw);
        return;
    }

    core.set_pc(next_pc);
}

}  // namespace

CPU::CPU() : trap_(core_, csr_) {}

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

void cpu_init(CPU& cpu, uint64_t entry) {
    cpu.core().reset(entry);
    cpu.csr().reset();
    cpu.csr().write(CSR_MISA, (2ULL << 62) | (1ULL << 8) | (1ULL << 12) | (1ULL << 0) | (1ULL << 2));
}

uint64_t csr_read(const CPU& cpu, uint32_t addr) {
    return cpu.csr().read(addr, cpu.core());
}

void csr_write(CPU& cpu, uint32_t addr, uint64_t val) {
    cpu.csr().write(addr, val);
}

void cpu_step(CPU& cpu, Bus& bus) {
    cpu.trap().handle_platform_events(bus.tick());

    const uint32_t raw = static_cast<uint32_t>(bus.load(cpu.core().pc(), 4));
    Insn insn;
    decode(raw, &insn);
    execute(cpu, bus, &insn);

    cpu.core().advance_cycle();
}
