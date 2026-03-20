#include "cpu.h"

#include "mem/bus.h"

extern "C" {
#include "decode.h"
}

namespace {

constexpr uint64_t CAUSE_ILLEGAL_INSN = 2;
constexpr uint64_t CAUSE_BREAKPOINT = 3;
constexpr uint64_t CAUSE_ECALL_M = 11;

void set_rd(CPU& cpu, uint8_t rd, uint64_t value) {
    cpu.core().write_gpr(rd, value);
}

void execute(CPU& cpu, Bus& bus, Insn* in) {
    CoreState& core = cpu.core();
    const uint64_t pc = core.pc();
    const uint64_t rs1v = core.read_gpr(in->rs1);
    const uint64_t rs2v = core.read_gpr(in->rs2);
    const int64_t imm = in->imm;
    uint64_t next_pc = pc + 4;

    switch (in->opcode) {
    case 0x37:
        set_rd(cpu, in->rd, static_cast<uint64_t>(imm));
        break;
    case 0x17:
        set_rd(cpu, in->rd, pc + static_cast<uint64_t>(imm));
        break;
    case 0x6F:
        set_rd(cpu, in->rd, next_pc);
        next_pc = pc + static_cast<uint64_t>(imm);
        break;
    case 0x67:
        set_rd(cpu, in->rd, next_pc);
        next_pc = (rs1v + static_cast<uint64_t>(imm)) & ~1ULL;
        break;
    case 0x63: {
        int taken = 0;
        switch (in->funct3) {
        case 0:
            taken = rs1v == rs2v;
            break;
        case 1:
            taken = rs1v != rs2v;
            break;
        case 4:
            taken = static_cast<int64_t>(rs1v) < static_cast<int64_t>(rs2v);
            break;
        case 5:
            taken = static_cast<int64_t>(rs1v) >= static_cast<int64_t>(rs2v);
            break;
        case 6:
            taken = rs1v < rs2v;
            break;
        case 7:
            taken = rs1v >= rs2v;
            break;
        default:
            cpu.trap().enter_exception(CAUSE_ILLEGAL_INSN, in->raw);
            return;
        }
        if (taken) {
            next_pc = pc + static_cast<uint64_t>(imm);
        }
        break;
    }
    case 0x03: {
        const uint64_t addr = rs1v + static_cast<uint64_t>(imm);
        uint64_t val = 0;
        switch (in->funct3) {
        case 0:
            val = static_cast<uint64_t>(static_cast<int64_t>(static_cast<int8_t>(bus.load(addr, 1))));
            break;
        case 1:
            val = static_cast<uint64_t>(static_cast<int64_t>(static_cast<int16_t>(bus.load(addr, 2))));
            break;
        case 2:
            val = static_cast<uint64_t>(static_cast<int64_t>(static_cast<int32_t>(bus.load(addr, 4))));
            break;
        case 3:
            val = bus.load(addr, 8);
            break;
        case 4:
            val = static_cast<uint8_t>(bus.load(addr, 1));
            break;
        case 5:
            val = static_cast<uint16_t>(bus.load(addr, 2));
            break;
        case 6:
            val = static_cast<uint32_t>(bus.load(addr, 4));
            break;
        default:
            cpu.trap().enter_exception(CAUSE_ILLEGAL_INSN, in->raw);
            return;
        }
        set_rd(cpu, in->rd, val);
        break;
    }
    case 0x23: {
        const uint64_t addr = rs1v + static_cast<uint64_t>(imm);
        switch (in->funct3) {
        case 0:
            bus.store(addr, rs2v, 1);
            break;
        case 1:
            bus.store(addr, rs2v, 2);
            break;
        case 2:
            bus.store(addr, rs2v, 4);
            break;
        case 3:
            bus.store(addr, rs2v, 8);
            break;
        default:
            cpu.trap().enter_exception(CAUSE_ILLEGAL_INSN, in->raw);
            return;
        }
        break;
    }
    case 0x13: {
        uint64_t val = 0;
        const uint8_t shamt = static_cast<uint8_t>(in->rs2 | ((in->funct7 & 1U) << 5));
        switch (in->funct3) {
        case 0:
            val = rs1v + static_cast<uint64_t>(imm);
            break;
        case 1:
            val = rs1v << shamt;
            break;
        case 2:
            val = static_cast<int64_t>(rs1v) < imm;
            break;
        case 3:
            val = rs1v < static_cast<uint64_t>(imm);
            break;
        case 4:
            val = rs1v ^ static_cast<uint64_t>(imm);
            break;
        case 5:
            val = (in->funct7 & 0x20U) ? static_cast<uint64_t>(static_cast<int64_t>(rs1v) >> shamt) : (rs1v >> shamt);
            break;
        case 6:
            val = rs1v | static_cast<uint64_t>(imm);
            break;
        case 7:
            val = rs1v & static_cast<uint64_t>(imm);
            break;
        default:
            cpu.trap().enter_exception(CAUSE_ILLEGAL_INSN, in->raw);
            return;
        }
        set_rd(cpu, in->rd, val);
        break;
    }
    case 0x1B: {
        uint64_t val = 0;
        const uint8_t shamt = in->rs2;
        switch (in->funct3) {
        case 0:
            val = static_cast<uint64_t>(static_cast<int64_t>(static_cast<int32_t>(rs1v + static_cast<uint64_t>(imm))));
            break;
        case 1:
            val = static_cast<uint64_t>(static_cast<int64_t>(static_cast<int32_t>(static_cast<uint32_t>(rs1v) << shamt)));
            break;
        case 5:
            val = (in->funct7 & 0x20U)
                ? static_cast<uint64_t>(static_cast<int64_t>(static_cast<int32_t>(static_cast<int32_t>(rs1v) >> shamt)))
                : static_cast<uint64_t>(static_cast<int64_t>(static_cast<int32_t>(static_cast<uint32_t>(rs1v) >> shamt)));
            break;
        default:
            cpu.trap().enter_exception(CAUSE_ILLEGAL_INSN, in->raw);
            return;
        }
        set_rd(cpu, in->rd, val);
        break;
    }
    case 0x33: {
        uint64_t val = 0;
        const uint8_t shamt = static_cast<uint8_t>(rs2v & 63U);
        if (in->funct7 == 1) {
            switch (in->funct3) {
            case 0:
                val = rs1v * rs2v;
                break;
            case 1:
                val = static_cast<uint64_t>((static_cast<__int128>(static_cast<int64_t>(rs1v)) * static_cast<int64_t>(rs2v)) >> 64);
                break;
            case 2:
                val = static_cast<uint64_t>((static_cast<__int128>(static_cast<int64_t>(rs1v)) * rs2v) >> 64);
                break;
            case 3:
                val = static_cast<uint64_t>((static_cast<__uint128_t>(rs1v) * rs2v) >> 64);
                break;
            case 4:
                val = rs2v ? static_cast<uint64_t>(static_cast<int64_t>(rs1v) / static_cast<int64_t>(rs2v)) : ~0ULL;
                break;
            case 5:
                val = rs2v ? rs1v / rs2v : ~0ULL;
                break;
            case 6:
                val = rs2v ? static_cast<uint64_t>(static_cast<int64_t>(rs1v) % static_cast<int64_t>(rs2v)) : rs1v;
                break;
            case 7:
                val = rs2v ? rs1v % rs2v : rs1v;
                break;
            default:
                cpu.trap().enter_exception(CAUSE_ILLEGAL_INSN, in->raw);
                return;
            }
        } else {
            switch (in->funct3) {
            case 0:
                val = (in->funct7 & 0x20U) ? (rs1v - rs2v) : (rs1v + rs2v);
                break;
            case 1:
                val = rs1v << shamt;
                break;
            case 2:
                val = static_cast<int64_t>(rs1v) < static_cast<int64_t>(rs2v);
                break;
            case 3:
                val = rs1v < rs2v;
                break;
            case 4:
                val = rs1v ^ rs2v;
                break;
            case 5:
                val = (in->funct7 & 0x20U) ? static_cast<uint64_t>(static_cast<int64_t>(rs1v) >> shamt) : (rs1v >> shamt);
                break;
            case 6:
                val = rs1v | rs2v;
                break;
            case 7:
                val = rs1v & rs2v;
                break;
            default:
                cpu.trap().enter_exception(CAUSE_ILLEGAL_INSN, in->raw);
                return;
            }
        }
        set_rd(cpu, in->rd, val);
        break;
    }
    case 0x3B: {
        uint64_t val = 0;
        const uint8_t shamt = static_cast<uint8_t>(rs2v & 31U);
        if (in->funct7 == 1) {
            switch (in->funct3) {
            case 0:
                val = static_cast<uint64_t>(static_cast<int64_t>(static_cast<int32_t>(rs1v * rs2v)));
                break;
            case 4:
                val = rs2v ? static_cast<uint64_t>(static_cast<int64_t>(static_cast<int32_t>(rs1v) / static_cast<int32_t>(rs2v))) : UINT64_MAX;
                break;
            case 5:
                val = rs2v ? static_cast<uint64_t>(static_cast<int64_t>(static_cast<int32_t>(static_cast<uint32_t>(rs1v) / static_cast<uint32_t>(rs2v)))) : UINT64_MAX;
                break;
            case 6:
                val = rs2v ? static_cast<uint64_t>(static_cast<int64_t>(static_cast<int32_t>(rs1v) % static_cast<int32_t>(rs2v))) : static_cast<uint64_t>(static_cast<int64_t>(static_cast<int32_t>(rs1v)));
                break;
            case 7:
                val = rs2v ? static_cast<uint64_t>(static_cast<int64_t>(static_cast<int32_t>(static_cast<uint32_t>(rs1v) % static_cast<uint32_t>(rs2v)))) : static_cast<uint64_t>(static_cast<int64_t>(static_cast<int32_t>(rs1v)));
                break;
            default:
                cpu.trap().enter_exception(CAUSE_ILLEGAL_INSN, in->raw);
                return;
            }
        } else {
            switch (in->funct3) {
            case 0:
                val = static_cast<uint64_t>(static_cast<int64_t>(static_cast<int32_t>((in->funct7 & 0x20U) ? (rs1v - rs2v) : (rs1v + rs2v))));
                break;
            case 1:
                val = static_cast<uint64_t>(static_cast<int64_t>(static_cast<int32_t>(static_cast<uint32_t>(rs1v) << shamt)));
                break;
            case 5:
                val = (in->funct7 & 0x20U)
                    ? static_cast<uint64_t>(static_cast<int64_t>(static_cast<int32_t>(static_cast<int32_t>(rs1v) >> shamt)))
                    : static_cast<uint64_t>(static_cast<int64_t>(static_cast<int32_t>(static_cast<uint32_t>(rs1v) >> shamt)));
                break;
            default:
                cpu.trap().enter_exception(CAUSE_ILLEGAL_INSN, in->raw);
                return;
            }
        }
        set_rd(cpu, in->rd, val);
        break;
    }
    case 0x73: {
        const uint32_t csr_addr = in->raw >> 20;
        uint64_t old = 0;
        switch (in->funct3) {
        case 0:
            if (in->raw == 0x00000073) {
                if (core.read_gpr(17) == 93) {
                    core.set_halted(true);
                    return;
                }
                cpu.trap().enter_exception(CAUSE_ECALL_M, 0);
                return;
            }
            if (in->raw == 0x00100073) {
                cpu.trap().enter_exception(CAUSE_BREAKPOINT, pc);
                return;
            }
            if (in->raw == 0x30200073) {
                cpu.trap().return_from_mret();
                return;
            }
            break;
        case 1:
            old = csr_read(cpu, csr_addr);
            set_rd(cpu, in->rd, old);
            csr_write(cpu, csr_addr, rs1v);
            break;
        case 2:
            old = csr_read(cpu, csr_addr);
            set_rd(cpu, in->rd, old);
            csr_write(cpu, csr_addr, old | rs1v);
            break;
        case 3:
            old = csr_read(cpu, csr_addr);
            set_rd(cpu, in->rd, old);
            csr_write(cpu, csr_addr, old & ~rs1v);
            break;
        case 5:
            old = csr_read(cpu, csr_addr);
            set_rd(cpu, in->rd, old);
            csr_write(cpu, csr_addr, in->rs1);
            break;
        case 6:
            old = csr_read(cpu, csr_addr);
            set_rd(cpu, in->rd, old);
            csr_write(cpu, csr_addr, old | in->rs1);
            break;
        case 7:
            old = csr_read(cpu, csr_addr);
            set_rd(cpu, in->rd, old);
            csr_write(cpu, csr_addr, old & ~static_cast<uint64_t>(in->rs1));
            break;
        default:
            cpu.trap().enter_exception(CAUSE_ILLEGAL_INSN, in->raw);
            return;
        }
        break;
    }
    case 0x0F:
        break;
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
