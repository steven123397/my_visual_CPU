#include "control_flow_ops.h"

#include "../cpu.h"

namespace {

constexpr uint64_t CAUSE_ILLEGAL_INSN = 2;

void write_rd(CPU& cpu, uint8_t rd, uint64_t value) {
    cpu.core().write_gpr(rd, value);
}

}  // namespace

bool execute_control_flow_instruction(
    CPU& cpu,
    const Insn& insn,
    uint64_t rs1v,
    uint64_t rs2v,
    int64_t imm,
    uint64_t pc,
    uint64_t& next_pc) {
    switch (insn.opcode) {
    case 0x6F:
        write_rd(cpu, insn.rd, next_pc);
        next_pc = pc + static_cast<uint64_t>(imm);
        return true;
    case 0x67:
        write_rd(cpu, insn.rd, next_pc);
        next_pc = (rs1v + static_cast<uint64_t>(imm)) & ~1ULL;
        return true;
    case 0x63: {
        int taken = 0;
        switch (insn.funct3) {
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
            cpu.trap().enter_exception(CAUSE_ILLEGAL_INSN, insn.raw);
            return false;
        }
        if (taken) {
            next_pc = pc + static_cast<uint64_t>(imm);
        }
        return true;
    }
    default:
        return false;
    }
}
