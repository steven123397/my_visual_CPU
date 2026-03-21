#include "memory_ops.h"

#include "../cpu.h"
#include "../mem/bus.h"

namespace {

constexpr uint64_t CAUSE_ILLEGAL_INSN = 2;

void write_rd(CPU& cpu, uint8_t rd, uint64_t value) {
    cpu.core().write_gpr(rd, value);
}

}  // namespace

bool execute_memory_instruction(CPU& cpu, Bus& bus, const Insn& insn, uint64_t rs1v, uint64_t rs2v, int64_t imm) {
    const uint64_t addr = rs1v + static_cast<uint64_t>(imm);

    switch (insn.opcode) {
    case 0x03: {
        uint64_t val = 0;
        switch (insn.funct3) {
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
            cpu.trap().enter_exception(CAUSE_ILLEGAL_INSN, insn.raw);
            return false;
        }
        write_rd(cpu, insn.rd, val);
        return true;
    }
    case 0x23:
        switch (insn.funct3) {
        case 0:
            bus.store(addr, rs2v, 1);
            return true;
        case 1:
            bus.store(addr, rs2v, 2);
            return true;
        case 2:
            bus.store(addr, rs2v, 4);
            return true;
        case 3:
            bus.store(addr, rs2v, 8);
            return true;
        default:
            cpu.trap().enter_exception(CAUSE_ILLEGAL_INSN, insn.raw);
            return false;
        }
    default:
        return false;
    }
}
