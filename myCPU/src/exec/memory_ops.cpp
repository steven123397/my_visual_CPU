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
            if (!cpu.address_space().load(bus, addr, 1, val)) {
                return false;
            }
            val = static_cast<uint64_t>(static_cast<int64_t>(static_cast<int8_t>(val)));
            break;
        case 1:
            if (!cpu.address_space().load(bus, addr, 2, val)) {
                return false;
            }
            val = static_cast<uint64_t>(static_cast<int64_t>(static_cast<int16_t>(val)));
            break;
        case 2:
            if (!cpu.address_space().load(bus, addr, 4, val)) {
                return false;
            }
            val = static_cast<uint64_t>(static_cast<int64_t>(static_cast<int32_t>(val)));
            break;
        case 3:
            if (!cpu.address_space().load(bus, addr, 8, val)) {
                return false;
            }
            break;
        case 4:
            if (!cpu.address_space().load(bus, addr, 1, val)) {
                return false;
            }
            val = static_cast<uint8_t>(val);
            break;
        case 5:
            if (!cpu.address_space().load(bus, addr, 2, val)) {
                return false;
            }
            val = static_cast<uint16_t>(val);
            break;
        case 6:
            if (!cpu.address_space().load(bus, addr, 4, val)) {
                return false;
            }
            val = static_cast<uint32_t>(val);
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
            return cpu.address_space().store(bus, addr, rs2v, 1);
        case 1:
            return cpu.address_space().store(bus, addr, rs2v, 2);
        case 2:
            return cpu.address_space().store(bus, addr, rs2v, 4);
        case 3:
            return cpu.address_space().store(bus, addr, rs2v, 8);
        default:
            cpu.trap().enter_exception(CAUSE_ILLEGAL_INSN, insn.raw);
            return false;
        }
    default:
        return false;
    }
}
