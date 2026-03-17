#include "decode.h"

void decode(uint32_t raw, Insn *insn) {
    insn->raw    = raw;
    insn->opcode = raw & 0x7F;
    insn->rd     = (raw >> 7)  & 0x1F;
    insn->funct3 = (raw >> 12) & 0x07;
    insn->rs1    = (raw >> 15) & 0x1F;
    insn->rs2    = (raw >> 20) & 0x1F;
    insn->funct7 = (raw >> 25) & 0x7F;

    // Immediate decoding by format
    switch (insn->opcode) {
    case 0x03: case 0x13: case 0x1B: case 0x67: case 0x73: // I-type
        insn->imm = (int64_t)(int32_t)(raw & 0xFFF00000) >> 20;
        break;
    case 0x23: // S-type
        insn->imm = (int64_t)(int32_t)(
            ((raw & 0xFE000000)) | ((raw & 0xF80) << 13)) >> 20;
        break;
    case 0x63: // B-type
        insn->imm = (int64_t)(int32_t)(
            ((raw & 0x80000000)) |
            ((raw & 0x80) << 23) |
            ((raw & 0x7E000000) >> 1) |
            ((raw & 0xF00) << 12)) >> 19;
        break;
    case 0x37: case 0x17: // U-type
        insn->imm = (int64_t)(int32_t)(raw & 0xFFFFF000);
        break;
    case 0x6F: // J-type
        insn->imm =
            ((int64_t)(int32_t)(raw & 0x80000000) >> 11) |
            (int64_t)(raw & 0x000FF000) |
            ((int64_t)(raw & 0x00100000) >> 9) |
            ((int64_t)(raw & 0x7FE00000) >> 20);
        break;
    default:
        insn->imm = 0;
    }
}
